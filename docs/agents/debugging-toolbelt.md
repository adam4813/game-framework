# Debugging toolbelt

The tools for inspecting a running or headless game, and when to use each. `AGENTS.md` points here.

## Running game: Flecs Remote API

The desktop build serves the Flecs REST API on `localhost:27750` (not WASM). A Game Target may bake in its own port,
and `FLECS_REST_PORT=<port>` on launch overrides either; give the tools the same `FLECS_REST_PORT`. Use the `flecs_*` MCP tools or
`node tools/flecs-api.js` (run it with no arguments for its verbs). The `flecs-remote-api` skill covers scene
switching, UI clicks and input overrides.

- **Launch the Debug build** (`cmake --build --preset desktop-debug`, then `build/desktop/games/<game>/Debug/<game>.exe`).
  Flecs asserts only fire in Debug; a Release run reports a clean log while a bad Flecs call goes unnoticed.
  Verify runtime behaviour on Debug, and grep its log for `assert` and `fatal`.
- **After launching, run `node tools/flecs-api.js wait <Leaf>`** with a component only your game registers. It polls
  until the server answers and that component exists. The second check matters: the user often has another game
  instance running, and only one process can hold port 27750. If `wait` times out with "another game on the port?",
  the port belongs to someone else's instance. Leave it running and relaunch yours with a free `FLECS_REST_PORT`.
- **`set` can report `applied: false` when the value did land.** Its read-back misses a value that has already
  moved on (a draining timer) or an entity that has already gone (a Ticket set to expire). Confirm with a `query`
  rather than trusting the flag.
- **Stop only the processes you started**, by PID (`Stop-Process -Id $proc.Id`). Killing by image name
  (`taskkill /IM sandbox.exe`) also kills the user's instances.
- **Query by full dot-path.** `find <Leaf>` prints a component's path (`find Transform` gives
  `engine.spatial.SpatialModule.Transform`). An unresolved name in a query crashes the game.

## Screenshots

`tools/screenshot.ps1` saves a PNG you can read back to check what is drawn:

- **Desktop:** `pwsh tools/screenshot.ps1 -Id $proc.Id -Out shot.png` captures the client area of the process you
  started, at full resolution, even when other windows cover it. `-Process sandbox` picks by name, which can catch
  another instance.
- **Browser (WASM):** `pwsh tools/screenshot.ps1 -Wasm build/wasm/Release -Page sandbox.html -Out web.png` serves
  the build, loads it in headless Chrome, and saves the PNG plus a `.console.log` with the page's console. Grep that
  log for the game's `[info]`/`[error]` lines and for `Aborted`.

- **Desktop on macOS** (no `pwsh`): bring the game to the front, then capture the whole screen and read the PNG back:

  ```bash
  osascript -e 'tell application "System Events" to set frontmost of (first process whose name is "<game>") to true'
  screencapture -x <scratchpad>/shot.png
  ```

  The terminal app needs the Screen Recording permission (System Settings → Privacy & Security). Without it, the
  capture shows only the desktop wallpaper. Prefer the full screen over a `-R x,y,w,h` crop: the region is in points,
  not the PNG's pixels, so a crop guessed from a screenshot misses.

Write screenshots to your scratchpad, not the repo.

## Logs

Each executable writes its log to the directory it was launched from: `game.log` for the sandbox; each Game
Target names its own. Flecs asserts and errors go there too, tagged `[Flecs]`.

## Headless tests

- **Engine scenes:** `tests/scene_equivalence` (`TestWorld` = `EngineContext` + `NullPlatform`).
- `NullPlatform` (`tests/scene_equivalence/null_platform.hpp`) takes an asset directory, so headless loads resolve
  real asset files.

## In-game overlays

- **Backtick** toggles the ImGui debug UI: the Engine panel (FPS, entity count) plus any game panels.
- **F3** toggles debug render overlays.
- **F4** toggles the per-system timing panel in scenes that build it.

## Profiling

The `desktop-profile` preset builds with Tracy and JSONL system-stats telemetry (`stats-telemetry.jsonl`). Open the
JSONL in `tools/stats-viewer.html`.
