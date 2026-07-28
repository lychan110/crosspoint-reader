# Code Review Guide

This document defines the approach and standards for reviewing code changes in this repository. Reviews should be proportionate to the research-oriented nature of the project — do not demand production-grade hardening unless the task explicitly requires it.

## Core Review Axes

Every review should assess changes against four dimensions, weighted according to the scope and intent of the work:

1. **Correctness** — Does the code do what it claims? Are there logic errors, unsafe assumptions, or failure paths that could produce wrong results?
2. **Requirement Alignment** — Does the change match what was asked for? Is anything missing, or has unrequested scope been introduced?
3. **Validation Credibility** — Does the evidence (tests, logs, manual checks) actually support the claimed behavior? Are there gaps between what changed and what was verified?
4. **Maintainability** — Will someone else understand this code in six months? Are there hidden assumptions, brittle coupling, or confusing structure?

## What to Check

### Requirement Alignment

- Does the implementation match the task description, bug report, or specification?
- Is any expected behavior absent?
- Has scope crept beyond what was requested in a way that introduces risk or changes behavior?

### Correctness Risk

- Are there logic errors, unhandled edge cases, or unsafe assumptions?
- Does the fix address the root cause rather than just the visible symptom?
- Are there paths where the code could fail silently or produce incorrect output?

### Validation Credibility

- Does the provided validation (tests, reproduction steps, output samples) actually demonstrate the claimed behavior?
- Is the level of evidence appropriate for the risk of the change?
- Are there areas touched by the change that were not covered by validation?

### Maintainability

- Is the code clear enough for its expected lifespan and reuse?
- Are there implicit assumptions that a future reader would not easily discover?
- Is the structure straightforward, or does it create unnecessary cognitive load?

## What to Avoid

- Reviewing code outside the supplied scope unless it is needed to explain a concrete issue.
- Demanding test-driven development as a blanket requirement.
- Insisting on production-maximalist patterns (excessive abstraction, defensive layers, error handling for impossible cases) when the task does not call for them.
- Inflating minor style or clarity concerns into blockers.
- Suggesting speculative improvements that have no clear consequence if left unaddressed.
- Rewriting the design unless a design-level issue is necessary to explain a concrete technical risk.

## Tooling Notes

If bash is restricted or expected tools are not found, consult `AGENTS.md` for setup instructions before proceeding.
