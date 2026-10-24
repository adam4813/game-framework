# Crabnado: agent rules

Crabnado is a 1.5-day game jam game. For work inside `games/crabnado/`, these rules take precedence over the
repo-wide `AGENTS.md` and `CODING_STANDARDS.md`. Everything they don't cover still applies.

- **Spec and tickets:** `spec.md` is the index (its Sections table points at `spec/<topic>.md`), and tickets are in
  `issues/`. A ticket's `Spec:` line names the sections to read.
- **Verify by playing.** A ticket is done when the `desktop` preset builds and its acceptance criteria are checked in
  the running game through the Flecs Remote API (Crabnado serves it on port 27751).
- **WASM is validated once, at the end of the jam**, in one pass that builds the `wasm` preset and plays it in a
  browser. Tickets don't need a `wasm` build, Emscripten or a browser check: mark a WASM-only acceptance item as
  deferred to that pass and finish the rest on desktop.
- **Tests only where `spec/testing.md` calls for one.** Keep the existing Crabnado tests passing with
  `ctest --preset desktop-test-release`. When running `/implement`, skip its `/tdd` and `/code-review` steps unless
  the user asks for them.
- **Ship over polish.** If a ticket runs long, follow the cut list in `spec.md` and note the cut in the ticket's
  `## Comments`.
