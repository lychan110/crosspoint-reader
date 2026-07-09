#include "RainmakerSchedule.h"

#include <HalClock.h>

#include <algorithm>

namespace rainmaker {

namespace {
constexpr uint16_t MIN_INTERVAL_MIN = 5;
constexpr uint16_t DEFAULT_START_MIN = 8 * 60;  // 08:00
constexpr uint16_t DEFAULT_END_MIN = 22 * 60;   // 22:00
constexpr uint32_t SECONDS_PER_DAY = 24UL * 60UL * 60UL;
}  // namespace

uint16_t utcHmToLocalMinutes(uint8_t hour, uint8_t minute, uint8_t clockUtcOffsetQBiased) {
  // The bias is quarter-hour steps from UTC-12 (0) to UTC+14 (104), with
  // 48 representing UTC+0. Convert to a total minute offset (which may be
  // negative for zones west of UTC). Then wrap into 0..1439.
  const int offsetQuarters = static_cast<int>(clockUtcOffsetQBiased) - 48;
  const int totalMinutes = static_cast<int>(hour) * 60 + static_cast<int>(minute) + offsetQuarters * 15;
  int wrapped = totalMinutes % 1440;
  if (wrapped < 0) wrapped += 1440;
  return static_cast<uint16_t>(wrapped);
}

uint32_t nextDelaySeconds(uint16_t nowLocalMinutes, uint16_t intervalMinutes, uint8_t startMode, uint8_t endMode,
                          uint16_t fixedStartMinutes, uint16_t fixedEndMinutes, int16_t solarSunrise,
                          int16_t solarSunset) {
  // Resolve effective start/end.
  int32_t start = (startMode == 1 && solarSunrise >= 0) ? solarSunrise : fixedStartMinutes;
  int32_t end = (endMode == 1 && solarSunset >= 0) ? solarSunset : fixedEndMinutes;

  // Defensive clamping.
  if (start < 0) start = 0;
  if (start > 1439) start = 1439;
  if (end < 0) end = 0;
  if (end > 1439) end = 1439;

  if (start >= end) {
    start = DEFAULT_START_MIN;
    end = DEFAULT_END_MIN;
  }

  // Clamp interval.
  uint16_t interval = intervalMinutes;
  if (interval < MIN_INTERVAL_MIN) interval = MIN_INTERVAL_MIN;
  const uint32_t intervalSec = static_cast<uint32_t>(interval) * 60UL;

  const int32_t now = nowLocalMinutes;

  if (now < start) {
    // Wait until today's start.
    return static_cast<uint32_t>(start - now) * 60UL;
  }
  if (now >= end) {
    // Wait until tomorrow's start.
    return (SECONDS_PER_DAY - static_cast<uint32_t>(now) * 60UL) + static_cast<uint32_t>(start) * 60UL;
  }
  // In window. Schedule at the next interval, but if the next interval would
  // cross `end`, skip to tomorrow's start (avoid a wake that does no sync).
  const uint32_t nextMinute = now + interval;
  if (nextMinute >= end) {
    return (SECONDS_PER_DAY - static_cast<uint32_t>(now) * 60UL) + static_cast<uint32_t>(start) * 60UL;
  }
  return intervalSec;
}

}  // namespace rainmaker
