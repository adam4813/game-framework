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
  1. **Framework commit** (only if the ticket touched shared code): everything outside `games/signal-room/` and the
     game's `.scratch/` folders, code and docs together, worded without game references so it cherry-picks cleanly
     into upstream. Build and run `ctest` on it alone before committing.
  2. **Ticket commit**: the game code, assets, tests and ticket docs, plus any game-only additions to shared docs.

  A game-only ticket is one `git merge --squash <branch>` and one commit. Engine work the game needs still lands on
  `feat/signal-room`, never directly on `main`.
