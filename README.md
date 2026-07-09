# CrossPoint × Rainmaker (X4 Sync Fork)

Fork of [crosspoint-reader/crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader).
Adds Rainmaker dashboard sync to Xteink X4/X3. All upstream CrossPoint features preserved.

## IMMEDIATE ACTIONS (every session)

```bash
git status                       # MUST be clean
git branch --show-current        # MUST be rainmaker-sync
git fetch origin && git rebase origin/rainmaker-sync
```

## BRANCH RULES

| Branch | Purpose | Commit? |
|---|---|---|
| `origin/develop` | Mirror of upstream — read-only | **NO** |
| `rainmaker-sync` | All Rainmaker work | **YES** |

Never commit on `develop`. If you see commits there — stop.

For parallel sessions, use a worktree on a `feat/<desc>-<date>` branch off `rainmaker-sync`; merge with `--no-ff` and delete the branch. Never push `session/*` to origin (blocked by `bin/pre-push-guard`).

## SETUP

```bash
git clone https://github.com/lychan110/crosspoint-reader.git
cd crosspoint-reader
git checkout rainmaker-sync
./bin/install-deps.sh            # one-shot: PlatformIO, clang-format 21, Python deps, submodules
```

## BUILD

```bash
./bin/install-deps.sh          # one-shot: PlatformIO, clang-format 21, Python deps, submodules
./bin/pio run -e gh_release    # build release firmware.bin + bootloader.bin + partitions.bin
./bin/pio check                # static analysis (cppcheck)
ctest --test-dir build/test --output-on-failure -j   # host unit tests
./bin/clang-format-fix         # before commit (required)
```

Full build & flash guide (including how to roll back to a previous
firmware and recover from a bad flash) is in
[`docs/build-and-flash.md`](docs/build-and-flash.md). Cloud-agent
sandbox gotchas are in [`docs/ci-and-sandbox.md`](docs/ci-and-sandbox.md).

## FILE LAYOUT

```
src/rainmaker/                  # all new code
├── RainmakerManifest.{h,cpp}
├── RainmakerSyncState.{h,cpp}
├── RainmakerSyncService.{h,cpp}
├── RainmakerSchedule.{h,cpp}
├── RainmakerSleepScreen.{h,cpp}
├── RainmakerManualSyncActivity.{h,cpp}
└── Sha256.{h,cpp}

src/main.cpp                # timer-wake block only
src/CrossPointSettings.h     # append settings fields
src/SettingsList.h          # append settings entries
src/activities/boot_sleep/SleepActivity.cpp  # RAINMAKER case only
src/activities/settings/SettingsActivity.{h,cpp}  # RainmakerSyncNow action

bin/install-deps.sh
```

## SETTINGS

| Field | Type | Default |
|---|---|---|
| `rainmakerSyncEnabled` | uint8_t | 0 |
| `rainmakerManifestUrl` | char[160] | "" |
| `rainmakerUsername` | char[48] | "" |
| `rainmakerPassword` | char[80] | "" |
| `rainmakerIntervalMinutes` | uint8_t | 30 |
| `rainmakerStartMode` | uint8_t | 0 (FIXED) |
| `rainmakerEndMode` | uint8_t | 0 (FIXED) |
| `rainmakerStartMinutes` | uint16_t | 480 (08:00) |
| `rainmakerEndMinutes` | uint16_t | 1320 (22:00) |
| `rainmakerMinBatteryPercent` | uint8_t | 20 |

All appended to existing enums (no renumbering). First enable auto-defaults `sleepScreen=RAINMAKER` (one-shot).

## MANIFEST SCHEMA

```json
{ "version": 1, "id": "rainmaker-x4-dashboard", "sha256": "...", "bytes": N,
  "width": 480, "height": 800, "bmpUrl": "https://...",
  "sunriseLocalMinutes": N, "sunsetLocalMinutes": N }
```

Validation: version=1, sha256 lowercase hex 64 chars, bytes matches, dimensions 480x800.

## SYNC ALGORITHM

1. Validate config, check battery
2. Connect Wi-Fi
3. Fetch + parse manifest
4. If sha256 unchanged → return (scheduled) or "already current" (manual)
5. Download BMP to `latest.tmp`
6. Verify bytes + SHA-256
7. Atomic rename to `latest.bmp`
8. Persist state

Fail-safe: existing cache preserved on any error.

## TIMER WAKE

- `src/main.cpp`: detect timer wake in `setup()`, run scheduled sync, return to deep sleep
- Arm timer via `esp_sleep_enable_timer_wakeup(delay * 1'000'000ULL)` before sleep
- Power-button wake stays functional (additive, not replacement)

## BUILD MILESTONES

1. Compile-only settings (build only)
2. Manual manifest fetch + log result
3. BMP download + verify + atomic replace
4. RAINMAKER sleep mode draws cached BMP
5. Timer wake (test with 2-min interval)
6. Full schedule: fixed/solar + battery guard

## LICENSE

Same as upstream CrossPoint Reader. See `LICENSE`.