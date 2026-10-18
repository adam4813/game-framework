# 01: Game Target & headless Room harness

Spec: [Signal Room spec 1: Room & bootstrap](../spec.md). Read: [Game Target & bootstrap](../spec/game-target.md), [Testing decisions](../spec/testing.md).
Base branch: `feat/signal-room` (this game's main: branch off it, PR back into it, never into `main`)

**What to build:** Signal Room exists as its own Game Target (ADR-0006) and runs. Launching it on desktop or in the
browser opens a Room scene showing a bare floor from the fixed high-angle camera. A developer can write a test that
loads a Shift with no window, steps the world and queries it.

The game is 3D-rendered, but play happens on a plane. Operators will move freely and continuously over the floor
plane, collision is 2D on that plane, and everything placed (Stations, Counters, walls, props) snaps to a 1 m floor
grid. This ticket sets up that frame: a floor grid singleton and a camera authored in the Layout.

**Blocked by:** None (can start immediately).

**Status:** done

- [x] The Game Target is added from the root CMake. Game code is a static library linking `engine_lib`, a thin
      executable adds the entry point, and a test target links the same library through an `add_signal_room_test`
      helper.
- [x] The desktop and WASM presets build the Signal Room executable. Assets are copied (desktop) or preloaded (WASM)
      like the sandbox's.
- [x] `ctest --preset desktop-test-release` runs the Signal Room tests, and the total test count is at least the
      pre-change baseline.
- [x] One game module registers the game's Level loaders, reflected components and systems. Every game component is
      pre-registered in the module constructor. Physics is not imported.
- [x] A Room scene (tagged for pipeline filtering) loads a Shift file that names a Layout. The Layout is an engine Level
      JSON holding a `floor_grid` singleton (width, height, 1 m cell size, origin) and a camera entity aimed by a
      `look_at` ref.
- [x] On boot the game activates the Room scene with a minimal Shift, showing the bare grid floor from the fixed
      camera (~55° pitch, looking north).
- [x] A headless harness (scene-equivalence `TestWorld` style: `EngineContext` + `NullPlatform`) can load a Shift
      file, press actions by writing bound keys into `InputState`, step N frames at a fixed delta, and query the world.
- [x] A harness test loads the minimal Shift and checks the floor grid's size. A test that loads a Shift naming a
      missing Layout file fails with that path in the error.
- [x] An ImGui Room panel (main thread, scoped to the Room scene) lists the Shift files and has a Reload button that
      re-reads the Shift and Layout from disk.
- [x] Runtime check: the desktop build runs, `flecs-api switch-scene` to the Room scene works, and the browser build
      loads and shows the floor.

## Comments

**2026-10-18 (implementation):** Done on `feat/sr-room-01-game-target`.

- Game Target `games/signal-room/` (`signal_room_lib`, `signal_room`, `add_signal_room_test`), added last from the root
  CMake so it inherits the WASM `.html` suffix. It copies or preloads the game's assets plus the engine's `shaders` and
  `config`.
- The camera is orthographic (`ortho_size` 12.8, 55° pitch, looking north), framed like the greybox prototype. The
  engine orthographic Camera was cherry-picked from `feat/farm-preview-first-entry` at the user's request. See
  [backlog 07](../../backlog/issues/07-reconcile-ortho-camera.md).
- The harness presses raw keys (`Hold`/`Release`/`Tap`). It disables `InputPoll` and writes `InputState` itself.
  Action-level helpers come with the bindings file in ticket 03.
- The Room panel system is parented under the Room's root entity rather than tagged with `RoomSceneTag`. Tagging a
  system with the scene identity tag would let `FindScene<RoomSceneTag>` match the system instead of the scene.
- Tests went from 11 to 26 (camera projection unit tests plus the 4 Room tests).
- Runtime check: on desktop, `switch-scene Room` reloads the scene, and FloorGrid (13×9), RoomState and Camera are
  queryable over REST. The WASM build shows the floor in Chrome. `tools/flecs-api.js` now accepts any dotted module
  path, so `signal_room.*` components can be queried.
- Follow-up: [backlog 06](../../backlog/issues/06-shared-game-entry-point.md) (the entry point and WASM flags are
  copied from the sandbox).
