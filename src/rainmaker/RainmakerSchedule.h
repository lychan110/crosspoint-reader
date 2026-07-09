#pragma once

#include <cstdint>

namespace rainmaker {

// Decides the delay (in seconds) until the next scheduled sync attempt given
// the current local-time minutes-of-day and the effective start/end window.
// Inputs are minutes-of-day in [0,1439]. solarSunrise / solarSunset may be
// < 0 to indicate "no cached solar times"; in that case the fixed start/end
// is always used regardless of mode.
//
// Rules:
//   1. start = fixed, unless startMode == SOLAR and solarSunrise valid.
//   2. end   = fixed, unless endMode   == SOLAR and solarSunset valid.
//   3. If start >= end, fall back to 08:00-22:00 (480..1320).
//   4. now < start:  delay to start (today).
//   5. start <= now < end: delay by interval (>= 300s), unless that overshoots
//      end — in which case delay to tomorrow's start (avoids a wake that does
//      nothing).
//   6. now >= end: delay to tomorrow's start.
//   7. interval is clamped to at least 5 minutes.
uint32_t nextDelaySeconds(uint16_t nowLocalMinutes, uint16_t intervalMinutes, uint8_t startMode, uint8_t endMode,
                          uint16_t fixedStartMinutes, uint16_t fixedEndMinutes, int16_t solarSunrise,
                          int16_t solarSunset);

// Convert a stored "6-minute quanta" setting (0..239) to minutes-of-day.
constexpr uint16_t quantaToMinutes(uint16_t quanta) { return quanta * 6; }

// Convert a UTC hour+minute + CrossPoint quarter-hour UTC offset bias
// (clockUtcOffsetQ: 48 = UTC+0, 0 = UTC-12, 104 = UTC+14) into a local
// minutes-of-day value in [0, 1439].
uint16_t utcHmToLocalMinutes(uint8_t hour, uint8_t minute, uint8_t clockUtcOffsetQBiased);

// Fallback delay when the clock isn't available: `rainmakerIntervalMinutes`
// worth of seconds, clamped to the same minimums as the schedule logic.
constexpr uint32_t fallbackDelaySeconds(uint16_t intervalMinutes) {
  uint16_t interval = intervalMinutes < 5 ? 5 : intervalMinutes;
  return static_cast<uint32_t>(interval) * 60UL;
}

}  // namespace rainmaker
