# rainmaker_manifest tests — skipped

The Rainmaker manifest parser (`src/rainmaker/RainmakerManifest.cpp`) depends
on ArduinoJson, which is a header-only library that is not installed on this
host and is not fetched by the test harness. The Phase 5 plan explicitly says
to skip the host manifest tests when ArduinoJson cannot be cleanly included
and to cover the parser's behaviour via the hardware smoke checklist on real
firmware instead. See `docs/rainmaker_production_readiness.md` (Phase 5) for
the full rationale.

If a future task installs ArduinoJson (via `apt install libarduinojson-dev`
or by adding a `FetchContent_Declare(ArduinoJson ...)` in this directory's
`CMakeLists.txt`), the test cases listed in the Phase 5 plan are:

- Valid manifest populates all required fields and `validate()` returns true.
- Missing `sha256` -> `parseManifest` returns false.
- Wrong `version` (e.g. 2) -> false.
- Wrong `width` (e.g. 479) -> false.
- Truncated `sha256` (32 hex chars in JSON) -> `isHexString` rejects, false.
- Overlong `sha256` (65 hex chars) -> `copyBounded` truncates, `isHexString`
  still fails, false.
- Overlong `bmpUrl` (200 chars) -> false.
- Optional `sunriseLocalMinutes` / `sunsetLocalMinutes` present -> populated.
- Optional solar fields absent -> both stay at -1.
