# Rainmaker Production Readiness Plan

## Goal
Make the CrossPoint × Rainmaker fork buildable, testable, and safe to flash as a release candidate. Do not call it production-ready until the required hardware smoke checklist passes on a real Xteink device.

## Key Decisions
- Host-side unit tests are required for portable Rainmaker logic only; avoid brittle HAL/Arduino host mocks unless a clean shim already exists.
- `gh_release` is the flash-candidate build; `default` must also compile to catch dev/release differences.
- A manual on-device smoke checklist is a hard safe-to-flash gate.
- Cache replacement must preserve the last known-good dashboard on every failure.
- Manifest fetch/parsing must be bounded and strict: no unbounded body buffering and no silently accepted truncation.

## Branching

- Work only on `rainmaker-sync`. Treat it as the default/master branch.
- `develop` is read-only. Do not commit, merge, or open a PR targeting `develop`.
- If you ever find yourself on `develop`, run:
  ```bash
  git checkout rainmaker-sync
  ```

## Phase 1 — Repository and Environment Sanity
- [ ] Confirm current branch is `rainmaker-sync` before further fixes.
- [ ] Initialize/update `freeink-sdk` submodule.
- [ ] Set a writable PlatformIO core dir for this sandbox/session:
  - `export PLATFORMIO_CORE_DIR="$PWD/.pio-platformio"`
- [ ] Resolve `tool-scons` without disabling TLS globally.
  - Preferred: install from the already-downloaded tarball into the PlatformIO package store with a valid `package.json` (`name: tool-scons`, `version: 4.40801.0`).
  - Then let PlatformIO resolve remaining project deps normally.

## Phase 2 — Must-Fix Compile and Runtime Bugs
- [ ] Fix hard compile error in `src/rainmaker/RainmakerManifest.cpp:129` and `:136`.
  - Change reassigned `const long long v` variables to non-const.
- [ ] Add the explicit time include for `time(nullptr)` in `src/rainmaker/RainmakerSyncService.cpp:168`.
- [ ] Load Wi-Fi credentials in the sync path.
  - `connectToSavedWifi()` must call `WIFI_STORE.loadFromFile()` before reading `getLastConnectedSsid()` / credentials.
  - This makes manual and timer sync independent of whichever UI activity happened to load credentials.
- [ ] Remove or guard `display.deepSleep()` in the timer-wake fast path at `src/main.cpp:411`.
  - Timer wake does not initialize the display; the fast path should sync, re-arm timer wake, shut down Wi-Fi, and enter deep sleep.

## Phase 3 — Make Sync Fail-Safe
- [ ] Replace remove-then-rename cache flow in `src/rainmaker/RainmakerSyncService.cpp:237-246`.
  - Required safe flow:
    1. Verify `latest.tmp` byte count and SHA-256.
    2. If `latest.bmp` exists, rename it to `latest.bak`.
    3. Rename `latest.tmp` to `latest.bmp`.
    4. If that fails, restore `latest.bak` to `latest.bmp`.
    5. Delete `latest.bak` only after success.
  - Result: the last known-good dashboard remains available after every failure.
- [ ] Set useful `state.lastError` before `saveState(state)` on failures.
  - Not required for correctness, but important for diagnosis after unattended timer wakes.

## Phase 4 — Bound Manifest Fetch and Validation
- [ ] Stop using unbounded `HttpDownloader::fetchUrl(url, std::string&)` for the manifest.
  - Use `HttpDownloader::fetchUrl(url, DataCallback, user, pass)` from `src/network/HttpDownloader.h:38`.
  - Accumulate into a fixed capped buffer, e.g. 2048 bytes including NUL.
  - Abort and return `ManifestFetchFailed` or `ManifestInvalid` if the response exceeds the cap.
- [ ] Reject any truncation of required manifest strings.
  - `copyBounded()` already detects truncation in `src/rainmaker/RainmakerManifest.cpp:32-40`; callers must check it.
  - Required fields (`sha256`, `bmpUrl`, dimensions, bytes, version) must fail validation if too long, missing, or wrong type.
  - A too-long SHA-256 must not be truncated to 64 chars and accepted.
- [ ] Handle JSON parse OOM explicitly.
  - If ArduinoJson reports `NoMemory`, return the existing `ERR_OOM` path or equivalent.

