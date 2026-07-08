**Project name:** `crosspoint-rainmaker` — include in every codebase-memory-mcp tool call.

**Project id for cross-project edges:** target projects in cross-repo-intelligence mode as needed (e.g. `rainmaker` to find call sites that build the dashboard BMP the firmware consumes).

---

## What this repo is

This is a **rebase-friendly fork** of [crosspoint-reader/crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader) that adds Rainmaker X4 dashboard sync. All Rainmaker work lives on the `rainmaker-sync` branch.

**Upstream is the source of truth for everything CrossPoint.** This fork adds a thin slice; it does not reorganise, refactor, or "improve" the upstream codebase. The moment we diverge on something that isn't strictly Rainmaker-related, we are doing it wrong.

| Where | Source of truth |
|---|---|
| Reader engine, EPUB, fonts, sleep screens, OTA, network stack | upstream `develop` |
| Rainmaker sync feature | this fork's `rainmaker-sync` branch |
| Spec, settings, sync algorithm, schedule logic | [`docs/plans/crosspoint-fork-implementation-handoff.md`](./plans/crosspoint-fork-implementation-handoff.md) — authoritative |
| Friendly summary | [`README.md`](../README.md) |
| Historical scaffold (frozen) | [`docs/plans/README-original-scaffold.md`](./plans/README-original-scaffold.md) |

---

## Hard rules (any of these being broken is a stop-the-line bug)

### 1. Never push to upstream

- `origin` = `lychan110/crosspoint-reader` — your fork. **Push here.**
- `upstream` = `crosspoint-reader/crosspoint-reader` — read-only reference. **Never `git push` here.**

If a tool, subagent, or CI workflow proposes pushing to `upstream`, refuse and re-target to `origin`.

### 2. Rebase discipline — the headline rule

The whole point of this fork is that it stays a clean topic branch on top of upstream so we can absorb upstream changes cheaply. The rebase is the cost of doing business.

- Always develop on `rainmaker-sync` (never on `develop` or `master`).
- Sync with upstream regularly, before any non-trivial commit, and absolutely before opening a PR:
  ```bash
  git fetch upstream
  git rebase upstream/develop
  git push --force-with-lease origin rainmaker-sync
  ```
- If `rebase` produces conflicts, resolve them so the **post-rebase diff is still a clean Rainmaker delta** — never "fix" upstream code as part of a conflict. If a conflict reveals that upstream has changed something we depend on, fix our code to use the new upstream API, not the other way around.
- `--force-with-lease`, never bare `--force`. The lease guard catches the case where someone else has pushed to `rainmaker-sync` in the meantime.
- Before force-pushing after rebase, **always** confirm with the user. `--force-with-lease` to a public branch is the kind of thing they should approve.

### 3. Keep Rainmaker code in `src/rainmaker/` only

- New files go in `src/rainmaker/` (Manifest, SyncState, SyncService, Schedule, SleepScreen, Sha256).
- Edits to existing files must be **minimum-scope**:
  - `src/main.cpp` — only the timer-wake block, nothing else.
  - `src/CrossPointSettings.h` — only **append** fields, never insert into the middle of an existing enum.
  - `src/SettingsList.h` — only add new entries, never reorder existing ones.
- Any change that "while you're there" fixes an upstream issue is forbidden — file a separate issue or PR against upstream, or leave it alone.
- Don't move or rename existing upstream files. Renames make rebase conflicts exponentially worse.

### 4. Follow upstream build and safety conventions

CrossPoint has a Resource Protocol (see `.skills/SKILL.md`). Rainmaker code is firmware code and must obey it:

- **380KB RAM ceiling** (ESP32-C3) is non-negotiable. Justify any new heap allocation. Prefer static / stack / `std::array`.
- **Single-buffer mode** (`EINK_DISPLAY_SINGLE_BUFFER_MODE=1`) — be aware before you allocate anything that looks like a framebuffer.
- **No exceptions, no RTTI** (`-fno-exceptions`).
- **Strings** — use `tr()` for UI strings, fixed `char[]` buffers with `snprintf` for construction. No `std::string` in hot paths.
- **Flash persistence** — large const data must be `static const` or `constexpr`, in flash, not DRAM.
- **`new` is not nothrow on ESP32** — always `new (std::nothrow)` and null-check, or use `makeUniqueNoThrow<T>()` from `lib/Memory/Memory.h`.
- **No PSRAM** — ESP32-C3 has no PSRAM. Don't allocate buffers assuming you have it.
- **SPIFFS write throttling** — guard settings writes with value-change checks; debounce progress saves.
- **SdFat destructor closes files** — do not add explicit `file.close()` for local `FsFile` variables.
- **Logging** — use `LOG_INF` / `LOG_DBG` / `LOG_ERR` from `Logging.h`, not raw `Serial`.

