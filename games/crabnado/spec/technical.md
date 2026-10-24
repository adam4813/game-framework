# Technical

Back to the [spec](../spec.md).

## User stories

1. As the developer, I want to know in the first hour whether the WASM build can handle a big horde, so that I size the
   game to the engine before building content on it.
2. As a web player, I want the game to stay smooth (around 60 fps) through the 5-minute run, so that it's playable in
   the browser.
3. As the developer, I want game state reflected in the Flecs Explorer, so that I can inspect and tweak a running game
   over the Remote API.
4. As the developer, I want runs to be reproducible from a seed, so that a bug seen once can be replayed.

## Implementation decisions

- **Crabnado-only modules** live in the Game Target, never in the engine (ADR-0006): run, horde (enemies and
  crabnado), waves (wind and the wave field), weapons, upgrades, arena, HUD. Each is a Flecs module imported by the
  game's top-level module. Engine changes happen only when a ticket shows the game can't work around a gap.
- **Performance gate (first ticket):** spawn about 1,000 enemies in the WASM Release build and measure FPS. If it
  can't hold about 60 fps, cap the horde (target about 400) and make up the difficulty with tougher enemies. The cap
  is tuning data. Each enemy is a Flecs entity with reflected components, rendered as instanced engine primitives.
- **Crowding and hit checks** use a game-side uniform spatial grid: one singleton rebuilt each frame from landed
  enemies (cell size about 2.2 units in the prototype), queried for separation, bubble hits and claw range. The
  engine's steering separation/cohesion is not used for the horde, because it compares every agent with every other
  one.
- **No physics engine.** Movement, knockback, rock collisions and the wave push are hand-written distance and velocity
  maths. Jolt isn't used.
- **Determinism:** all gameplay randomness (spawns, landing jitter, wind shifts, rogue waves, upgrade rolls) draws
  from the engine's `RngState`, seeded per run. The seed is in the run state.
- **Pausing** is data (ADR-0004): simulation systems are `Pausable`. Molting, Esc pause and the results screen use
  it.
- **Reflection:** every gameplay component and singleton (run state, wind, player stats, enemy state, crabnado state)
  is reflected, so the Remote API can query and set it.
- **Flecs Remote API** for Crabnado serves on port 27751, so it can run beside the sandbox.
- **Builds:** both the `desktop` and `wasm` presets must build. The browser build is the one judges play.
