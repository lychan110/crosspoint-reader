# CrossPoint × Rainmaker (X4 Sync Fork)

> **Fork of [crosspoint-reader/crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader).**
> This branch (`rainmaker-sync`) adds autonomous Rainmaker dashboard sync to the Xteink X4 / X3 — no phone bridge, just a VPS-served BMP and a timer wake. All upstream CrossPoint features (EPUB reader, fonts, sleep screens, web UI, OTA, etc.) are preserved.

[![Upstream](https://img.shields.io/badge/upstream-crosspoint--reader-blue)](https://github.com/crosspoint-reader/crosspoint-reader)
[![Rainmaker](https://img.shields.io/badge/dashboard-rainmaker-orange)](https://github.com/lychan110/rainmaker)
[![Branch](https://img.shields.io/badge/branch-rainmaker--sync-success)]()

## What is this?

A minimal, rebase-friendly fork that lets an Xteink X4 / X3 fetch a pre-rendered Rainmaker dashboard (`manifest.json` + `latest.bmp`) from a VPS over HTTPS, cache the BMP on SD, and use it as a deterministic sleep screen. The device wakes on a timer during a configurable window (fixed hours or solar), syncs if the manifest changed, then goes back to deep sleep.

**Rainmaker remains the canonical renderer and publisher.** The firmware only consumes the static artifacts — it does not render widgets. CrossPoint's own reader engine, sleep screens, and settings UI are untouched.

## Intent

- Fetch `manifest.json` and `latest.bmp` over HTTPS with Basic auth
- Cache the BMP and render it as a sleep screen
- Scheduled timer wake during a configurable waking window (fixed hours or sunrise/sunset)
- Manual sync from a settings entry
- Preserve every existing CrossPoint feature (books, UI, power-button wake, etc.)

## Repository Setup

```bash
git clone https://github.com/lychan110/crosspoint-reader.git crosspoint-rainmaker
cd crosspoint-rainmaker
git remote rename origin upstream
git remote add origin https://github.com/lychan110/crosspoint-reader.git
git fetch upstream
git checkout upstream/develop
git checkout -b rainmaker-sync
```

Ongoing sync with upstream:

```bash
git fetch upstream
git rebase upstream/develop
git push --force-with-lease origin rainmaker-sync
```

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
| `rainmakerIntervalMinutes` | uint8_t | 30 | Sync interval, clamped to ≥5 |
| `rainmakerStartMode` | uint8_t | 0 | `RAINMAKER_BOUND_FIXED=0`, `RAINMAKER_BOUND_SOLAR=1` |
| `rainmakerEndMode` | uint8_t | 0 | `RAINMAKER_BOUND_FIXED=0`, `RAINMAKER_BOUND_SOLAR=1` |
| `rainmakerStartMinutes` | uint16_t | 480 (08:00) | Local minutes from midnight |
| `rainmakerEndMinutes` | uint16_t | 1320 (22:00) | Local minutes from midnight |
| `rainmakerMinBatteryPercent` | uint8_t | 20 | Scheduled sync skips below this % |
| `rainmakerDefaultedSleepMode` | uint8_t | 0 | Set to 1 after first auto-default to RAINMAKER sleep screen |

First-time enable auto-defaults `sleepScreen` to `RAINMAKER` if the user hasn't chosen anything else. If they later pick a different sleep mode, respect that.

A new `RAINMAKER` value is appended to the existing sleep-screen enum. Existing values are not renumbered.

Settings UI entries: enable toggle, manifest URL, username, password, interval (5–180), start/end mode, start/end minutes, minimum battery, and a **Sync dashboard now** manual action (or a home-menu entry if the settings framework can't host an action row).

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
- `bmpUrl` is used as-is — no string concatenation
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

### Sync modes

- **Scheduled** — skips on disabled, missing config, or low battery; never forces a UI redraw; preserves old cache on failure
- **Manual** — ignores the low-battery guard (warns if the UI supports it); shows progress; returns to the previous screen

## Schedule Logic

`RainmakerSchedule::nextDelaySeconds(uint16_t nowLocalMinutes)` returns the seconds until the next sync attempt, given:

- `rainmakerIntervalMinutes`, `rainmakerStartMode`, `rainmakerEndMode`
- `rainmakerStartMinutes`, `rainmakerEndMinutes`
- Cached `sunriseLocalMinutes` / `sunsetLocalMinutes`
- Current local minutes from the CrossPoint clock/timezone

Rules:

1. Start = fixed value unless start mode is `SOLAR` and cached sunrise is valid
2. End = fixed value unless end mode is `SOLAR` and cached sunset is valid
3. If `start >= end`, fall back to 08:00–22:00
4. If `now < start`, delay until start
5. If `start <= now < end`, delay by interval (don't punch past end)
6. If `now >= end`, delay until tomorrow's start
7. Clamp interval to at least 5 minutes

If the next interval would cross the end boundary, prefer tomorrow's start over a wake at end that does nothing.

## Timer Wake Integration

`src/main.cpp` changes only:

- Include `esp_sleep.h`
- Detect timer wakeup at the top of `setup()`
- On timer wake with sync enabled: run `RainmakerSyncService::sync(Scheduled)`, then `enterDeepSleep(true)` and `return`
- Before deep sleep: if sync enabled, call `esp_sleep_enable_timer_wakeup(delaySeconds * 1'000'000ULL)`
- Power-button wake stays armed — timer wake is additive, never replaces it

## Sleep Screen

When `SETTINGS.sleepScreen == RAINMAKER`:

- If `/.crosspoint/rainmaker/latest.bmp` exists, draw it full screen
- Otherwise fall back to dark/light/custom or a status message
- The Rainmaker BMP never lands in `/.sleep` or any random-wallpaper directory
- BMP render failures fall back safely

Reuse CrossPoint's existing BMP draw utility if there is one; otherwise add only what 1-bit / 24-bit Rainmaker output needs.

## Manual Sync UI

One entry: **Sync Rainmaker dashboard**.

Flow:

1. Connecting Wi-Fi…
2. Fetching manifest…
3. Downloading dashboard… (only if changed)
4. Result: `Dashboard updated` / `Dashboard already current` / specific failure
5. Return to the previous screen

Manual sync never sleeps the device automatically.

## Build Instructions

### Prerequisites

- [PlatformIO Core](https://platformio.org/install/cli) (or PlatformIO IDE)
- ESP32-S3 dev board (X4) or ESP32 (X3)
- USB cable for flashing

### Build

```bash
git clone https://github.com/lychan110/crosspoint-reader.git
cd crosspoint-reader
git checkout rainmaker-sync
pio run                  # build
pio run --target upload  # flash
pio device monitor       # serial log
```

### `platformio.ini` (X4 env)

```ini
[env:x4]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
build_flags =
    -D X4_TARGET
    -D ENABLE_RAINMAKER_SYNC
```

## Build Milestones

Incremental — each milestone is independently testable:

1. **Compile-only settings** — add fields, add UI entries, build, confirm existing reader still opens books
2. **Manual manifest fetch** — parser + `sync(Manual)` with manifest only; log the result
3. **BMP download + verify** — temp file, byte/SHA-256 check, atomic replace, cache survives reboot
4. **Rainmaker sleep mode** — `RAINMAKER` enum value + draw cached BMP, confirm override back to other modes
5. **Timer wake** — `esp_sleep_enable_timer_wakeup`, test with a temporary 2-minute interval, confirm power-button wake still works
6. **Full schedule + solar + battery guard** — fixed/solar bounds, cached solar times, low-battery skip, manual sync still runs

## Test Matrix

### Happy paths

- Manual sync with valid credentials downloads `latest.bmp`
- Manual sync again skips when SHA-256 unchanged
- Sleep mode `RAINMAKER` displays the cached dashboard
- Scheduled timer wake syncs then returns to deep sleep
- Power-button wake still opens the normal CrossPoint UI

### Failure paths

- Manifest fetch fails → state records `lastError`, cache preserved
- Manifest invalid (bad version, missing field, bad dimensions, non-hex SHA-256) → `ManifestInvalid`, no download
- BMP download truncates → byte-count mismatch, `DownloadFailed`, no replace
- SHA-256 mismatch → `HashMismatch`, tmp deleted, cache preserved
- Scheduled run below battery threshold → `LowBattery`, no Wi-Fi attempt

## Credits & Provenance

- **Upstream:** [crosspoint-reader/crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader) — all reader, network, and HAL code is theirs
- **Device:** [Xteink X4 / X3](https://www.xteink.com/) — ESP32-S3 / ESP32 e-ink hardware
- **Dashboard:** [Rainmaker](https://github.com/lychan110/rainmaker) — canonical renderer and VPS publisher

This fork is **not affiliated with CrossPoint Reader, Xteink, or any device manufacturer**.

## License

Same as upstream CrossPoint Reader — see `LICENSE` in this repo.
