**Project name:** `crosspoint-rainmaker` — include in every codebase-memory-mcp tool call.

**Project id for cross-project edges:** target projects in cross-repo-intelligence mode as needed (e.g. `rainmaker` to find call sites that build the dashboard BMP the firmware consumes).

---

## Branching

Two-branch model:

- `rainmaker-sync` is the integration branch. Treat it as the default branch.
- `develop` is a byte-for-byte mirror of `upstream/develop`. Read-only. **Never commit, merge, or open a PR targeting `develop`.**

If you are on `develop`, switch back:

```bash
git checkout rainmaker-sync
```

### When to use a worktree / feature branch

- **Single-session, non-experimental work:** commit directly to `rainmaker-sync`. This is the default.
- **Parallel sessions, experimental work, or work you might want to revert cleanly:** create a worktree on a work-descriptively named branch off current `rainmaker-sync`. Examples:
  - `feat/safety-cache-2026-07-09`
  - `fix/timer-wake-fail-2026-07-09`
  - `chore/install-deps-2026-07-09`

  Rebase the feature branch onto the latest `rainmaker-sync` before merging. Merge with `--no-ff`, push `rainmaker-sync`, then delete the feature branch. The worktree is your private namespace; the merge commit on `rainmaker-sync` is the durable record.

### Branch naming

- Use a conventional-commit prefix: `feat/`, `fix/`, `chore/`, `docs/`, `refactor/`, `style/`, `test/`, `build/`.
- Append a short kebab-case description and the date.
- **Never name a branch `session/*` or `tmp/*`.** Those prefixes are reserved for cloud-agent internals and are blocked by `bin/pre-push-guard` from ever being pushed to origin.

### Pull requests

- PRs are for human review. They are not the integration mechanism for automated work.
- If a session opens a PR, it must be merged or closed the same day. A PR whose head diverges from the base for hours will become a regression waiting to land (see PR #6 post-mortem).

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
