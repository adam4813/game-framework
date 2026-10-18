# 01: Game Target & headless Room harness

Spec: [Signal Room spec 1: Room & bootstrap](../spec.md)
Base branch: `feat/signal-room` (this game's main: branch off it, PR back into it, never into `main`)

**What to build:** Signal Room exists as its own Game Target (ADR-0006) and runs. Launching it on desktop or in the
browser opens a Room scene showing a bare floor from the fixed high-angle camera. A developer can write a test that
loads a Shift with no window, steps the world and queries it.

The game is 3D-rendered, but play happens on a plane. Operators will move freely and continuously over the floor
plane, collision is 2D on that plane, and everything placed (Stations, Counters, walls, props) snaps to a 1 m floor
grid. This ticket sets up that frame: a floor grid singleton and a camera authored in the Layout.

**Blocked by:** None (can start immediately).

**Status:** ready-for-agent

- [ ] The Game Target is added from the root CMake. Game code is a static library linking `engine_lib`, a thin
      executable adds the entry point, and a test target links the same library through an `add_signal_room_test`
      helper.
- [ ] The desktop and WASM presets build the Signal Room executable. Assets are copied (desktop) or preloaded (WASM)
      like the sandbox's.
- [ ] `ctest --preset desktop-test-release` runs the Signal Room tests, and the total test count is at least the
      pre-change baseline.
- [ ] One game module registers the game's Level loaders, reflected components and systems. Every game component is
      pre-registered in the module constructor. Physics is not imported.
- [ ] A Room scene (tagged for pipeline filtering) loads a Shift file that names a Layout. The Layout is an engine Level
      JSON holding a `floor_grid` singleton (width, height, 1 m cell size, origin) and a camera entity aimed by a
      `look_at` ref.
- [ ] On boot the game activates the Room scene with a minimal Shift, showing the bare grid floor from the fixed
      camera (~55° pitch, looking north).
- [ ] A headless harness (scene-equivalence `TestWorld` style: `EngineContext` + `NullPlatform`) can load a Shift
      file, press actions by writing bound keys into `InputState`, step N frames at a fixed delta, and query the world.
- [ ] A harness test loads the minimal Shift and checks the floor grid's size. A test that loads a Shift naming a
      missing Layout file fails with that path in the error.
- [ ] An ImGui Room panel (main thread, scoped to the Room scene) lists the Shift files and has a Reload button that
      re-reads the Shift and Layout from disk.
- [ ] Runtime check: the desktop build runs, `flecs-api switch-scene` to the Room scene works, and the browser build
      loads and shows the floor.
