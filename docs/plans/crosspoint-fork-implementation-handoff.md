# CrossPoint Fork Implementation Handoff: Rainmaker Dashboard Sync

> **Authoritative spec.** The friendly summary lives in [`/README.md`](../../README.md). If they ever disagree, this handoff wins. The historical scaffold (now superseded) is at [`README-original-scaffold.md`](./README-original-scaffold.md).

## Purpose

Implement a minimal, rebase-friendly CrossPoint Reader firmware fork that lets an Xteink X4 fetch a pre-rendered Rainmaker dashboard directly from a VPS and use it as a deterministic sleep screen.

The CrossPoint fork must not render widgets. Rainmaker remains the canonical renderer and publisher. The firmware only consumes:

- `manifest.json`
- `latest.bmp`

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

## Rainmaker Endpoint Contract

The VPS publishes static artifacts behind HTTPS Basic auth:

```text
GET /manifest.json
GET /latest.bmp
```

Both require the same Basic auth credentials.

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
- Treat `sunriseLocalMinutes` and `sunsetLocalMinutes` as optional.
- Use `bmpUrl` from the manifest, not string concatenation.
- Verify downloaded BMP SHA-256 and byte count before replacing cache.

## CrossPoint Source Areas Already Identified

From upstream CrossPoint `develop`:

- `src/main.cpp`
  - owns setup, loop, deep sleep, wake routing, and `enterDeepSleep()`.
  - already tears down Wi-Fi before sleep.
  - already uses `APP_STATE.showBootScreen` and quick-resume frame behavior.
- `src/CrossPointSettings.h`
  - stores persistent settings.
  - append fields; do not insert enum values in the middle.
- `src/SettingsList.h`
  - registers settings shown in device/web settings UI.
- `src/network/HttpDownloader.h`
  - has `fetchUrl(url, string&, username, password)`.
  - has `downloadToFile(url, destPath, progress, cancelFlag, username, password)`.
  - already supports HTTPS with CA bundle and Basic auth.
- Existing deep sleep path uses power-button wake only through FreeInk power helpers.
  - Add app-level `esp_sleep_enable_timer_wakeup(delayUs)` before deep sleep.
  - Do not modify FreeInk SDK unless absolutely necessary.

## New Files to Add

Keep most Rainmaker code in new files.

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

If upstream style prefers flat `src/`, use `Rainmaker*.h/cpp` in `src/`, but a subdirectory is cleaner.

## Settings to Add

Append to `CrossPointSettings`:

```cpp
uint8_t rainmakerSyncEnabled = 0;
char rainmakerManifestUrl[160] = "";
char rainmakerUsername[48] = "";
char rainmakerPassword[80] = "";
uint8_t rainmakerIntervalMinutes = 30;
uint8_t rainmakerStartMode = 0; // 0=fixed, 1=sunrise
uint8_t rainmakerEndMode = 0;   // 0=fixed, 1=sunset
uint16_t rainmakerStartMinutes = 480;
uint16_t rainmakerEndMinutes = 1320;
uint8_t rainmakerMinBatteryPercent = 20;
uint8_t rainmakerDefaultedSleepMode = 0;
```

Add enum constants:

```cpp
enum RAINMAKER_BOUND_MODE {
  RAINMAKER_BOUND_FIXED = 0,
  RAINMAKER_BOUND_SOLAR = 1,
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

Add to `SettingsList.h` under a new category or existing system/display category:

- Enable Rainmaker Sync: toggle
- Manifest URL: string
- Username: string
- Password: string/password-like field if supported
- Sync interval minutes: value, suggested range 5–180
- Start mode: enum `Fixed`, `Sunrise`
- End mode: enum `Fixed`, `Sunset`
- Start time minutes: value 0–1439
- End time minutes: value 0–1439
- Minimum battery percent: value 0–100
- Manual action: `Sync dashboard now`

If CrossPoint’s settings framework does not support action rows cleanly, put manual sync in the home/menu activity instead.

## Runtime State File

Store sync runtime state separately from settings:

```text
/.crosspoint/rainmaker/state.json
/.crosspoint/rainmaker/latest.bmp
/.crosspoint/rainmaker/latest.tmp
```

State fields:

```json
{
  "lastSha256": "hex",
  "sequence": 123,
  "updatedAt": "2026-07-08T08:00:00Z",
  "sunriseLocalMinutes": 356,
  "sunsetLocalMinutes": 1234,
  "lastAttemptEpoch": 0,
  "lastSuccessEpoch": 0,
  "lastError": ""
}
```

If JSON parsing/writing is too heavy, use a simple binary struct or line-based key-value file. Keep it self-contained.

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

- Parse using whichever lightweight JSON facility CrossPoint already uses, if any.
- Otherwise use small manual extraction for top-level string/number fields.
- Reject missing required fields.
- Reject invalid dimensions.
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
};
```

