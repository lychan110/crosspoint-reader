**Project name:** `crosspoint-rainmaker` — include in every codebase-memory-mcp tool call.

**Project id for cross-project edges:** target projects in cross-repo-intelligence mode as needed (e.g. `rainmaker` to find call sites that build the dashboard BMP the firmware consumes).

---

## IMMEDIATE ACTIONS (do this first, every session)

```bash
cd /workspace/99fd9e94-e945-4128-b206-9b0905d0e89b/sessions/agent_2cb6eb6c-e17e-4118-9efe-498ea8ff9ab4
git status                          # MUST show clean working tree
git branch --show-current             # MUST be rainmaker-sync
git log --oneline --decorate        # verify commits are rainmaker-only
```

If not on `rainmaker-sync`, run:
```bash
git fetch origin
git checkout rainmaker-sync
```

---

## BRANCH STRUCTURE (sacred — never violate)

| Branch | Purpose | Can commit? |
|---|---|---|
| `origin/develop` | Mirror of `crosspoint-reader/crosspoint-reader` — read-only | **NO** |
| `rainmaker-sync` | All Rainmaker work lives here | **YES** |

Every commit must land on `rainmaker-sync`. If you see yourself committing on `develop` — stop, switch branches.

---

## REBASE BEFORE PUSHING (required, not optional)

Before any push to `origin rainmaker-sync`:

```bash
git fetch origin
git rebase origin/develop      # absorb any upstream changes
# If conflicts: resolve to keep a clean Rainmaker delta
git push --force-with-lease origin rainmaker-sync
```

**Why:** This fork must stay a clean topic branch so upstream can be rebased in easily.

---

## WHEN TO OPEN A PR

Only after code is on `rainmaker-sync` and passes:
```bash
./bin/clang-format-fix
python3 scripts/gen_i18n.py
```

Open via:
```bash
gh pr create --repo lychan110/crosspoint-reader \
  --base develop --head rainmaker-sync
```

**Important:** The PR target is `develop` (the fork's mirror), but the PR is NOT merged in. The PR is a record of work. The actual sync-to-upstream happens later via force-push. See "Re-anchoring checklist" below.

---

## WHAT TO COMMIT WHERE

### Must go in `src/rainmaker/`:
- New source files (Manifest, SyncState, SyncService, Schedule, SleepScreen, Sha256, ManualSyncActivity)

### Safe to edit (append-only):
- `src/CrossPointSettings.h` — append fields, never insert into enums
- `src/SettingsList.h` — append entries, never reorder
- `src/main.cpp` — only timer-wake block
- `src/activities/boot_sleep/SleepActivity.cpp` — only rainmaker case
- `src/activities/settings/SettingsActivity.{h,cpp}` — only rainmaker action

### Safe to create/edit:
- `docs/` — any rainmaker documentation
- `AGENTS.md`, `CONTEXT.md` — agent guidance
- `.kilo/` — tool config
- `.github/workflows/rainmaker-*.yml` — release workflows (new only, never edit upstream files)
- `bin/` — helper scripts
- `README.md` — user-facing docs

---

## FILE ALLOW-LIST (violate = stop-the-line bug)

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

## HARDWARE CONSTRAINTS (ESP32-C3: 380KB RAM, no PSRAM)

- Prefer stack/static over heap
- `new (std::nothrow)` or `makeUniqueNoThrow<T>()` for allocations
- No `std::string` in hot paths — use `char[]` + `snprintf`
- No exceptions / RTTI (`-fno-exceptions` is set)
- Use `LOG_INF/LOG_DBG/LOG_ERR` — not raw `Serial`

---

## RE-ANCHORING CHECKLIST (start of any session)

```bash
pwd                                           # must be in this workspace
git status && git branch --show-current        # clean? rainmaker-sync?
git fetch origin                             # get latest from fork
git log --oneline HEAD..origin/rainmaker-sync # any pending pushes?
git log --oneline --decorate --graph -10     # verify only Rainmaker commits
```

---

## KEY FILES

- `docs/plans/crosspoint-fork-implementation-handoff.md` — the spec (authoritative)
- `.skills/SKILL.md` — upstream cross-project rules
- `bin/clang-format-fix` — run before commits
- `bin/pre-push-guard` — blocks illegal pushes to `develop`
- `scripts/gen_i18n.py` — generates I18nKeys.h/I18nStrings.cpp