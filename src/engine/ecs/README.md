# ECS Module

Engine-wide ECS vocabulary shared by every module, plus the Flecs Remote API setup. It holds no Flecs module class of
its own.

## Structure

- **ecs_types.hpp** — The engine's custom pipeline phases, `OnUI` (2D overlays) and `OnDebugUI` (the debug UI).
- **pausable.hpp** — The `Pausable` system tag that paused pipelines filter out (ADR 0004).
- **pipelines.hpp / .cpp** — The `FixedStep` and `FrameSample` system tags and the pipeline builders scenes start
  from: `FramePipeline` (every phased system but those two), `FixedStepPipeline` and `FrameSamplePipeline`. The fixed
  step that runs them is `engine/fixed_step/` (ADR 0008).
- **singletons.hpp** — Global run state singletons, such as the deterministic `RngState`.
- **flecs_remote.hpp / .cpp** — `InitializeRemoteAPI`, which starts the REST server the Flecs Explorer,
  `tools/flecs-api.js` and the `flecs_*` MCP tools talk to. Desktop only: the REST server needs TCP sockets, so games
  compile the call out under `__EMSCRIPTEN__`.
- **world_logger.hpp / .cpp** — The `WorldLogger` singleton, `SetWorldLogger` and the `WorldLog` accessor: a world's
  own named logger on the default logger's sinks.
- **ecs.hpp** — Index header for the vocabulary (not `flecs_remote.hpp`, which games include where they call it).

## World logger

```cpp
#include "engine/ecs/world_logger.hpp"

// After the game sets spdlog's default logger (its sinks and pattern), name each world.
engine::ecs::SetWorldLogger(client_world, "client");
engine::ecs::SetWorldLogger(server_world, "server");

// Code holding a world, a system included (pass it.world()), logs through that world's logger.
engine::ecs::WorldLog(world).info("Player {} joined", id);
// [2026-11-12 22:07:06.511] [server] [info] Player 3 joined
```

- **Shared sinks.** A world's logger writes to the default logger's sinks, so both worlds' lines land interleaved, in
  order, in the same console and file. It takes the default logger's level and flush level when it is set.
- **The name shows through the sinks' pattern** (`%n`, in spdlog's default pattern). Sinks own the formatting and are
  shared with the default logger, so set a pattern on the default logger, never on a world's logger.
- **No world in reach, no change.** `WorldLog` returns the default logger for a world without one, and code with no
  world keeps calling `spdlog::info`. Flecs' own messages, routed through the game's `os_api.log_` handler, stay
  unlabeled.
- **Registration.** EngineContext registers `WorldLogger`, so `WorldLog` is safe in a multi-threaded system. A bare
  `flecs::world` registers it first with `world.component<WorldLogger>()`.

## Remote API

```cpp
#include "engine/ecs/flecs_remote.hpp"

// Serve on 27750, or on FLECS_REST_PORT when it is set, so several game instances can run side by side.
engine::ecs::InitializeRemoteAPI(world);

// A game's own port, still overridable by FLECS_REST_PORT.
engine::ecs::InitializeRemoteAPI(world, 27751);

// Two worlds in one process (say a client and an integrated server): FLECS_REST_PORT can name only one port, so let it
// override one world and serve the other on its given port whatever the variable says.
engine::ecs::InitializeRemoteAPI(client_world, 27750);
engine::ecs::InitializeRemoteAPI(server_world, 27760, engine::ecs::RestPortEnv::Ignore);
```

A `FLECS_REST_PORT` that isn't a port number (1–65535) is ignored with a warning, and the given port is used.

`InitializeRemoteAPI` also imports `flecs::stats` for the Explorer, but disables the stats addon's pipeline monitors:
they assert when a scene switch leaves a scene's custom pipeline inactive, and nothing here reads pipeline stats.
