# Spec 1 · Game Target & bootstrap

Part of [Signal Room spec 1: Room & bootstrap](../spec.md). The `games/signal-room/` target, its build, assets, modules and the Room scene's load order.

## User stories

1. As a developer, I want Signal Room to build as its own executable from `games/signal-room/`, so that it can be built,
   run and tested apart from the sandbox and other games.
2. As a developer, I want the desktop and WASM presets to build the Signal Room target along with the sandbox, so that
   I don't need a separate build recipe.
3. As a developer, I want the game's code compiled into a library that both the executable and the tests link, so that
   tests exercise the same code that ships.
4. As a developer, I want Signal Room's tests registered with CTest through the existing test presets, so that
   `ctest --preset desktop-test-release` runs them with the engine tests.
5. As a player, I want the game to open straight into Shift 1's room, so that I can start moving with no menus (menus
   arrive with the meta spec).
6. As a player on the web build, I want the room to load and play in the browser, so that the slice can be shared as a
   link.
7. As a developer, I want the room to run as a registered scene, so that the Flecs Remote API can switch to it and
   query it.
8. As a developer, I want the game's assets (Shift, Layout, Item, tuning, greybox and palette files) copied or preloaded
   with the build like the sandbox's, so that the executable finds them on desktop and web.

## Implementation decisions

- **New Game Target per ADR-0006** under `games/signal-room/`: its own CMake file added from the root with
  `add_subdirectory`, its own `src/`, `tests/` and assets. The game code builds as a static library (game code only,
  linking `engine_lib`). A thin executable adds the entry point and a test target links the same library. A
  `add_signal_room_test` helper mirrors `add_engine_test`. Asset copy (desktop) and preload (WASM) mirror the sandbox's,
  including the WASM pre-js scripts. The entry point follows the sandbox `main` (platform, engine context, Flecs remote
  on desktop, emscripten main loop on web).
- **Engine modules imported:** level, render, input, particles, pathfinding, debug UI. **Not physics** (Jolt is not used
  in the slice, per issue 07). Every game component is pre-registered in the module constructor, as `CODING_STANDARDS.md` asks.
  The crash that rule guards against still has an unverified cause (see
  [lazy component registration crash](https://github.com/adam4813/game-framework/issues/27)).
- **One game module** that registers the game's Level loaders, components (with reflection, so the Explorer shows
  them) and systems. **One Room scene**, tagged for pipeline filtering. Its `Load` order: load the palette, greybox,
  Item and tuning registries → register systems → load the Shift → build the Layout → spawn the Operator and starting
  Items. On boot the game activates the Room scene with Shift 1. Title, Slot and Shift pickers are spec 5.
- **Game vocabulary:** use `games/signal-room/GLOSSARY.md` terms in identifiers: Shift, Layout, Station, Tarped,
  Counter, Item, Operator, Roster, Target. Never "level" for a Shift, never "Mainframe".
