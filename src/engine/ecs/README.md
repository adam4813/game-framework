# ECS Module

Engine-wide ECS vocabulary shared by every module, plus the Flecs Remote API setup. It holds no Flecs module class of
its own.

## Structure

- **ecs_types.hpp** — The engine's custom pipeline phases, `OnUI` (2D overlays) and `OnDebugUI` (the debug UI).
- **pausable.hpp** — The `Pausable` system tag that paused pipelines filter out (ADR 0004).
- **singletons.hpp** — Global run state singletons, such as the deterministic `RngState`.
- **flecs_remote.hpp / .cpp** — `InitializeRemoteAPI`, which starts the REST server the Flecs Explorer,
  `tools/flecs-api.js` and the `flecs_*` MCP tools talk to. Desktop only: the REST server needs TCP sockets, so games
  compile the call out under `__EMSCRIPTEN__`.
- **ecs.hpp** — Index header for the vocabulary (not `flecs_remote.hpp`, which games include where they call it).

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
