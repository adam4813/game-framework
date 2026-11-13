# Crabnado: agent rules

Crabnado is a 1.5-day game jam game. For work inside `games/crabnado/`, these rules take precedence over the
repo-wide `AGENTS.md` and `CODING_STANDARDS.md`. Everything they don't cover still applies.

- **Design and tickets:** `GDD.md` describes the game as built. Read it before changing gameplay, and update it when
  a change alters what it says. Open tickets are in `issues/`. The jam's spec and finished tickets are at the
  `crabnado-jam` tag.
- **Verify by playing.** A ticket is done when the `desktop` preset builds and its acceptance criteria are checked in
  the running game through the Flecs Remote API (Crabnado serves it on port 27751).
  The `.mcp.json` flecs MCP tools target port 27750 and won't reach it, so use
  `FLECS_REST_PORT=27751 node tools/flecs-api.js …` instead.
- **WASM is validated once, at the end of the jam**, in one pass that builds the `wasm` preset and plays it in a
  browser. Tickets don't need a `wasm` build, Emscripten or a browser check: mark a WASM-only acceptance item as
  deferred to that pass and finish the rest on desktop.
- **Tests only where truly needed:** logic that is easy to break silently and slow to check by playing, such as the
  wave field's ramp stages. Keep the existing Crabnado tests passing with
  `ctest --preset desktop-test-release`. When running `/implement`, skip its `/tdd` and `/code-review` steps unless
  the user asks for them.
- **Ship over polish.** If a ticket runs long, cut scope and note the cut in the ticket's `## Comments`.
