# Rainmaker Production Audit & Fix Plan

## Goal
Fix all bugs, resolve build blockers, verify the firmware compiles, add host-side unit tests, and ensure the fork is production-ready for flashing.

## Task List

### Phase 1: Fix Critical Bugs

- [ ] **1.1 Fix `const` compile error in RainmakerManifest.cpp:129,136**
  File: `src/rainmaker/RainmakerManifest.cpp`
  Change `const long long v = ...` to `long long v = ...` on lines 129 and 136.
  The `const` qualifier prevents `v = 0` assignment on the next line — this is a hard compile error.

- [ ] **1.2 Add missing `#include <time.h>` to RainmakerSyncService.cpp**
  File: `src/rainmaker/RainmakerSyncService.cpp`
  Add `#include <time.h>` in the include block (line 9-10). `time(nullptr)` at line 168 needs this declaration.

### Phase 2: Fix Branch Alignment

- [ ] **2.1 Switch to `rainmaker-sync` branch**
  The current HEAD is on `session/agent_880f91cb-9038-4b40-bd29-e783b55c21bf`. Per CONTEXT.md, all work must live on `rainmaker-sync`.
  ```bash
  git stash                     # stash any uncommitted changes
  git checkout rainmaker-sync   # switch to correct branch
  git merge --ff-only session/agent_880f91cb-9038-4b40-bd29-e783b55c21bf  # fast-forward rainmaker-sync
  ```
  If not fast-forwardable, cherry-pick the rainmaker commits (4d2e121, 81ff9a9, 3fd8e9d, 82681f1, 3f03bbd).

### Phase 3: Resolve Build Blockers

- [ ] **3.1 Manually install tool-scons**
  The tarball exists at `/tmp/agent_2cb6eb6c-e17e-4118-9efe-498ea8ff9ab4/tool-scons.tar.gz`.
  Create `~/.platformio/packages/tool-scons/` with proper `package.json`:
  ```json
  {
    "name": "tool-scons",
    "version": "4.40801.0",
    "description": "SCons software construction tool"
  }
  ```
  Extract the tarball contents into that directory. PlatformIO recognizes pre-installed packages by their `package.json` manifest.

- [ ] **3.2 Set PLATFORMIO_CORE_DIR**
  The sandbox environment requires `PLATFORMIO_CORE_DIR` to be a writable path.
  ```bash
  export PLATFORMIO_CORE_DIR="${PWD}/.pio-platformio"
  ```
  This is already documented in `bin/install-deps.sh`.

- [ ] **3.3 Run `pio run -e default`**
  Attempt the full build. If scons-using platform packages are already cached and recognized, the build should proceed. The project-local deps (ArduinoJson, QRCode, PNGdec, JPEGDEC, WebSockets) will be downloaded by the SCons build.

### Phase 4: Build Verification

- [ ] **4.1 Verify compilation succeeds**
  Run `pio run -e default`. Fix any additional compilation errors that surface.

- [ ] **4.2 Run clang-format check**
  ```bash
  ./bin/clang-format-fix
  ```
  Verify no files are reformatted.

- [ ] **4.3 Run gen_i18n.py**
  ```bash
  python3 scripts/gen_i18n.py
  ```
  Verify no errors. The 33 rainmaker i18n strings are already in `english.yaml`.

### Phase 5: Host-Side Unit Tests

The existing test framework uses `gtest` via CMake FetchContent. Tests live in `test/` and are built/run with:
```bash
cmake -S test -B build/test && cmake --build build/test && ctest --test-dir build/test --output-on-failure
```

- [ ] **5.1 RainmakerManifest parser tests**
  New file: `test/rainmaker_manifest/RainmakerManifestTest.cpp`
  Test cases:
  - Valid manifest parses all fields correctly
  - Missing required field (sha256, bytes, bmpUrl) → parse fails
  - Invalid version (not 1) → validate fails
  - Wrong dimensions (not 480x800) → validate fails
  - Non-hex SHA-256 → validate fails
  - Truncated strings (URL longer than 192 chars) → bounded copy
  - Optional solar minutes present → parsed correctly
  - Optional solar minutes absent → -1 sentinel
  - Empty/null JSON → parse fails

- [ ] **5.2 Sha256 tests**
  New file: `test/rainmaker_sha256/Sha256Test.cpp`
  Test cases:
  - Known-answer test vector (e.g. empty string → e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)
  - hashBuffer produces correct lowercase hex
  - equalsHex case-insensitive comparison
  - equalsHex rejects mismatched strings
  - equalsHex rejects null pointers
  - hashFile on nonexistent path returns false

