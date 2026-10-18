# Check formatting before calling work done

Status: needs-triage
Source: retro of [12: Operator movement tidy-ups](12-operator-movement-tidy.md), 2026-10-18

## Context

Ticket 03 landed lines that `.clang-format` reformats (an over-long `UpdateDash` signature, a long
`BuildGreyboxModel` call, test lambdas). Nothing checks formatting: there is no pre-commit hook, and CI runs only for
`main`. The drift surfaced as unrelated reflows in ticket 12's diff, which reviewers then have to read past.

## To do

- [ ] Decide on the guardrail: a line in the root `AGENTS.md` "before calling work done" list
      (`clang-format --dry-run --Werror` over the changed files), a script in `tools/`, or a CMake/ctest target.
- [ ] Apply it once over `games/signal-room/` (and `src/` if wanted) in its own formatting-only commit, so later
      diffs stay clean.
