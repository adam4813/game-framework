# Game Engine — Framework Documentation

The engine is a small, data-driven C++23 game engine built on **Flecs 4.x** (ECS), **Raylib**
(platform/rendering), **Jolt** (physics), **AngelScript** (scripting) and **nlohmann_json**
(data). Everything is a module that a single `EngineContext` imports into one Flecs world; game behaviour is expressed
as **components + systems + data**, not hardcoded branches.

This folder is the framework-level reference. It consolidates and cross-links the per-module
`README.md` files (which remain the authoritative deep-dive for each subsystem).

## Start here

| Doc                                          | What it covers                                                                                                                                                 |
|----------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [architecture.md](architecture.md)           | The big picture: `EngineContext`, module import order, the frame loop, the pipeline **phases**, and the **pause** model.                                       |
| [ecs-components.md](ecs-components.md)       | Core ECS conventions, the shared `Transform`/`WorldTransform`, singletons (`RngState`, refs), tags (`Pausable`, scene tags), and per-module component catalog. |
| [scenes.md](scenes.md)                       | The `Scene` interface and its **lifecycle methods**, scene registration/activation/reload, per-scene pipelines, and the deferred-write hazard.                 |
| [systems-reference.md](systems-reference.md) | Every registered **system** and **observer**, its phase, scene scope, and pausability — one table per module.                                                  |
| [scripting-api.md](scripting-api.md)         | The backend-agnostic **scripting API**: component accessors, singletons, globals, key codes, and the script lifecycle.                                         |
| [ui.md](ui.md)                               | The data-driven **UI components**, factories, layout/interaction/render systems, and scripting/tooling hooks.                                                  |

## Directory structure

```
src/engine/
├── engine_context.{hpp,cpp}  # Owns the world and imports all modules (dependency injection)
├── app/                      # Frame loop: platform begin/end, world.progress() step
├── engine.hpp                # Public header: #include "engine/engine.hpp" for all modules
│
├── core/                     # Shared utilities (math, hashing, etc.)
├── ecs/                      # ECS metadata (components, phases, pausable tag)
├── platform/                 # Platform abstraction interface (Raylib, WASM target)
│
└── <moduleA>/, <moduleB>/, …  # Flecs modules (each imports via world.import<T>())
    ├── <module>_module.{hpp,cpp}   # Module entry point; registers systems & observers
    ├── <module>_components.hpp     # Plain-data component structs
    ├── <module>_*.hpp/cpp          # Helper files (systems, callbacks, loaders)
    └── README.md                   # Deep-dive for the module
```

Each **Flecs module** lives under `src/engine/<name>/` and registers itself into the world via `world.import<T>()`. The
`EngineContext` (`src/engine/engine_context.cpp`) imports all modules in dependency order. The **frame loop**
(`src/engine/app/`) owns `EngineContext`, calls `world.progress(dt)` each tick, and drains the deferred task queue.

| Module      | Kind         | Registers                                                    | README                                                                                                        |
|-------------|--------------|--------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------|
| `assets`    | data/service | `AssetRegistry` singleton; free `Acquire/Get/Release` API    | [assets](../src/engine/assets/README.md)                                                                      |
| `audio`     | simulation   | `SoundEffect`/`Music`, resolve observers, playback system    | [audio](../src/engine/audio/README.md)                                                                        |
| `input`     | simulation   | `InputState` singleton, poll + reset systems                 | [input](../src/engine/input/README.md)                                                                        |
| `level`     | data/service | `LevelRegistry` singleton; JSON → entity tree loaders        | [level](../src/engine/level/README.md)                                                                        |
| `particles` | simulation   | `ParticleEmitter`/`Particle`, emit + update systems          | [particles](../src/engine/particles/README.md)                                                                |
| `physics`   | simulation   | Jolt bodies, step/sync systems, collision events             | [physics](../src/engine/physics/README.md)                                                                    |
| `save`      | data/service | `SaveRegistry` singleton; schema-driven JSON save/load       | [save](../src/engine/save/README.md)                                                                          |
| `scene`     | management   | scene lifecycle observers + tick/input/UI systems            | [scene](../src/engine/scene/README.md)                                                                        |
| `scripting` | runtime      | backend singleton, script lifecycle observers + tick systems | [scripting](../src/engine/scripting/README.md) · [angelscript](../src/engine/scripting/angelscript/README.md) |
| `tilemap`   | simulation   | `TileMap`/`Tile`, tilemap update system                      | [tilemap](../src/engine/tilemap/README.md)                                                                    |
| `timer`     | simulation   | `Timer`/`Cooldown`/`Tween`, three advance systems            | [timer](../src/engine/timer/README.md)                                                                        |
| `render`    | simulation   | primitives, lights, camera, draw systems, resolve observers  | [render](../src/engine/render/README.md)                                                                      |
| `ui`        | simulation   | UI components, layout/interaction/render systems             | [ui](../src/engine/ui/README.md)                                                                              |

## Conventions in one screen

- **Data-driven** — new content = a JSON entry / data-table row, not a new `if`/`else`.
- **Components are plain data** — no logic, no virtual methods; tags are zero-size structs.
- **Systems are stateless** — query components, use `it.delta_time()` for timing.
- **Singletons** via `world.get<T>()` / `world.get_mut<T>()` for global state.
- **Phase vs. scene filter are separate** — order with a single `.kind(phase)`, then
  `.add<scene::YourSceneTag>()` for pipeline filtering. Untagged systems run in **every** scene.
- **Pausing is data** — add `.add<engine::ecs::Pausable>()` to a system to make it stop while the game is paused; omit
  it to keep running (input, rendering, audio, UI).
- **Platform abstraction** — game/engine logic calls the `platform::Platform` interface, never Raylib directly.
- **Game code includes** — import `#include "engine/engine.hpp"` to access all engine modules, components, and
  singletons. The one-way dependency (engine never includes game code) is maintained, making this safe and convenient.
- **Naming** — `snake_case` files, `PascalCase` types/methods/functions, `camelCase` locals/fields.

See the repository [AGENTS.md](../AGENTS.md) for the full authoring guide and build commands.
