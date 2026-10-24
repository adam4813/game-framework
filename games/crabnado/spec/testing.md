# Testing

Back to the [spec](../spec.md).

This is a 1.5-day game jam game. Tests and code review are a hard sell, so **a test is added only when it is truly
needed**. The default check for a ticket is that it builds and plays correctly.

## Decisions

- **Default check (every ticket):** build the `desktop` preset, launch the game, and check the ticket's behaviour in
  the running game through the Flecs Remote API (switch scenes, click buttons, read and set run state, wind and player
  components). Check the `wasm` preset builds before the jam's final build.
- **The existing scene-flow test stays green** (title ⇄ arena through the buttons). It runs with
  `ctest --preset desktop-test-release`.
- **When a new test is truly needed:** only for logic that is easy to break silently and slow to check by playing.
  The one expected candidate is the **wave field**: it's a pure function, the ramp stages (one swell, then crossing
  swells, then rogue waves) take minutes of play to reach, and a regression there would quietly break the core
  mechanic. If it gets tests, they check behaviour a player would notice: e.g. "before 1:30 every crest pushes along
  the wind", "after a wind shift past 1:30, crests from two directions add up", "inside a pool the push is near zero".
- **Where a test goes:** the headless Crabnado harness (the full engine on the null platform with the Crabnado module
  imported, as the scene-flow test does) is the one seam for anything that needs the world. A pure module like the
  wave field is tested directly.
- **No test-first or code-review loop is required** by tickets. A ticket may ask for one when the logic warrants it.
- **Prior art:** the Crabnado scene-flow test (harness, button clicks inside a deferred frame); the engine's unit tests
  for pure logic.
