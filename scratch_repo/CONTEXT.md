**Project name:** `crosspoint-rainmaker` — include in every codebase-memory-mcp tool call.

**Project id for cross-project edges:** target projects in cross-repo-intelligence mode as needed (e.g. `rainmaker` to find call sites that build the dashboard BMP the firmware consumes).

---

## Branching (simplified)

- `rainmaker-sync` is the working branch. Treat it as the default/master branch. Commit directly to it.
- `develop` is read-only. **Do not commit, merge, or open a PR targeting `develop`.**
- If you ever find yourself on `develop`, run:
  ```bash
  git checkout rainmaker-sync
  ```
- Manual rebases onto `origin/develop` are done only when explicitly needed to pull upstream changes; do not rebase as a routine step before every push.

---

## File Allow-List (violate = stop-the-line bug)

Only these paths may be modified for Rainmaker work:
- `src/rainmaker/`
- `docs/`
- `AGENTS.md`
- `CONTEXT.md`
- `.kilo/`
- `.github/` (except never edit: `release.yml`, `release_candidate.yml`, `release-fonts.yml`)
- `bin/`
- `.cbmignore`
- `.env.example`
- `.ignore`

---

## Hardware Constraints (ESP32-C3: 380KB RAM, no PSRAM)

- Prefer stack/static over heap
- `new (std::nothrow)` or `makeUniqueNoThrow<T>()` for allocations
- No `std::string` in hot paths — use `char[]` + `snprintf`
- No exceptions / RTTI (`-fno-exceptions` is set)
- Use `LOG_INF/LOG_DBG/LOG_ERR` — not raw `Serial`

---

## Key Files

- `docs/plans/crosspoint-fork-implementation-handoff.md` — the spec (authoritative)
- `.skills/SKILL.md` — upstream cross-project rules
- `bin/clang-format-fix` — run before commits
- `scripts/gen_i18n.py` — generates I18nKeys.h/I18nStrings.cpp
