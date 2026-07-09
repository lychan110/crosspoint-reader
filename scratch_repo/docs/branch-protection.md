# Branch protection for origin/develop

`develop` on `lychan110/crosspoint-reader` is protected to enforce the rule that it must remain a clean mirror of `upstream/develop`.

## Required GitHub rule (Settings → Branches → Branch protection rules → develop)

1. **Branch name pattern:** `develop`
2. **Require a pull request before merging:** ON
   - Required approving reviews: 0 (this is a personal fork, no reviewers needed)
   - Dismiss stale pull request approvals when new commits are pushed: ON
3. **Require status checks to pass before merging:** OFF (mirror has no CI)
4. **Require linear history:** ON — this is the key one. It forces `rebase`/`squash`, so a direct merge commit landing on develop (the bug we just fixed) would be impossible.
5. **Include administrators:** ON — the guard must apply to everyone, including the owner.
6. **Allow force pushes:** ON, but **restrict to specific actors/teams** so only the owner can do it. The pre-push hook (`bin/pre-push-guard` is the committed copy) will still block the force-push unless it is a clean re-sync to `upstream/develop`.
7. **Allow deletions:** OFF — never delete `develop`.
8. **Block force pushes that create new commits matching specific patterns:** n/a

## Local guard: `.git/hooks/pre-push`

A `pre-push` hook is installed locally and rejects any push to `origin/develop` that is not a clean re-sync to `upstream/develop`. The hook is not committed (git ignores `.git/hooks/`); re-run `bin/install-hooks` after cloning.

## One-liner to apply the GitHub rule

```bash
gh api repos/lychan110/crosspoint-reader/branches/develop/protection \
  -X PUT \
  -f required_status_checks='null' \
  -f enforce_admins=true \
  -f required_pull_request_reviews='{"required_approving_review_count":0,"dismiss_stale_reviews":true}' \
  -f restrictions='null' \
  -f required_linear_history=true \
  -f allow_force_pushes=true \
  -f allow_deletions=false \
  -f block_creations=false \
  -f required_conversation_resolution=false \
  -f lock_branch=false
```

Note: `allow_force_pushes=true` is needed for the periodic re-sync to upstream. The `pre-push` hook is the second line of defense; the GitHub rule is the first.