Scheduled mode:

- skip if disabled
- skip if missing URL or credentials
- skip if battery below threshold
- run without forcing UI sleep screen redraw
- preserve old cache on failure

Manual mode:

- do not skip solely because of low battery; warn/confirm if UI supports it
- show progress/status
- return to UI on success/failure

## Sync Algorithm

```text
sync(mode):
  validate settings
  if scheduled and low battery: return LowBattery
  connect Wi-Fi using existing CrossPoint network mechanism
  fetch manifest with HttpDownloader::fetchUrl(url, body, username, password)
  parse and validate manifest
  update cached solar times if present
  if manifest.sha256 == state.lastSha256 and cached BMP exists:
    save state and return Ok changed=false
  download BMP to /.crosspoint/rainmaker/latest.tmp
  verify byte count
  compute sha256 over tmp
  compare to manifest.sha256
  rename tmp -> latest.bmp
  update state.lastSha256/sequence/updatedAt/solar times
  save state
  return Ok changed=true
```

Always close files before rename/remove. Always delete temp on failure.

## SHA-256 Implementation

Use ESP-IDF mbedTLS if available:

```cpp
#include <mbedtls/sha256.h>
```

Pseudo-flow:

```cpp
mbedtls_sha256_context ctx;
mbedtls_sha256_init(&ctx);
mbedtls_sha256_starts(&ctx, 0);
while (read chunks from file) {
  mbedtls_sha256_update(&ctx, buffer, len);
}
uint8_t digest[32];
mbedtls_sha256_finish(&ctx, digest);
mbedtls_sha256_free(&ctx);
hexEncode(digest, outHex);
```

Compare lowercase hex strings case-insensitively.

## Schedule Logic

Inputs:

- `rainmakerIntervalMinutes`
- `rainmakerStartMode`
- `rainmakerEndMode`
- `rainmakerStartMinutes`
- `rainmakerEndMinutes`
- cached `sunriseLocalMinutes`
- cached `sunsetLocalMinutes`
- current local minutes from CrossPoint clock/timezone

Function:

```cpp
uint32_t RainmakerSchedule::nextDelaySeconds(uint16_t nowLocalMinutes);
```

Rules:

1. Start = fixed start, unless start mode is sunrise and cached sunrise is valid.
2. End = fixed end, unless end mode is sunset and cached sunset is valid.
3. If start >= end, fallback to 08:00–22:00.
4. If now < start, delay until start.
5. If start <= now < end, delay by interval, but do not exceed end unless intentional.
6. If now >= end, delay until tomorrow’s start.
7. Clamp interval to at least 5 minutes.

Examples:

```text
now 07:00, window 08:00-22:00 -> 3600s
now 10:10, interval 30m -> 1800s
now 21:50, interval 30m -> 600s or next-day start; choose 600s to hit final boundary
now 22:10 -> next day 08:00
```

Recommended: if next interval would go past end, schedule next day start instead to avoid a wake exactly at end that does no sync.

## Timer Wake Integration

In `src/main.cpp`:

- include `esp_sleep.h`
- identify timer wake during setup:

```cpp
const bool rainmakerTimerWake = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
```

After settings/state load and before normal routing:

```cpp
if (rainmakerTimerWake && SETTINGS.rainmakerSyncEnabled) {
  auto result = RainmakerSyncService::sync(RainmakerSyncMode::Scheduled);
  enterDeepSleep(true);
  return;
}
```

Before deep sleep start, after power button wake has been armed:

```cpp
if (SETTINGS.rainmakerSyncEnabled) {
  uint32_t delaySeconds = RainmakerSchedule::nextDelaySeconds(nowLocalMinutes);
  if (delaySeconds > 0) {
    esp_sleep_enable_timer_wakeup((uint64_t)delaySeconds * 1000000ULL);
  }
}
```

Do not remove existing power-button wake. Timer wake is additive.

