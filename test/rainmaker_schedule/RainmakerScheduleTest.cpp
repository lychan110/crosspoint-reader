#include <gtest/gtest.h>

#include "rainmaker/RainmakerSchedule.h"

namespace {

using rainmaker::nextDelaySeconds;
using rainmaker::utcHmToLocalMinutes;
using rainmaker::fallbackDelaySeconds;
using rainmaker::quantaToMinutes;

constexpr uint8_t MODE_FIXED = 0;
constexpr uint8_t MODE_SOLAR = 1;

constexpr uint16_t hm(uint8_t h, uint8_t m) { return static_cast<uint16_t>(h) * 60 + m; }

}  // namespace

TEST(RainmakerSchedule, BeforeWindow_DelayToStart) {
  // now=07:00, fixed window 08:00-22:00, interval=60 -> 60 minutes to start.
  EXPECT_EQ(nextDelaySeconds(hm(7, 0), 60, MODE_FIXED, MODE_FIXED, hm(8, 0), hm(22, 0), -1, -1),
            static_cast<uint32_t>(3600));
}

TEST(RainmakerSchedule, InWindow_IntervalDelay) {
  // now=12:00, interval=30 -> 30 minutes.
  EXPECT_EQ(nextDelaySeconds(hm(12, 0), 30, MODE_FIXED, MODE_FIXED, hm(8, 0), hm(22, 0), -1, -1),
            static_cast<uint32_t>(1800));
}

TEST(RainmakerSchedule, AfterWindow_NextDayStart) {
  // now=23:00, window 08:00-22:00 -> (1440-1380)*60 + 480*60 = 32400.
  EXPECT_EQ(nextDelaySeconds(hm(23, 0), 60, MODE_FIXED, MODE_FIXED, hm(8, 0), hm(22, 0), -1, -1),
            static_cast<uint32_t>(32400));
}

TEST(RainmakerSchedule, IntervalCrossingEnd_NextDayStart) {
  // now=21:30, end=22:00, interval=60 -> nextMinute=1350 >= 1320, skip to tomorrow.
  // (1440-1290)*60 + 480*60 = 150*60 + 28800 = 37800.
  EXPECT_EQ(nextDelaySeconds(hm(21, 30), 60, MODE_FIXED, MODE_FIXED, hm(8, 0), hm(22, 0), -1, -1),
            static_cast<uint32_t>(37800));
}

TEST(RainmakerSchedule, SolarMode_UsesSolarBounds) {
  // startMode=1, endMode=1, solar bounds 360-1200, now=200 (before solar start).
  // (360-200)*60 = 9600.
  EXPECT_EQ(nextDelaySeconds(200, 30, MODE_SOLAR, MODE_SOLAR, hm(8, 0), hm(22, 0), 360, 1200),
            static_cast<uint32_t>(9600));
}

TEST(RainmakerSchedule, SolarMode_FixedFallback) {
  // Modes are FIXED (0), so solar values are ignored. With now=500 (08:20),
  // start=480, end=1320, interval=15 -> in window, nextMinute=515 < 1320,
  // so return 15*60 = 900.
  EXPECT_EQ(nextDelaySeconds(500, 15, MODE_FIXED, MODE_FIXED, hm(8, 0), hm(22, 0), 360, 1200),
            static_cast<uint32_t>(900));
}

TEST(RainmakerSchedule, InvalidWindow_FallsBack) {
  // fixedStart=1200, fixedEnd=600 (start >= end) -> falls back to 08:00-22:00.
  // now=1400 (after 22:00) -> (1440-1400)*60 + 480*60 = 40*60 + 28800 = 31200.
  EXPECT_EQ(nextDelaySeconds(1400, 60, MODE_FIXED, MODE_FIXED, 1200, 600, -1, -1),
            static_cast<uint32_t>(31200));
}

TEST(RainmakerSchedule, UtcHmToLocalMinutes_WrapsNegative) {
  // hour=1, minute=0, offset=16 (UTC-12: 16-48 = -32 quarters = -480 minutes).
  // totalMinutes = 60 - 480 = -420. -420 % 1440 = -420; +1440 = 1020.
  EXPECT_EQ(utcHmToLocalMinutes(1, 0, 16), 1020);
}

TEST(RainmakerSchedule, UtcHmToLocalMinutes_ForwardWrap) {
  // hour=23, minute=30, offset=104 (UTC+14: 104-48 = 56 quarters = +840 minutes).
  // totalMinutes = 1410 + 840 = 2250. 2250 % 1440 = 810.
  EXPECT_EQ(utcHmToLocalMinutes(23, 30, 104), 810);
}

TEST(RainmakerSchedule, UtcHmToLocalMinutes_NoOffset) {
  // Sanity: offset=48 (UTC+0), hour=12, minute=0 -> 720.
  EXPECT_EQ(utcHmToLocalMinutes(12, 0, 48), 720);
}

TEST(RainmakerSchedule, FallbackDelaySeconds_ClampsTo5Min) {
  // intervalMinutes=2 -> clamped to 5 -> 5*60 = 300.
  EXPECT_EQ(fallbackDelaySeconds(2), static_cast<uint32_t>(300));
  // Larger intervals pass through unchanged.
  EXPECT_EQ(fallbackDelaySeconds(60), static_cast<uint32_t>(3600));
}

TEST(RainmakerSchedule, QuantaToMinutes) {
  // quantaToMinutes(60) = 360 (06:00). 0..239 -> 0..1434.
  EXPECT_EQ(quantaToMinutes(60), 360);
  EXPECT_EQ(quantaToMinutes(0), 0);
}
