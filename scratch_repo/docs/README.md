# Docs

Planning documents, design notes, and historical context for the `rainmaker-sync` fork.

## Plans

- [`plans/crosspoint-fork-implementation-handoff.md`](plans/crosspoint-fork-implementation-handoff.md) — **authoritative technical spec** for the Rainmaker X4 dashboard sync. Defines the manifest contract, settings, sync algorithm, schedule rules, build milestones, and test matrix. When in doubt, this is the source of truth.
- [`plans/README-original-scaffold.md`](plans/README-original-scaffold.md) — historical README scaffold that the top-level `README.md` was derived from. Kept for lineage / "what changed and why" reference. Do not edit; the canonical version lives in `projects/rainmaker/.kilo/plans/`.

## How they relate

```
/README.md                                  ← friendly summary (lives at the repo root)
    └── plans/crosspoint-fork-implementation-handoff.md   ← authoritative spec
    └── plans/README-original-scaffold.md                  ← historical scaffold (do not edit)
```

Top-level `README.md` and the handoff are kept in lockstep on every spec change. The scaffold is frozen at the point of first PR (#1) so future drift can be measured.