These rules exist because CrossPoint has spent years tuning for the hardware. Ignore them and you break the build, the RAM budget, or both.

### 5. Build must plug into the upstream release env

The fork must produce a buildable OTA bundle that follows the same path as upstream releases:

- Build with `pio run -e gh_release` (not `default` or `slim`) for any release artifact.
- The resulting `.pio/build/gh_release/` must contain: `firmware.bin`, `bootloader.bin`, `partitions.bin`, `firmware.elf`, `firmware.map`. These are the artifacts upstream's `release.yml` uploads.
- The `gh_release_rc` env is for release candidates. Use it for `release/<x.y>` branches.
- The `slim` env is for size-constrained builds; do not enable Rainmaker in slim builds (sync needs the network stack).
- Run `bin/clang-format-fix` before committing C++ changes. Upstream's `ci.yml` rejects unformatted code on PRs.
- If you change `platformio.ini`, keep the diff to Rainmaker-only additions (`-D ENABLE_RAINMAKER_SYNC`, `-D RAINMAKER_*` defines). Do not retune existing envs.

### 6. OTA packaging & release flow

The fork must be **flashable the same way upstream is** — three paths, all must work:

1. **SD card firmware update** — `firmware.bin` + `bootloader.bin` + `partitions.bin` copied to SD card root, used by the device's `SdFirmwareUpdateActivity`. This is the user-facing recovery path. Build artifacts must be drop-in compatible with what the upstream `SdFirmwareUpdateActivity` expects.
2. **Web OTA** — `WebSocket` upload of `firmware.bin` via the device's web UI. Must match upstream's OTA partition layout.
3. **GitHub Releases** — tag-driven release flow that mirrors `release.yml` in upstream. A tag of the form `rainmaker-sync/v0.1.0` (or similar) should trigger a release build that publishes the same `firmware.bin` / `bootloader.bin` / `partitions.bin` / `firmware.elf` / `firmware.map` bundle.

When adding the OTA workflow to the fork, do not modify the existing upstream workflows. Add a new one (e.g. `.github/workflows/rainmaker-release.yml`) that calls the same build commands. Never edit `.github/workflows/release.yml`, `release_candidate.yml`, or `release-fonts.yml` — those are upstream files.

OTA partition layout is fixed by `partitions.csv`. Do not change it. If Rainmaker needs persistent storage beyond what `/.crosspoint/rainmaker/` already provides on the SD card, push for an SD-only solution; do not change the partition table.

### 7. Test before declaring done

The handoff defines 6 build milestones. Each one ends with a concrete test from the test matrix. A milestone is "done" only when its test passes on real hardware (or in a faithful sim). "Compiles" is necessary but not sufficient.

- Milestone 1 (compile-only settings) is the only milestone where "builds and existing reader still works" is enough.
- Milestones 2–6 all require hardware verification (or a stub of the network/manifest endpoint that the firmware can actually fetch from).

---

## Workflow cheatsheet

### Starting a change

```bash
cd ~/projects/crosspoint-rainmaker
git fetch upstream
git status                          # clean? on rainmaker-sync?
git rebase upstream/develop         # absorb upstream changes first
git checkout -b rainmaker-sync      # if you got bounced
```

### During a change

- New code in `src/rainmaker/`.
- Settings fields: **append** to `CrossPointSettings.h`, never insert.
- Before commit: `bin/clang-format-fix` then `git diff --stat` to verify the diff is Rainmaker-only.
- Commit messages: conventional commits, no `Co-Authored-By` footers, scope to Rainmaker where it makes sense (`feat(rainmaker): ...`, `fix(rainmaker): ...`).

### Committing and pushing