## Phase 5 — Host-Side Tests
Use existing host test flow from `test/README`:
```bash
cmake -S test -B build/test
cmake --build build/test
ctest --test-dir build/test --output-on-failure -j
```

- [ ] Add portable tests for `RainmakerSchedule`.
  - Before window → delay to start.
  - In window → interval delay.
  - After window → next-day start.
  - Interval crossing end → next-day start.
  - Solar times present → use solar bounds.
  - Solar times absent → fixed fallback.
  - Invalid start/end → 08:00–22:00 fallback.
  - `utcHmToLocalMinutes()` offset wrapping.
  - `fallbackDelaySeconds()` clamps to 5 minutes.
- [ ] Add portable tests for `Sha256::hashBuffer()` and `equalsHex()`.
  - Empty-string SHA-256 known vector.
  - Case-insensitive compare.
  - Mismatch and null-pointer rejection.
- [ ] Add manifest parser tests only if ArduinoJson can be cleanly included in the host CMake target.
  - Valid manifest.
  - Missing required fields.
  - Wrong version.
  - Wrong dimensions.
  - Invalid or too-long SHA-256.
  - Overlong `bmpUrl` rejected.
  - Optional solar times present/absent.
- [ ] Do not add fragile host tests for `RainmakerSyncState`/SD persistence unless a clean `HalStorage`/Arduino `String` shim already exists.
  - Cover SD persistence in the hardware smoke checklist instead.

## Phase 6 — Build and Static Validation Gates
- [ ] Run formatting and i18n generation:
```bash
./bin/clang-format-fix
python3 scripts/gen_i18n.py
```
- [ ] Build both required environments:
```bash
pio run -e default
pio run -e gh_release
```
- [ ] Treat only `gh_release` output as the flash-candidate artifact.
- [ ] If available and not over the two-minute command limit, also run focused static checks; otherwise record why they were skipped.
- [ ] Verify heap/layering discipline in Rainmaker code:
  - No bare `new` / `new[]`.
  - No unbounded manifest body allocation.
  - SD access goes through `Storage` / `HalFile`.
  - No direct SdFat / raw SDK storage calls.
  - User-facing strings use `tr(STR_*)`.
  - `std::string` remains cold-path only.

## Phase 7 — Manual Hardware Smoke Gate
The firmware is only a release candidate until these pass on a real Xteink X4/X3 target.

- [ ] Flash the `gh_release` artifact.
- [ ] Boot with Rainmaker disabled; confirm normal CrossPoint UI and reader behavior still works.
- [ ] Configure Wi-Fi through the existing network UI; reboot; confirm Rainmaker manual sync can use saved credentials.
- [ ] Configure Rainmaker manifest URL, username, and password.
- [ ] Manual sync valid manifest: downloads, verifies, and writes `/.crosspoint/rainmaker/latest.bmp`.
- [ ] Manual sync unchanged manifest: reports already current and does not rewrite cache.
- [ ] Bad auth / offline server / invalid manifest / SHA mismatch: old `latest.bmp` remains intact and temp files are cleaned.
- [ ] Rainmaker sleep screen renders cached dashboard.
- [ ] User can switch sleep screen away from Rainmaker and it is respected.
- [ ] Timer wake with short interval: wakes, syncs or skips, re-arms, and returns to deep sleep without showing UI.
- [ ] Power-button wake still opens normal CrossPoint UI.
- [ ] Low-battery scheduled path skips sync; manual sync remains available.
- [ ] Solar mode without cached solar times falls back to fixed schedule.

## Definition of Done
- Correct branch: `rainmaker-sync`.
- `freeink-sdk` present.
- Host portable tests pass.
- `./bin/clang-format-fix` and `python3 scripts/gen_i18n.py` pass.
- `pio run -e default` and `pio run -e gh_release` pass.
- Safe cache replacement, bounded manifest fetch, Wi-Fi credential loading, and timer fast-path cleanup are implemented.
- Hardware smoke checklist passes before labeling the fork production-ready or safe for routine flashing.

## Explicit Non-Goals
- Do not refactor unrelated CrossPoint code.
- Do not make Rainmaker render widgets on-device.
- Do not vendor Rainmaker into the firmware repo.
- Do not bypass the HAL for SD/storage access.
- Do not call the cloud build alone “production-ready”; hardware smoke is mandatory.
