# Multi-game cmake targets with a shared engine library

The farm game was embedded inside `src/game/` alongside engine demonstration scenes, making it
impossible to build, test, or iterate on farm independently. We separated the project into
independent per-game cmake targets — each an executable built from its own source tree under
`games/<name>/` — all linking against a shared `engine_lib` static library compiled from
`src/engine/`.

## Key boundary rule

A module belongs to `src/engine/` if it could be useful to *any* game with no game-specific
semantics (render, physics, input, audio, tilemap, spatial, UI, …). A module belongs under
`games/<name>/src/` if it only makes sense in that game's domain (farm dialog, game_time,
player_inventory, crop systems, …). When a module is shared between two game targets it is
promoted to `src/engine/` rather than duplicated.

## Considered options

- **Shared game layer** (`src/game/` as a second static lib that all games link): rejected because
  it creates an ambiguous middle tier and encourages accumulating mixed concerns there over time.
- **Separate repo per game**: rejected as premature — the games share assets, CI, and the engine
  source; a monorepo is lower friction at this scale.

## Consequences

- `src/game/` narrows to the `sandbox` demo executable (CubeScene, NoiseScene, TilemapScene,
  TitleScene) used for engine feature development.
- Every new game starts in `games/<name>/` with its own CMakeLists.txt, `src/`, and `tests/`; it
  adds itself via `add_subdirectory` from the root.
- `engine_testable` is retired; its role is replaced by `engine_lib` (tests link the same target
  as production code).
