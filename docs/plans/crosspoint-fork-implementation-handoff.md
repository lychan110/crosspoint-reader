# CrossPoint Fork Implementation Handoff: Rainmaker Dashboard Sync

> **Authoritative spec.** The friendly summary lives in [`/README.md`](../../README.md). If they ever disagree, this handoff wins. The historical scaffold (now superseded) is at [`README-original-scaffold.md`](./README-original-scaffold.md).

## Purpose

Implement a minimal, rebase-friendly CrossPoint Reader firmware fork that lets an Xteink X4 fetch a pre-rendered Rainmaker dashboard directly from a VPS and use it as a deterministic sleep screen.

The CrossPoint fork must not render widgets. Rainmaker remains the canonical renderer and publisher. The firmware only consumes:

- `manifest.json`
- `latest.bmp`

## Important Limitations

### X4 Time Drift
The X4 ESP32-C3 lacks a dedicated RTC (unlike X3's DS3231). Internal RTC drifts significantly during deep sleep. **Solar-based scheduling is UNRELIABLE on X4**. Use fixed time windows only, or require NTP sync before first timer wake.

### Credential Security
Credentials are obfuscated but recoverable. Consider per-device tokens or scoped API keys if security is critical.

### OTA/Update Safety
Timer wake configuration must be disabled before OTA updates to prevent waking into incompatible firmware.

## Repository Strategy

Use a separate CrossPoint firmware repository.

Preferred setup:

```bash
git clone https://github.com/crosspoint-reader/crosspoint-reader.git crosspoint-rainmaker
git remote rename origin upstream
git remote add origin <your-fork-or-private-repo-url>
git checkout develop
git checkout -b rainmaker-sync
git push -u origin rainmaker-sync
```

Ongoing updates:

```bash
git fetch upstream
git rebase upstream/develop
git push --force-with-lease origin rainmaker-sync
```

Keep Rainmaker work out of CrossPoint except firmware client logic. Do not vendor Rainmaker into the firmware repo.

### Build Configuration for X4

The X4 uses ESP32-C3, not ESP32-S3. Target `esp32-c3-devkitm-1` in `platformio.ini`. Builds produce `update.bin` for OTA partition flashing.

```ini
[env:r4sync]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.37/platform-espressif32.zip
board = esp32-c3-devkitm-1
framework = arduino
build_flags =
    -DFREEINK_DEVICE_X4=1
    -DENABLE_RAINMAKER_SYNC
```

Flash methods (per [crosspointreader.com](https://crosspointreader.com/#flash-tools)):
1. **Web flasher**: Chrome/Edge desktop, requires device at home screen
2. **SD card**: Copy `update.bin` to SD root, hold power+up buttons at boot (recommended for all X3/X4 devices)
3. **International devices**: SD flashing recommended (USB flashing may be locked)

After flashing, press Reset then hold power button 3-5 seconds to start the device.

## Rainmaker Endpoint Contract

The VPS publishes static artifacts behind HTTPS Basic auth:

```text
GET /manifest.json
GET /latest.bmp
```

Both require the same Basic auth credentials. HTTPS certs must be from a public CA (firmware uses `esp_crt_bundle_attach`).

Expected manifest schema:

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

Firmware requirements:

- Accept `version === 1` only.
- Require `sha256`, `bytes`, `width`, `height`, `bmpUrl`.
- Require `width === 480`, `height === 800` for X4.
- `sunriseLocalMinutes`/`sunsetLocalMinutes` are optional - used for X3 RTC devices only.
- Use `bmpUrl` from the manifest, not string concatenation.
- Verify downloaded BMP SHA-256 and byte count before replacing cache.

## CrossPoint Source Areas Already Identified

From upstream CrossPoint `develop`:

- `src/main.cpp` - owns setup, loop, deep sleep, wake routing, `enterDeepSleep()`. Already tears down Wi-Fi before sleep.
- `src/CrossPointSettings.h` - stores persistent settings. Append fields; do not insert enum values in the middle.
- `src/SettingsList.h` - registers settings shown in device/web settings UI. Use `SettingInfo::DynamicString` with ObfuscationUtils for password.
- `src/network/HttpDownloader.h` - has `fetchUrl` and `downloadToFile` with HTTPS + Basic auth support.
- `src/activities/network/WifiSelectionActivity.h` - auto-connects to last network via `WIFI_STORE.getLastConnectedSsid()`.

## New Files to Add

Keep most Rainmaker code in new files under `src/rainmaker/`:

```text
src/rainmaker/RainmakerManifest.h
src/rainmaker/RainmakerManifest.cpp
src/rainmaker/RainmakerSyncState.h
src/rainmaker/RainmakerSyncState.cpp
src/rainmaker/RainmakerSyncService.h
src/rainmaker/RainmakerSyncService.cpp
src/rainmaker/RainmakerSchedule.h
src/rainmaker/RainmakerSchedule.cpp
src/rainmaker/RainmakerSleepScreen.h
src/rainmaker/RainmakerSleepScreen.cpp
src/rainmaker/Sha256.h
src/rainmaker/Sha256.cpp
```

## Settings to Add

Append to `CrossPointSettings`:

```cpp
uint8_t rainmakerSyncEnabled = 0;
char rainmakerManifestUrl[160] = "";
char rainmakerUsername[48] = "";
char rainmakerPassword[80] = "";
uint8_t rainmakerIntervalMinutes = 30;
uint8_t rainmakerStartMode = 0; // 0=fixed only (X4 has no reliable RTC)
uint8_t rainmakerEndMode = 0;   // 0=fixed only
uint16_t rainmakerStartMinutes = 480;
uint16_t rainmakerEndMinutes = 1320;
uint8_t rainmakerMinBatteryPercent = 20;
uint8_t rainmakerDefaultedSleepMode = 0;
uint32_t rainmakerLastAttemptEpoch = 0;
uint8_t rainmakerConsecutiveFailures = 0;
```

Add enum constants:

```cpp
enum RAINMAKER_BOUND_MODE {
  RAINMAKER_BOUND_FIXED = 0,
};
```

Append sleep-screen enum value at the end only:

```cpp
RAINMAKER = <next value>
```

Do not renumber existing sleep-screen values.

When enabling sync for the first time:

- if `rainmakerDefaultedSleepMode == 0`, set `sleepScreen = RAINMAKER` and `rainmakerDefaultedSleepMode = 1`.
- if the user later changes sleep screen away from Rainmaker, respect it.

## Settings UI Entries

Add to `SettingsList.h` under System category or new category:

- Enable Rainmaker Sync: toggle
- Manifest URL: string (max 160 chars)
- Username: string (max 48 chars)
- Password: string (max 80 chars, obfuscated via ObfuscationUtils)
- Sync interval minutes: value, range 5-180 (clamp min 5)
- Start mode: enum Fixed only (X4: no reliable RTC for solar)
- End mode: enum Fixed only
- Start time minutes: value 0-1439
- End time minutes: value 0-1439
- Minimum battery percent: value 0-100
- Manual action: `Sync dashboard now` (SettingAction::RainmakerSync)

## Runtime State File

Store sync runtime state separately from settings:

```text
/.crosspoint/rainmaker_sync/state.json
/.crosspoint/rainmaker_sync/latest.bmp
/.crosspoint/rainmaker_sync/latest.tmp
```

State fields:

```json
{
  "lastSha256": "hex",
  "sequence": 123,
  "updatedAt": "2026-07-08T08:00:00Z",
  "sunriseLocalMinutes": -1,
  "sunsetLocalMinutes": -1,
  "lastAttemptEpoch": 0,
  "lastSuccessEpoch": 0,
  "lastError": "",
  "consecutiveFailures": 0
}
```

Storage implementation: Use `Storage.openFileForRead/Write` via HalStorage. Keep writes atomic: close file before rename. Handle corrupted state by falling back to defaults.

## Manifest Parser

`RainmakerManifest` struct:

```cpp
struct RainmakerManifest {
  uint8_t version = 0;
  char id[64] = "";
  char updatedAt[32] = "";
  uint32_t sequence = 0;
  char sha256[65] = "";
  uint32_t bytes = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  char format[8] = "";
  char contentType[16] = "";
  char bmpUrl[192] = "";
  int16_t sunriseLocalMinutes = -1;
  int16_t sunsetLocalMinutes = -1;
};
```

Parser behavior:

- Use ArduinoJson (already in project) or manual extraction for top-level fields.
- Reject missing required fields.
- Reject invalid dimensions (must be 480x800 for X4).
- Reject non-hex SHA-256 or wrong length.
- Bounds-check all strings before copying.

## Sync Service API

`RainmakerSyncService.h`:

```cpp
enum class RainmakerSyncMode : uint8_t {
  Scheduled,
  Manual,
};

enum class RainmakerSyncStatus : uint8_t {
  Ok,
  Disabled,
  MissingConfig,
  LowBattery,
  WifiFailed,
  ManifestFetchFailed,
  ManifestInvalid,
  DownloadFailed,
  HashMismatch,
  FileError,
};

struct RainmakerSyncResult {
  RainmakerSyncStatus status;
  bool changed;
  char message[96];
};

class RainmakerSyncService {
 public:
  static RainmakerSyncResult sync(RainmakerSyncMode mode);
  static bool hasCachedDashboard();
  static const char* cachedDashboardPath();
  static void disableTimerWakeForOta(); // Call before OTA update
};
```

Scheduled mode:

- skip if disabled
- skip if missing URL or credentials
- skip if battery below threshold
- skip if too many consecutive failures (e.g., >3, indicate user intervention needed)
- run without forcing UI redraw
- preserve old cache on failure

Manual mode:

- ignore low-battery guard (show warning if < threshold)
- show progress/status
- return to UI on success/failure

## Sync Algorithm

```text
sync(mode):
  check power button - abort early if user wants control
  validate settings (non-empty URL, credentials)
  if scheduled and battery < minBatteryPercent: return LowBattery
  connect Wi-Fi (max 15s timeout)
  if WiFi fails: return WifiFailed, increment failure counter
  fetch manifest via HttpDownloader::fetchUrl(url, body, username, password)
  if fetch failed: return ManifestFetchFailed
  parse and validate manifest
  if invalid: return ManifestInvalid
  if manifest.sha256 == state.lastSha256 and latest.bmp exists:
    reset failure counter, save state, return Ok changed=false
  download BMP to /.crosspoint/rainmaker_sync/latest.tmp
  if download failed: return DownloadFailed
  verify byte count matches manifest.bytes
  compute SHA-256 over tmp in 1024-byte chunks
  if hash mismatch: delete tmp, return HashMismatch
  verify BMP header with Bitmap class
  if BMP invalid: delete tmp, return FileError
  close file before rename
  rename tmp -> latest.bmp (atomic replace)
  update state (lastSha256, sequence, updatedAt)
  reset failure counter
  save state and disconnect WiFi
  return Ok changed=true
```

Always close files before rename/remove. All failures preserve existing cache. On success, WiFi is disconnected before returning. Feed watchdog during long operations.

## Schedule Logic

`RainmakerSchedule::nextDelaySeconds(uint16_t nowLocalMinutes)` returns seconds until next sync:

- X4 uses FIXED mode only (solar times ignored due to RTC drift)
- X3 can use solar times from manifest if available

Rules:

1. Start = fixed start time
2. End = fixed end time
3. If start >= end, fallback to 08:00-22:00
4. If now < start, delay until start
5. If start <= now < end, delay by interval (don't exceed end)
6. If now >= end, delay until next day's start
7. Clamp interval to at least 5 minutes

If next interval would exceed end, schedule next day start.

Consecutive failures: If >3 consecutive failures, consider disabling sync until user intervention (display on sleep screen or settings).

## Timer Wake Integration

In `src/main.cpp`:

- Timer wake detection at top of `setup()`:

```cpp
const bool rainmakerTimerWake = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
```

- On timer wake with sync enabled: run `RainmakerSyncService::sync(Scheduled)`, then `enterDeepSleep(true)` and `return`
- Before deep sleep (after WiFi teardown in `enterDeepSleep()`): call `esp_sleep_enable_timer_wakeup(delaySeconds * 1000000ULL)`

**OTA Safety**: Call `RainmakerSyncService::disableTimerWakeForOta()` or check a "sync disabled during update" flag before setting timer wake.

Power-button wake stays armed. Timer wake is additive.

## Sleep Screen Mode

Add `RAINMAKER` sleep screen mode.

In the sleep-screen rendering path:

```text
if SETTINGS.sleepScreen == RAINMAKER:
  if /.crosspoint/rainmaker_sync/latest.bmp exists:
    draw BMP full-screen
    if consecutiveFailures > 0: show small error badge
  else:
    fallback to dark/light/custom
else:
  existing CrossPoint behavior
```

Reuse `SleepActivity::renderBitmapSleepScreen()` and `BmpViewerActivity` patterns. BMP render failures fall back safely.

## Manual Sync UI

One entry: **Sync Rainmaker dashboard**.

Flow:

1. Connecting Wi-Fi...
2. Fetching manifest...
3. Downloading dashboard... (only if changed)
4. Result: `Dashboard updated` / `Dashboard already current` / specific failure
5. Return to previous UI.

Manual sync never sleeps the device automatically.

## Build Milestones

### Milestone 1: Compile-only Settings

- Add settings fields.
- Add settings UI entries.
- Build firmware: `pio run -e r4sync`.
- Flash via SD card or web flasher.
- Confirm existing reader opens books and settings screen works.

### Milestone 2: Manual Manifest Fetch

- Add `RainmakerManifest` parser.
- Add `RainmakerSyncService::sync(Manual)` with manifest fetch only.
- Show parsed manifest result in UI/log.

### Milestone 3: BMP Download + Verify

- Download to temp.
- Verify size and SHA-256 in chunks (watchdog fed).
- Verify BMP header.
- Atomically replace cache.
- Confirm cache survives reboot.

### Milestone 4: Rainmaker Sleep Mode

- Add `RAINMAKER` sleep-screen enum.
- Draw cached BMP during sleep (reuse existing patterns).
- Show error badge for consecutive failures.
- Confirm user can override back to other sleep modes.

### Milestone 5: Timer Wake

- Add `esp_sleep_enable_timer_wakeup` after WiFi teardown.
- Implement `disableTimerWakeForOta()`.
- Test with 2-minute interval.
- Confirm timer wake syncs and returns to deep sleep.
- Confirm power button wake still works.

### Milestone 6: Production Hardening

- Add failure counter and graceful degradation.
- Add watchdog feeding during sync.
- Add OTA safety check.
- Add user warning for low battery manual sync.

## Test Matrix

### Happy paths

- Manual sync with valid credentials downloads `latest.bmp`.
- Manual sync skips when SHA-256 unchanged.
- Sleep mode `RAINMAKER` displays cached dashboard.
- Scheduled timer wake syncs and returns to deep sleep.
- Power-button wake opens normal CrossPoint UI.
- User can disable sync.
- User can change sleep screen away from Rainmaker.

### Failure paths

- Missing URL -> clear manual error.
- Bad auth -> no cache replacement.
- Server offline -> old dashboard remains, failure counter incremented.
- Invalid manifest -> old dashboard remains.
- SHA mismatch -> temp deleted, old dashboard remains.
- Low battery scheduled wake -> skip sync, schedule next wake.
- WiFi timeout -> `WifiFailed`, state records error.
- Corrupted state -> falls back to defaults.
- >3 consecutive failures -> degraded state shown on sleep screen.
- OTA update -> timer wake disabled during update.

## Rebase Safety Rules

- Keep new logic in `src/rainmaker/`.
- Touch `main.cpp` in the smallest possible blocks.
- Append settings and enum values; never reorder existing values.
- Do not refactor CrossPoint sleep or Wi-Fi flows beyond required hooks.
- Avoid changing `HttpDownloader`; use existing Basic auth parameters.
- Commit in small layers matching milestones.

Suggested commits:

```text
rainmaker: add settings and manifest parser
rainmaker: add sync state and BMP verification
rainmaker: add manual dashboard sync action
rainmaker: add deterministic dashboard sleep screen
rainmaker: add scheduled timer wake
rainmaker: add OTA safety and failure recovery
```

## Done Definition

The fork is complete when:

- X4 can manually sync from the VPS.
- X4 displays Rainmaker dashboard as sleep screen from cached BMP.
- X4 timer-wakes during configured window (fixed time only), fetches only changed dashboards, returns to deep sleep.
- Existing e-reader behavior is unaffected.
- The branch rebases cleanly over upstream `develop`.
- OTA updates disable timer wake safely.
- Consecutive failures are handled gracefully.
