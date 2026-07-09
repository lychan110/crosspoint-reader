# CrossPoint Firmware Fork for Rainmaker X4 Sync

This is a fork of the [CrossPoint Reader firmware](https://github.com/crosspoint-reader/crosspoint-reader) with minimal modifications that let an ESP32-based e-ink device (X4 / X3) pull a pre-rendered Rainmaker dashboard from a VPS and use it as a deterministic sleep screen - with no phone bridge.

All Rainmaker work lives on a single topic branch (`rainmaker-sync`) for painless rebases onto upstream `develop`. CrossPoint's own rendering pipeline, books, and settings UI stay untouched. Rainmaker remains the canonical renderer and publisher; the firmware only consumes `manifest.json` + `latest.bmp`.

## Intent

- Fetch `manifest.json` and `latest.bmp` over HTTPS with Basic auth
- Cache the BMP and render it as a sleep screen
- Scheduled timer wake during a configurable waking window (fixed hours only - see below)
- Manual sync from a settings entry
- Preserve every existing CrossPoint feature (books, UI, power-button wake, etc.)

## Important Limitations

### X4 Time Drift
The X4 ESP32-C3 lacks a dedicated RTC (unlike X3's DS3231). Internal RTC drifts significantly during deep sleep. **Solar-based scheduling is UNRELIABLE on X4**. Use fixed time windows only, or require NTP sync before first timer wake.

### Credential Security
Credentials are obfuscated but recoverable. Consider per-device tokens or scoped API keys if security is critical.

### OTA/Update Safety
Timer wake configuration must be disabled before OTA updates to prevent waking into incompatible firmware.

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
/.crosspoint/rainmaker_sync/state.json
/.crosspoint/rainmaker_sync/latest.bmp
/.crosspoint/rainmaker_sync/latest.tmp
```

Note: Using underscore (`rainmaker_sync`) to avoid path confusion with main cache directories.

`state.json` fields: `lastSha256`, `sequence`, `updatedAt`, `sunriseLocalMinutes`, `sunsetLocalMinutes`, `lastAttemptEpoch`, `lastSuccessEpoch`, `lastError`.

## Rainmaker Sync Settings

Appended to `CrossPointSettings` (do not insert enum values in the middle of any existing enum):

| Field | Type | Default | Notes |
|---|---|---|---|
| `rainmakerSyncEnabled` | uint8_t | 0 | Master toggle |
| `rainmakerManifestUrl` | char[160] | `""` | Full URL to `manifest.json` (HTTPS, public CA) |
| `rainmakerUsername` | char[48] | `""` | HTTPS Basic auth user |
| `rainmakerPassword` | char[80] | `""` | HTTPS Basic auth password (obfuscated on disk) |
| `rainmakerIntervalMinutes` | uint8_t | 30 | Sync interval, clamped to >=5 |
| `rainmakerStartMode` | uint8_t | 0 | FIXED=0 only (X4 has no reliable RTC for solar) |
| `rainmakerEndMode` | uint8_t | 0 | FIXED=0 only |
| `rainmakerStartMinutes` | uint16_t | 480 (08:00) | Local minutes from midnight |
| `rainmakerEndMinutes` | uint16_t | 1320 (22:00) | Local minutes from midnight |
| `rainmakerMinBatteryPercent` | uint8_t | 20 | Scheduled sync skips below this % |
| `rainmakerDefaultedSleepMode` | uint8_t | 0 | Set to 1 after first auto-default to RAINMAKER sleep screen |

First-time enable auto-defaults `sleepScreen` to `RAINMAKER` if the user hasn't chosen anything else. If they later pick a different sleep mode, respect that.

A new `RAINMAKER` value is appended to the existing sleep-screen enum. Existing values are not renumbered.

Settings UI entries: enable toggle, manifest URL, username, password, interval (5-180), start/end mode (fixed only), start/end minutes, minimum battery, and a **Sync dashboard now** manual action.

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
- `sunriseLocalMinutes` / `sunsetLocalMinutes` are optional (sentinel `-1` when absent) - used for X3 RTC devices only

## Sync Algorithm

```text
sync(mode):
  validate settings (non-empty URL, credentials)
  if scheduled and battery < minBatteryPercent: return LowBattery
  check for power button press - abort if held (reduces latency)
  connect Wi-Fi (existing CrossPoint network path)
  if connect fails after 15s timeout: return WifiFailed
  fetch manifest via HttpDownloader::fetchUrl(url, body, user, pass)
  if fetch failed: return ManifestFetchFailed
  parse and validate manifest
  if invalid: return ManifestInvalid
  if manifest.sha256 == state.lastSha256 and latest.bmp exists:
    save state, return Ok changed=false
  download BMP to /.crosspoint/rainmaker_sync/latest.tmp
  if download failed: return DownloadFailed
  verify byte count == manifest.bytes
  compute SHA-256 over tmp in 1024-byte chunks
  if hash mismatch: delete tmp, return HashMismatch
  verify BMP header with Bitmap class
  if BMP invalid: delete tmp, return FileError
  close file before rename
  rename tmp -> latest.bmp
  update state (lastSha256, sequence, updatedAt)
  save state
  disconnect WiFi
  return Ok changed=true
```

Temp files are closed before rename/remove. All failures preserve existing cache. On success, WiFi is disconnected before returning.

### WiFi Connection Strategy

Manual sync: user connects via `CrossPointWebServerActivity` (select network, connect) first. Then trigger sync.

Timer-wake sync: auto-connect to last stored network via `WIFI_STORE.getLastConnectedSsid()` if available. Max 15s connection timeout. If connection fails: log error, preserve cache, increment failure counter in state, reschedule for next wake (max retry every wake window).

### Watchdog Handling

During sync operations, feed watchdog every 500ms to prevent reset during slow network operations.

## Schedule Logic

`RainmakerSchedule::nextDelaySeconds(uint16_t nowLocalMinutes)` returns seconds until next sync:

- X4 uses only FIXED mode (solar times ignored - unreliable without RTC)
- X3 can use solar times from manifest if available

Rules:

1. Start = fixed start time
2. End = fixed end time
3. If start >= end, fallback to 08:00-22:00
4. If now < start, delay until start
5. If start <= now < end, delay by interval (don't exceed end)
6. If now >= end, delay until next day's start
7. Clamp interval to at least 5 minutes

If next interval would exceed end, schedule next day start to avoid unnecessary wake.

## Timer Wake Integration

`src/main.cpp` changes only:

- Include `esp_sleep.h` (via Arduino.h)
- Detect timer wakeup at the top of `setup()`:

```cpp
const bool rainmakerTimerWake = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
```

- On timer wake with sync enabled: run `RainmakerSyncService::sync(Scheduled)`, then `enterDeepSleep(true)` and `return`
- Before deep sleep (after WiFi teardown in `enterDeepSleep()`): call `esp_sleep_enable_timer_wakeup(delaySeconds * 1000000ULL)` if sync enabled

Power-button wake stays armed - timer wake is additive, never replaces it.

**Critical**: Timer wake must be disabled during OTA updates. Consider checking a flag in state or the OTA update path to prevent timer wake while updating.

## Sleep Screen

When `SETTINGS.sleepScreen == RAINMAKER`:

- If `/.crosspoint/rainmaker_sync/latest.bmp` exists, draw it full screen
- Otherwise fall back to dark/light/custom
- BMP render failures fall back safely

Reuse CrossPoint's existing BMP draw utility (see `SleepActivity::renderBitmapSleepScreen()` and `BmpViewerActivity`). The `Bitmap` class and `renderer.drawBitmap()` handle 1-bit BMP rendering.

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
- Xteink X3 or X4 e-reader (ESP32-C3)
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
pio run --target upload  # flash via USB
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

Note: The base profile uses `-fno-exceptions`. Do not enable exceptions.

## Build Milestones

Incremental - each milestone is independently testable:

1. **Compile-only settings** - add fields, add UI entries, build, confirm existing reader still opens books
2. **Manual manifest fetch** - parser + `sync(Manual)` with manifest only; log the result
3. **BMP download + verify** - temp file, byte/SHA-256 check, atomic replace, cache survives reboot
4. **Rainmaker sleep mode** - `RAINMAKER` enum value + draw cached BMP, confirm override back to other modes
5. **Timer wake** - `esp_sleep_enable_timer_wakeup`, test with 2-minute interval, confirm power-button wake still works
6. **Full schedule + battery guard** - fixed bounds only (X4), low-battery skip, manual sync still runs

## Test Matrix

### Happy paths

- Manual sync with valid credentials downloads `latest.bmp`
- Manual sync again skips when SHA-256 unchanged
- Sleep mode `RAINMAKER` displays the cached dashboard
- Scheduled timer wake syncs then returns to deep sleep
- Power-button wake still opens the normal CrossPoint UI
- Timer wake properly disabled during OTA update

### Failure paths

- Manifest fetch fails - state records `lastError`, cache preserved
- Manifest invalid - `ManifestInvalid`, no download
- BMP download truncates - byte-count mismatch, `DownloadFailed`, no replace
- SHA-256 mismatch - `HashMismatch`, tmp deleted, cache preserved
- Scheduled run below battery threshold - `LowBattery`, no Wi-Fi attempt
- WiFi timeout - `WifiFailed`, state records error, next wake retries
- Corrupted state file - falls back to defaults, sync runs