- [ ] **5.3 RainmakerSchedule tests**
  New file: `test/rainmaker_schedule/RainmakerScheduleTest.cpp`
  Test cases:
  - Before window → delay to start
  - In window → delay by interval
  - After window → delay to next day start
  - Next interval would cross end → delay to next day start
  - Solar mode with valid sunrise/sunset → uses solar times
  - Solar mode without cached solar times → falls back to fixed
  - start >= end → fallback to 08:00-22:00
  - utcHmToLocalMinutes conversion correctness
  - fallbackDelaySeconds clamps minimum 5 minutes

- [ ] **5.4 RainmakerSyncState tests**
  New file: `test/rainmaker_sync_state/RainmakerSyncStateTest.cpp`
  Test cases:
  - clear() resets all fields to defaults
  - loadState on missing file returns false, state is cleared
  - saveState produces valid JSON, loadState round-trips correctly
  - state paths are correct constants

- [ ] **5.5 Add CMakeLists.txt for each new test directory**
  Each test directory needs a `CMakeLists.txt` following the pattern in `test/release_json_parser/`.

- [ ] **5.6 Build and run all tests**
  ```bash
  cmake -S test -B build/test && cmake --build build/test && ctest --test-dir build/test --output-on-failure
  ```

### Phase 6: Production Readiness Validation

- [ ] **6.1 Verify file allow-list compliance**
  Check that only files in the CONTEXT.md allow-list are modified:
  - `src/rainmaker/` — all new rainmaker files ✓
  - `src/CrossPointSettings.h` — appended fields ✓
  - `src/SettingsList.h` — appended entries ✓
  - `src/main.cpp` — timer-wake block only ✓
  - `src/activities/boot_sleep/SleepActivity.cpp` — RAINMAKER case ✓
  - `src/activities/settings/SettingsActivity.{h,cpp}` — RainmakerSyncNow action ✓
  - `lib/I18n/translations/english.yaml` — 33 rainmaker strings ✓

- [ ] **6.2 Verify heap discipline**
  - No bare `new`/`new[]` in rainmaker code
  - No `push_back` without `reserve`
  - No allocations in hot paths
  - `std::string` on cold paths only (sync service)
  - All fallible allocations use `new (std::nothrow)` or are avoided

- [ ] **6.3 Verify HAL abstraction compliance**
  - All SD access uses `Storage` (HalStorage) and `HalFile`
  - No direct `SdFat`/`FsFile` use
  - User-facing strings use `tr(STR_*)`
  - No hardcoded 800/480

- [ ] **6.4 Verify rebase safety**
  - All new code in `src/rainmaker/`
  - Upstream files only appended/added to, never reordered
  - Enum values appended, not inserted

- [ ] **6.5 Verify against handoff spec test matrix**
  Document which test cases are verified by host-side tests vs. which must be verified on-device:
  - **Host-side verifiable**: manifest parsing, manifest validation, SHA-256, schedule logic, state persistence
  - **Device-only (manual)**: manual sync with valid credentials, manual sync skips unchanged, RAINMAKER sleep displays cache, timer wake syncs and returns to sleep, power button wake still works, disable sync, non-Rainmaker sleep mode, low battery skip, no solar times with solar mode

- [ ] **6.6 Commit all fixes**
  Single commit with all bug fixes and test additions:
  ```bash
  git add <changed files>
  git commit -m "fix(rainmaker): fix compile errors, add host-side unit tests"
  ```

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| `tool-scons` manual install doesn't work | Medium | High | PlatformIO docs confirm manual install via package.json; if it fails, install `scons` via pip and configure PIO to use system scons |
| Additional compile errors after fixing bugs | Medium | Medium | Iterative: fix each as it surfaces in `pio run` |
| Project-local deps (ArduinoJson etc.) fail to download | Low | Medium | These are from well-known repos (GitHub, PlatformIO registry); should work once scons is resolved |
| Host-side tests need mocking of HalStorage | High | Medium | RainmakerSyncState tests need `Storage` mock or file-based test. Use temporary directory for state files in tests. |
| Sha256 tests need file I/O | Medium | Low | Create temp files in test; `hashFile` tests can use a known content file |

## Open Questions

- Should `rainmakerStartMinutes`/`rainmakerEndMinutes` remain as `uint8_t` quanta (0..239) or be changed to `uint16_t` minutes (0..1439) as the handoff spec originally called for? The quanta approach saves 2 bytes of settings but limits resolution to 6 minutes. Current implementation uses quanta — this is a design decision, not a bug. Recommend keeping as-is unless the 6-minute resolution is insufficient.