```bash
git add <rainmaker-only files>
git commit -m "feat(rainmaker): ..."
git push --force-with-lease origin rainmaker-sync
```

### Opening a PR

- PR target base is always `upstream:develop` (set `--base develop`).
- Head is `rainmaker-sync` on this fork (set `--head rainmaker-sync`).
- Before opening the PR, `git fetch origin` so the local tracking ref is fresh — `gh pr create` will fail with "No commits between develop and X" otherwise. Add a rebase step too.
- Pass `--repo lychan110/crosspoint-reader` to every `gh` command. The local clone's first remote is `upstream`, so `gh pr view` / `gh pr list` default to the wrong repo without it.
- PR body in a temp file, then `--body-file`. Do not use `--body` with backticks or `$` in the body — shell will mangle it.

### After the PR is reviewed

- Merge via the REST API, not `gh pr merge` (ruleset edge cases):
  ```bash
  gh api repos/lychan110/crosspoint-reader/pulls/<N>/merge \
    -X PUT -f merge_method=merge
  ```
  Note: this merges into the fork's `develop`. **It does not** land in upstream `develop`. To get changes upstream, the maintainer of `crosspoint-reader/crosspoint-reader` would need to accept the PR — and that is a separate decision the user makes explicitly, not something we do from this fork.
- After merge: delete the remote branch (it's just `rainmaker-sync`, so be ready to recreate it from `upstream/develop` next time), switch back to `develop`, pull.

---

## What NOT to do

- ❌ Push to `upstream`. Ever.
- ❌ Edit upstream files except for the minimum-scope Rainmaker changes listed in the handoff.
- ❌ Refactor, rename, or "clean up" upstream code in passing.
- ❌ Change the partition table, the OTA flow, or upstream CI workflows.
- ❌ Enable Rainmaker in the `slim` env (no network stack there).
- ❌ Skip the rebase because "I'll do it later" — every un-rebased day is a future-conflict day.
- ❌ Skip `bin/clang-format-fix` — CI will reject the PR.
- ❌ Open a PR without `--base develop` explicit. Conflating `--base` and `--head` is the most common way to PR the wrong direction.
- ❌ Use `std::string`, raw `new`, or `std::vector` without `.reserve()` in Rainmaker code.
- ❌ Use `git push --force` (no lease). Use `--force-with-lease` and confirm with the user first.
- ❌ Drop the spec handoff from the conversation. If the handoff is missing a field, ask before inventing it.

---

## Pointers

- `.skills/SKILL.md` (symlinked from `CLAUDE.md`) — upstream's AI agent rules. Read it; it has the full Resource Protocol, build flags, and architecture map.
- `AGENTS.md` — repo-template agent guidance (lychan110 default). Inherited from `central-brain/templates/repo-starter/`.
- `.kilo/kilo.jsonc` — Kilo tool permissions (grep/glob/find denied; git allowed; commit allowed; push asks).
- `.github/workflows/ci.yml` — upstream's CI: clang-format + cppcheck + PlatformIO build. Mirrors what local CI looks like; PRs to this fork that touch `src/` or `src/rainmaker/` will run it.
- `.github/workflows/ntfy-alert.yml` — repo-template ntfy push/PR/issue/release alerter.
- `bin/clang-format-fix` — the formatter script CI runs. Run it before committing.
- `docs/plans/crosspoint-fork-implementation-handoff.md` — the spec.
- `README.md` — the friendly summary.

---

## Re-anchoring checklist (start of any AI session on this repo)

1. `cd ~/projects/crosspoint-rainmaker && pwd` — confirm you're in the fork, not in the rainmaker repo or somewhere else.
2. `git status` — clean? On `rainmaker-sync`? If not, **stop and ask the user** which branch they want and which working tree state to keep.
3. `git fetch upstream && git fetch origin` — get both remotes current.
4. `git log --oneline upstream/develop..HEAD` — see what Rainmaker commits exist ahead of upstream.
5. `git log --oneline HEAD..upstream/develop | head -20` — see what upstream has done since this branch's base. **If non-empty, run a rebase before any new work.**
6. `uname -s` — detect host platform per `.skills/SKILL.md`.
7. Read the handoff's "Build Milestones" section to know which milestone is current.
8. Only then start.
