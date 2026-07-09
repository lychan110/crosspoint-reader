# AGENTS.md

Do NOT use grep, glob, file-search, or bash-based search as a primary search method.
Always use `uv run` over `python` and `python3`, and `uv add` over `pip install`.

### Intent to tool mapping
- "Find me functions/classes/handlers" -> `search_graph`
- "Who calls this / what does X call / show dependencies" -> `trace_path`
- "Show me the source of function X" -> `get_code_snippet` (find qualified_name via search_graph first)
- "What does this repo do / show me the structure" -> `get_architecture`
- "Complex cross-module queries" -> `query_graph`

### Call priority (use in this order)
0. `index_repository` - always re-index the repo at beginning of session and after code refactoring.
1. `search_graph`
2. `trace_path`
3. `get_code_snippet`
4. `query_graph`
5. `get_architecture`

### grep/glob are FORBIDDEN (hard-blocked by permissions)
Do not attempt grep or glob for any reason. If MCP tools return insufficient results,
ask the user for guidance instead of falling back to grep/glob.

### Examples
- Find a handler: `search_graph(name_pattern=".*OrderHandler.*", project="<project>")`
- Who calls it: `trace_path(function_name="OrderHandler", direction="inbound", project="<project>")`
- Read source: `get_code_snippet(qualified_name="pkg/orders.OrderHandler", project="<project>")`

## Defaults
- Optimize for research code: clear, correct-enough, fast to iterate.
- Use proportional reasoning. Prefer the shortest valid path.
- Keep responses concise.

## Workflow
- Read relevant files before editing. Never edit blind.
- Re-read only if files changed or context is missing.
- Skip files over 100KB unless needed.
- Plan briefly only for nontrivial tasks.

## Output
- No fluff.
- For implementation: code/patch first; explain only if needed.
- For review/debug: findings first.
- Keep code copy-paste ready.
- Comment only non-obvious logic.

## Memory Operations (auto-brain)
- **Before any complex or unfamiliar task** → `recall(query, modes=["hybrid"])` to load relevant context (skip for trivial lookups)
- **After non-trivial decisions, discoveries, or completions** → `remember(content, tags, scope)` to persist them
- **On encountering a gotcha, workaround, or subtle pattern** → `learn(content, tags="gotcha,<topic>", topic="<area>", project="<repo>", source="session")`
- **On inaccurate or obsolete entries** → `forget(id)` to invalidate them
- **When wrapping up or before compacting context** → `improve(strategy="nrem")` to deduplicate, then `pending()` + `harvested(ids=[...])` to curate learnings

## Code
- Use `uv` for Python envs/packages. `conda` is forbidden.
- Prefer simple, direct solutions.
- Avoid over-engineering and speculative abstractions.
- No single-use abstractions.
- Duplicate a few lines rather than abstract early.
- Do not add types, docs, or refactors to untouched code unless asked.
- Prefer fail-fast checks over elaborate error handling.
- Do not add retries, fallbacks, or defensive code for unlikely paths unless asked.
- Follow local repo style unless it hurts readability.

## Validation
- Use the Validation-First Development skill when a code change needs behavioral verification.

## Review & Debug
- State: bug -> cause -> fix.
- Base claims on code and evidence, not guesses.
- Verify API/version details only when relevant or uncertain.
- Do not expand scope unless asked.

## Execution
- Chain CLI commands when useful. Do not chain with `grep`, `glob`, and `find`.

## Git Commits

- NEVER include "Co-Authored-By" footers in commit messages. Author is always the user.
- Keep commit messages concise, focused on the what/why, not narration of the task.

## Parallel Work

If working in parallel with other sessions, use a worktree on a work-descriptively named branch off `rainmaker-sync` (e.g., `feat/<desc>-<date>`, `fix/<desc>-<date>`). Rebase onto current `rainmaker-sync` before merging, merge with `--no-ff`, push, then delete the branch.

Never name a branch `session/*` or `tmp/*`; those prefixes are reserved for cloud-agent internals and are blocked by `bin/pre-push-guard` from ever being pushed to origin.