## Sleep Screen Mode

Add `RAINMAKER` sleep screen mode.

In the sleep-screen rendering path:

```text
if SETTINGS.sleepScreen == RAINMAKER:
  if cached BMP exists:
    draw BMP full-screen
  else:
    fallback to dark/light/custom or message
else:
  existing CrossPoint behavior
```

Important:

- Do not write Rainmaker BMP into `/.sleep` or `/sleep` random wallpaper directories.
- Use `/.crosspoint/rainmaker/latest.bmp` only.
- If BMP render fails, fall back safely.

If CrossPoint already has BMP image drawing utilities, use them. Otherwise implement only what is needed for 1-bit/24-bit BMP produced by Rainmaker.

## Manual Sync UI

Add one entry: `Sync Rainmaker dashboard`.

Flow:

1. Show progress: connecting Wi-Fi.
2. Fetch manifest.
3. Download/verify if changed.
4. Show:
   - `Dashboard updated`
   - `Dashboard already current`
   - or failure message.
5. Return to previous UI.

Manual sync does not sleep automatically.

## Build Milestones

### Milestone 1: Compile-only Settings

- Add settings fields.
- Add settings UI entries.
- Build firmware.
- Confirm existing reader opens books and settings screen works.

### Milestone 2: Manual Manifest Fetch

- Add `RainmakerManifest` parser.
- Add `RainmakerSyncService::sync(Manual)` with manifest fetch only.
- Show parsed manifest result in UI/log.

### Milestone 3: BMP Download + Verify

- Download to temp.
- Verify size and SHA-256.
- Atomically replace cache.
- Confirm cache survives reboot.

### Milestone 4: Rainmaker Sleep Mode

- Add `RAINMAKER` sleep-screen enum.
- Draw cached BMP during sleep.
- Confirm user can override back to other sleep modes.

### Milestone 5: Timer Wake

- Add `esp_sleep_enable_timer_wakeup` before deep sleep.
- Test with a temporary 2-minute interval.
- Confirm timer wake syncs and returns to deep sleep.
- Confirm power button wake still works.

### Milestone 6: Full Schedule + Solar + Battery Guard

- Add fixed/solar bounds.
- Cache solar times from manifest.
- Add low-battery skip for scheduled sync.
- Confirm manual sync can still run with warning.

## Test Matrix

### Required Happy Paths

- Manual sync with valid credentials downloads `latest.bmp`.
- Manual sync again skips unchanged image.
- Sleep mode `RAINMAKER` displays cached dashboard.
- Scheduled timer wake syncs and returns to deep sleep.
- Power button wake still opens normal CrossPoint UI.
- User can disable sync.
- User can choose non-Rainmaker sleep screen after enabling sync.

### Failure Cases

- Missing URL -> clear manual error.
- Bad auth -> no cache replacement.
- Server offline -> old dashboard remains.
- Invalid manifest -> old dashboard remains.
- SHA mismatch -> temp deleted, old dashboard remains.
- Low battery scheduled wake -> skip sync, schedule next wake.
- No cached solar times with solar mode -> fixed fallback.

## Rebase Safety Rules

- Keep new logic in `src/rainmaker/*`.
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
rainmaker: add solar schedule and battery guard
```

## Rainmaker VPS Setup Expected by Firmware

Example command after Rainmaker implementation:

```bash
X4_PUBLISH_USER=x4 \
X4_PUBLISH_PASS='<strong-password>' \
npm run x4:render -- \
  --config /srv/rainmaker/x4-dashboard.json \
  --out /srv/rainmaker/render/latest-render.bmp \
  --publish-dir /srv/x4 \
  --publish-base-url https://example.com/x4 \
  --manifest-id rainmaker-x4-dashboard
```

Expected public URLs:

```text
https://example.com/x4/manifest.json
https://example.com/x4/latest.bmp
```

Configure the X4 manifest URL to the manifest URL and set Basic auth username/password in CrossPoint settings.

## Done Definition

The fork is complete when:

- X4 can manually sync from the VPS.
- X4 displays Rainmaker dashboard as sleep screen from cached BMP.
- X4 timer-wakes during configured window, fetches only changed dashboards, and returns to deep sleep.
- Existing e-reader behavior is unaffected.
- The branch rebases cleanly over upstream `develop` with only small expected conflicts in settings/menu/main hooks.
