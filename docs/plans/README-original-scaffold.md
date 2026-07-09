# CrossPoint Firmware Fork for Rainmaker X4 Sync

This is a fork of the [CrossPoint Reader firmware](https://github.com/crosspoint-reader/crosspoint-reader) with minimal modifications that let an ESP32-based e-ink device (X4 / X3) pull a pre-rendered Rainmaker dashboard from a VPS and use it as a deterministic sleep screen - with no phone bridge.

All Rainmaker work lives on a single topic branch (`rainmaker-sync`) for painless rebases onto upstream `develop`. CrossPoint's own rendering pipeline, books, and settings UI stay untouched. Rainmaker remains the canonical renderer and publisher; the firmware only consumes `manifest.json` + `latest.bmp`.

## Intent

- Fetch `manifest.json` and `latest.bmp` over HTTPS with Basic auth
- Cache the BMP and render it as a sleep screen
- Scheduled timer wake during a configurable waking window (fixed hours or sunrise/sunset)
- Manual sync from a settings entry
- Preserve every existing CrossPoint feature (books, UI, power-button wake, etc.)

## Repository Setup

```bash
git clone https://github.com/crosspoint-reader/crosspoint-reader.git crosspoint-rainmaker
git remote rename origin upstream
git remote add origin <your-fork-or-private-repo-url>
git fetch upstream
git checkout upstream/develop
git checkout -b rainmaker-sync
git push -u origin rainmaker-sync
```

Ongoing sync with upstream:

```bash
git fetch upstream
git rebase upstream/develop
git push --force-with-lease origin rainmaker-sync
```

Keep all Rainmaker work in this fork. Do not vendor Rainmaker into the firmware repo.

## File Layout

New code goes in a `src/rainmaker/` subdirectory so upstream merges stay surgical. Existing files get the smallest possible edits.

```
src/
├── main.cpp                          # modified: timer wake + sync init
├── CrossPointSettings.h              # modified: append settings fields
├── SettingsList.h                    # modified: register settings UI entries
├── network/HttpDownloader.h          # reused (already supports HTTPS + Basic auth)
└── rainmaker/                        # all new code lives here
    ├── RainmakerManifest.h/.cpp      # manifest struct + parser
    ├── RainmakerSyncState.h/.cpp     # runtime state load/save
    ├── RainmakerSyncService.h/.cpp   # public sync() entry point
    ├── RainmakerSchedule.h/.cpp      # next-wake delay calculator
    ├── RainmakerSleepScreen.h/.cpp   # BMP draw on sleep
    └── Sha256.h/.cpp                 # SHA-256 over downloaded file
```

## Runtime State

Stored on the device, separate from CrossPoint settings:

```text
/.crosspoint/rainmaker/state.json
/.crosspoint/rainmaker/latest.bmp
/.crosspoint/rainmaker/latest.tmp
```

`state.json` fields: `lastSha256`, `sequence`, `updatedAt`, `sunriseLocalMinutes`, `sunsetLocalMinutes`, `lastAttemptEpoch`, `lastSuccessEpoch`, `lastError`.

## Rainmaker Sync Settings

Appended to `CrossPointSettings` (do not insert enum values in the middle of any existing enum):

| Field | Type | Default | Notes |
|---|---|---|---|
| `rainmakerSyncEnabled` | uint8_t | 0 | Master toggle |
| `rainmakerManifestUrl` | char[160] | `""` | Full URL to `manifest.json` |
| `rainmakerUsername` | char[48] | `""` | HTTPS Basic auth user |
| `rainmakerPassword` | char[80] | `""` | HTTPS Basic auth password |
| `rainmakerIntervalMinutes` | uint8_t | 30 | Sync interval, clamped to >=5 |
| `rainmakerStartMode` | uint8_t | 0 | `RAINMAKER_BOUND_FIXED=0`, `RAINMAKER_BOUND_SOLAR=1` |
| `rainmakerEndMode` | uint8_t | 0 | `RAINMAKER_BOUND_FIXED=0`, `RAINMAKER_BOUND_SOLAR=1` |
| `rainmakerStartMinutes` | uint16_t | 480 (08:00) | Local minutes from midnight |
| `rainmakerEndMinutes` | uint16_t | 1320 (22:00) | Local minutes from midnight |
| `rainmakerMinBatteryPercent` | uint8_t | 20 | Scheduled sync skips below this % |
| `rainmakerDefaultedSleepMode` | uint8_t | 0 | Set to 1 after first auto-default to RAINMAKER sleep screen |

First-time enable auto-defaults `sleepScreen` to `RAINMAKER` if the user hasn't chosen anything else. If they later pick a different sleep mode, respect that.

A new `RAINMAKER` value is appended to the existing sleep-screen enum. Existing values are not renumbered.

Settings UI entries: enable toggle, manifest URL, username, password, interval (5-180), start/end mode, start/end minutes, minimum battery, and a **Sync dashboard now** manual action (or a home-menu entry if the settings framework can't host an action row).

## Manifest Schema

The firmware expects `manifest.json` with this shape:

```json
{
  "version": 1,
  "id": "rainmaker-x4-dashboard",
  "updatedAt": "2026-07-08T08:00:00Z",
  "sequence": 123,
  "sha256": "hex-encoded-sha256-of-bmp",
  "bytes": 48062,
  "width": 480,
  "height": 800,
  "format": "1bit",
  "contentType": "image/bmp",
  "bmpUrl": "https://example.com/x4/latest.bmp",
  "sunriseLocalMinutes": 356,
  "sunsetLocalMinutes": 1234
}
```

### Validation rules

- `version` must equal `1`
- `sha256`, `bytes`, `width`, `height`, `bmpUrl` are required
- For X4, `width === 480` and `height === 800`
- `sha256` is **lowercase hex**, exactly 64 characters
- `bmpUrl` is used as-is - no string concatenation
- Downloaded BMP is verified against `sha256` and `bytes` **before** replacing the cache
- `sunriseLocalMinutes` / `sunsetLocalMinutes` are optional (sentinel `-1` when absent)

## Sync Algorithm

```text
sync(mode):
  validate settings
  if scheduled and battery < minBatteryPercent: return LowBattery
  connect Wi-Fi (existing CrossPoint network path)
  fetch manifest via HttpDownloader::fetchUrl(url, body, user, pass)
  parse and validate manifest
  cache solar times if present
  if manifest.sha256 == state.lastSha256 and latest.bmp exists:
    persist state, return Ok changed=false
  download BMP to /.crosspoint/rainmaker/latest.tmp
  verify byte count == manifest.bytes
  compute SHA-256 over tmp
  compare to manifest.sha256 (case-insensitive)
  rename tmp -> latest.bmp
  update state (lastSha256, sequence, updatedAt, solar times)
  persist state
  return Ok changed=true
```

Temp files are always closed before rename/remove. Failures leave the existing cache intact.

### WiFi Connection Strategy

Re-use CrossPoint's existing Wi-Fi infrastructure:

- Manual sync: user connects via `CrossPointWebServerActivity` (select network, connect) first. Then trigger sync.
- Timer-wake sync: auto-connect to last stored network via `WIFI_STORE.getLastConnectedSsid()` if available (STA mode)
- If connection fails: log error, preserve cache, reschedule for next wake

The firmware's `WifiSelectionActivity` auto-connects to the last used network on entry when `allowAutoConnect=true`. Timer-wake sync should follow the same pattern.

## Schedule Logic

`RainmakerSchedule::nextDelaySeconds(uint16_t nowLocalMinutes)` returns the seconds until the next sync attempt, given:

- `rainmakerIntervalMinutes`, `rainmakerStartMode`, `rainmakerEndMode`
- `rainmakerStartMinutes`, `rainmakerEndMinutes`
- Cached `sunriseLocalMinutes` / `sunsetLocalMinutes`
- Current local minutes from the CrossPoint clock/timezone

Rules:

1. Start = fixed value unless start mode is `SOLAR` and cached sunrise is valid
2. End = fixed value unless end mode is `SOLAR` and cached sunset is valid
3. If `start >= end`, fall back to 08:00-22:00
4. If `now < start`, delay until start
5. If `start <= now < end`, delay by interval (don't punch past end)
6. If `now >= end`, delay until tomorrow's start
7. Clamp interval to at least 5 minutes

If the next interval would cross the end boundary, prefer tomorrow's start over a wake at end that does nothing.

## Timer Wake Integration

`src/main.cpp` changes only:

- Include `esp_sleep.h` (via Arduino.h)
- Detect timer wakeup at the top of `setup()`:

```cpp
const bool rainmakerTimerWake = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
```

- On timer wake with sync enabled: run `RainmakerSyncService::sync(Scheduled)`, then `enterDeepSleep(true)` and `return`
- Before deep sleep (after WiFi teardown): call `esp_sleep_enable_timer_wakeup(delaySeconds * 1000000ULL)` if sync enabled

Power-button wake stays armed - timer wake is additive, never replaces it.

**Important**: Call `esp_sleep_enable_timer_wakeup()` before `enterDeepSleep()` returns, but after WiFi teardown in the existing code. The existing `enterDeepSleep()` calls `powerManager.startDeepSleep(gpio)` which handles GPIO wake setup.

## Sleep Screen

When `SETTINGS.sleepScreen == RAINMAKER`:

- If `/.crosspoint/rainmaker/latest.bmp` exists, draw it full screen
- Otherwise fall back to dark/light/custom or a status message
- The Rainmaker BMP never lands in `/.sleep` or any random-wallpaper directory
- BMP render failures fall back safely

Reuse CrossPoint's existing BMP draw utility (see `SleepActivity::renderBitmapSleepScreen()` and `BmpViewerActivity`). The `Bitmap` class and `renderer.drawBitmap()` handle 1-bit BMP rendering. For grayscale support, use `renderer.displayGrayscaleBase()` and `renderer.copyGrayscaleLsbBuffers()`/`renderer.copyGrayscaleMsbBuffers()` patterns already in `SleepActivity`.

## Manual Sync UI

One entry: **Sync Rainmaker dashboard**.

Flow:

1. Connecting Wi-Fi...
2. Fetching manifest...
3. Downloading dashboard... (only if changed)
4. Result: `Dashboard updated` / `Dashboard already current` / specific failure
5. Return to the previous screen

Manual sync never sleeps the device automatically.

## Build Instructions

### Prerequisites

- PlatformIO Core or PlatformIO IDE
- Xteink X3 or X4 e-reader (ESP32-C3, not ESP32-S3)
- USB cable for flashing OR SD card for SD-flashing method

### Flash Methods (per crosspointreader.com#flash-tools)

1. **Web flasher**: Chrome/Edge on desktop, device at home screen
2. **SD card (recommended)**: Copy `update.bin` to SD root (no extension), hold power+up at boot
3. **International locked devices**: SD flashing required (USB flashing disabled)

After flashing: Press Reset, then hold power 3-5s to start.

### Build

```bash
git clone <repo-url>
cd crosspoint-rainmaker
git checkout rainmaker-sync
pio run                  # build
pio run --target upload  # flash via USB (may be locked on some devices)
pio device monitor       # serial log
```

The built `update.bin` is flashed to the OTA partition, supporting fail-safe dual-bank OTA.

### `platformio.ini` (X4 env, extends base profile)

```ini
[env:r4sync]
extends = base
build_flags =
    ${base.build_flags}
    -DFREEINK_DEVICE_X4=1
    -DENABLE_RAINMAKER_SYNC
```

Note: The base profile already enables `-fno-exceptions`. Do not add `-fexceptions` or `-fpermissive` as these increase binary size and may cause stack overflow on constrained ESP32-C3.

## Build Milestones

Incremental - each milestone is independently testable:

1. **Compile-only settings** - add fields, add UI entries, build, confirm existing reader still opens books
2. **Manual manifest fetch** - parser + `sync(Manual)` with manifest only; log the result
3. **BMP download + verify** - temp file, byte/SHA-256 check, atomic replace, cache survives reboot
4. **Rainmaker sleep mode** - `RAINMAKER` enum value + draw cached BMP, confirm override back to other modes
5. **Timer wake** - `esp_sleep_enable_timer_wakeup`, test with a temporary 2-minute interval, confirm power-button wake still works
6. **Full schedule + solar + battery guard** - fixed/solar bounds, cached solar times, low-battery skip, manual sync still runs

## Test Matrix

### Happy paths

- Manual sync with valid credentials downloads `latest.bmp`
- Manual sync again skips when SHA-256 unchanged
- Sleep mode `RAINMAKER` displays the cached dashboard
- Scheduled timer wake syncs then returns to deep sleep
- Power-button wake still opens the normal CrossPoint UI

### Failure paths

- Manifest fetch fails - state records `lastError`, cache preserved
- Manifest invalid (bad version, missing field, bad dimensions, non-hex SHA-256) - `ManifestInvalid`, no download
- BMP download truncates - byte-count mismatch, `DownloadFailed`, no replace
- SHA-256 mismatch - `HashMismatch`, tmp deleted, cache preserved
- Scheduled run below battery threshold - `LowBattery`, no Wi-Fi attempt
