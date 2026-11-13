# Signal Room: Agent Instructions

Game-specific instructions for the `games/signal-room/` Game Target. The root `AGENTS.md` still applies; this file adds
to it.

## Git

- **Base branch: `feat/signal-room`.** It is long-lived, local only (not pushed to `origin`, the upstream framework
  repo), and never merges into `main`. Branch each ticket off it and diff or review against it
  (`feat/signal-room...HEAD`).
- **Merging a ticket is a local squash, not a pull request.** The in-between commits on a ticket branch are scratch
  history. Land each ticket on `feat/signal-room` as at most two commits, then delete the ticket branch
  (`git branch -D`):
  1. **Framework commit** (only if the ticket touched shared code): everything outside `games/signal-room/`, code and
     docs together, worded without game references so it cherry-picks cleanly into upstream. Build and run `ctest` on it alone before committing.
  2. **Ticket commit**: the game code, assets, tests and ticket docs, plus any game-only additions to shared docs.

  A game-only ticket is one `git merge --squash <branch>` and one commit. Engine work the game needs still lands on
  `feat/signal-room`, never directly on `main`.

## Specs and tickets

Signal Room's specs, tickets and backlog live in `games/signal-room/.scratch/`, laid out like the root `.scratch/`
(`docs/agents/issue-tracker.md`): `wayfinder/` (the slice map and its decision issues), one folder per spec
(`room-bootstrap/`, `core-loop/`) and `backlog/issues/`. New game work goes there, not in the root `.scratch/`, which
holds engine and cross-game work.

## Running it

- **REST port: `27760`** (baked into `main.cpp`, so it runs beside the sandbox on `27750`). Point the tools at it:
  `FLECS_REST_PORT=27760 node tools/flecs-api.js wait Station`.
