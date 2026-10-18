# Signal Room: Agent Instructions

Game-specific instructions for the `games/signal-room/` Game Target. The root `AGENTS.md` still applies; this file adds
to it.

## Git

- **Base branch: `feat/signal-room`.** It is long-lived and never merges into `main`. Branch work off
  `feat/signal-room`, open pull requests against it (`gh pr create --base feat/signal-room`), and diff or review
  against it (`feat/signal-room...HEAD`). Engine/framework work the game needs lands here too, not on `main`.
