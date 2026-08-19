---
name: create-module
create-module description: >
  Scaffold a new engine module (a Flecs 4.x module) for this engine. Use this skill whenever asked to create, add,
  or scaffold a new engine subsystem/module — e.g. "create a render module", "add an audio module", "scaffold a new
  module". Covers the folder layout, the module class + world.import<T>() pattern, component reflection, system
  registration, scene scoping, platform access, the README, and how to register the module in EngineContext. It pairs with
  the flecs-systems skill (use that for the system bodies themselves).
---

# Create an Engine Module

A **module** is a self-contained subsystem under `src/engine/<name>/` that registers its components and Flecs systems
through a single class Flecs imports with `world.import<T>()`.

Existing references: `src/engine/physics/`, `src/engine/input/`, `src/engine/scene/`.

**Always also use the `flecs-systems` skill** when writing the system bodies — it has the correct Flecs 4.x `each()` /
`run()` / observer / phase API. This skill covers the *module scaffolding*; that skill covers the *systems inside it*.

## 1. Folder layout

Create `src/engine/<name>/` with (one concern per file, `snake_case` filenames):

| File                    | Purpose                                                     |
|-------------------------|-------------------------------------------------------------|
| `<name>_components.hpp` | Plain-data ECS components & tags for this module. No logic. |
| `<name>_module.hpp`     | The module class declaration.                               |
| `<name>_module.cpp`     | Component reflection + system/observer registration.        |
| `<name>.hpp`            | Public umbrella header (includes components + module).      |
| `README.md`             | Short doc: structure, usage, systems registered.            |

Extra concrete backends/helpers get their own `*.hpp/.cpp` (see `jolt_physics_system.*`). CMake uses `GLOB_RECURSE` on
`src/*.cpp`, so **new `.cpp` files are auto-discovered** — no CMake edits needed. Re-run `cmake --preset release` only
if a brand-new file isn't picked up.

## 2. Components header — `<name>_components.hpp`

Components are **plain data structs** — no methods with logic, no virtuals. Tags are zero-size structs. Keep everything
in the `engine::<name>` namespace.

```cpp
#pragma once

#include <glm/glm.hpp>

namespace engine::<name> {

// Plain-data component.
struct MyComponent {
    glm::vec3 value{0.0F};
    float amount{1.0F};
};

// Zero-size tag for filtering.
struct MyTag {};

} // namespace engine::<name>
```

Data-driven design: prefer new components/data over `if (type == "foo")` branches.

## 3. Module class — `<name>_module.hpp`

The module is a class whose **constructor takes `flecs::world&`** and does all registration. That constructor signature
is what makes `world.import<MyModule>()` work.

```cpp
#pragma once

#include <flecs.h>

namespace engine::<name> {

/// <Name> module for Flecs ECS integration.
/// Registers <name> components and systems.
class <Name>Module {

public:
explicit <Name>Module (flecs::world& world); };

} // namespace engine::<name>
```

Use `const flecs::world&` only if you never mutate singletons at construction (see
`InputModule`); use `flecs::world&` if you emplace/set singletons (see `PhysicsModule`).

## 4. Module implementation — `<name>_module.cpp`

Order of work inside the constructor:

1. Create singletons the module owns: `world.set<MyState>({});`
2. Reflect components (Flecs meta) so they show up in the Flecs Explorer and, optionally, expose them to scripting.
   `glm::vec3` is already reflected as `vec3` in `EngineContext`.
3. Register observers (event-driven) and systems (per-frame). **Follow `flecs-systems`.**
4. `spdlog::info("[<Name>Module] Registered ...");`

```cpp
#include "<name>_module.hpp"

#include <spdlog/spdlog.h>
#include <flecs.h>
#include <glm/glm.hpp>

#include "engine/ecs/ecs.hpp"
#include "engine/platform/platform.hpp"
#include "engine/scene/scene_components.hpp"
#include "engine/scripting/scripting_module.hpp"
#include "<name>_components.hpp"

namespace engine::<name> {

<Name>Module::<Name>Module(flecs::world& world) {
  // (1) Singletons
  world.set<MyState>({});

  // (2) Reflection — optional but recommended for debugging via the Explorer.
  world.component<MyComponent>().member<glm::vec3>("value").member<float>("amount");
  // RegisterComponentForScripts is a no-op when no scripting backend is present.
  scripting::RegisterComponentForScripts(world, world.component<MyComponent>());

  // Grab the platform once if the module renders/plays audio (pointer is stable).

  // (3) Systems — see the flecs-systems skill for the correct API.
  // Scope per-scene by calling .add<scene::YourSceneTag>() on the returned system entity.
  // Use a single .kind(phase) for ordering — never chain two .kind() calls (the second
  // overwrites the first). Pick the phase:
  //   flecs::PreUpdate  — input reading
  //   flecs::OnUpdate   — game logic, physics, AI
  //   flecs::OnStore    — 3D rendering (platform draw calls inside BeginMode3D/EndMode3D)
  //   ecs::OnUI         — 2D overlays, HUD, buttons, debug panels (always after OnStore)
  // Omit .add<scene::*>() to run the system in every scene pipeline.
  world.system<const ecs::WorldTransform, const MyComponent>("MySystemThatUpdatesComponentBasedOnTransform")
    .kind(flecs::OnStore)
    .each([](const flecs::iter& it, size_t, const ecs::WorldTransform& wt, const MyComponent& c) {
      auto* platform = it.world().get<platform::PlatformRef>().ptr;
      // ... use platform + component ...
    })
    .add<ecs::Pausable>()          // omit if this system should keep running while paused
    .add<scene::YourSceneTag>();   // replace with the scene(s) this system belongs to

  spdlog::info("[<Name>Module] Registered <name> systems with Flecs");
}

} // namespace engine::<name>
```

### Conventions to honor

- **Never call Raylib directly** — go through the `platform::Platform` interface. If the module needs a capability the
  interface lacks, add a method to `platform.hpp` and implement it in `raylib_platform.*` (keep Raylib headers inside
  the
  `.cpp`).
- Components are plain data; systems are stateless functions; singletons via
  `world.get<T>()` / `world.get_mut<T>()`.
- Scope scene-specific systems with `.add<scene::YourSceneTag>()` on the returned system entity. Use a single
  `.kind(phase)` for ordering — never chain two `.kind()` calls. Untagged systems run in every scene pipeline.
- `PascalCase` types/functions, `camelCase` locals/fields, tabs not spaces (see
  `.clang-format` / `.editorconfig`).

## 5. Public umbrella header — `<name>.hpp`

```cpp
#pragma once

#include "<name>_components.hpp"
#include "<name>_module.hpp"
```

This module header is included by the centralized `src/engine/engine.hpp`, which game code can import directly (see
**Include Engine Headers in Game Code** below). Add the module's umbrella header to `engine.hpp`.

## 6. Register the module in `EngineContext`

In `src/engine/engine_context.cpp`, add the include and import it alongside the others.

Order matters: scripting is imported first so modules can expose components to scripts; import your module after any
module it depends on.

```cpp
#include "<name>/<name>_module.hpp"          // near the other module includes ...

world_.import<<namespace>::<Name>Module>(); // near the other world_.import<> calls

```

## 7. README — `src/engine/<name>/README.md`

Mirror `src/engine/physics/README.md`: a one-line summary, a **Structure** file list, a **Usage** snippet, an **Adding X
to an entity** snippet, and the list of registered systems grouped by kind (per-entity `each`, global `run`, observers).

## 8. Build & verify

```powershell
cmake --preset desktop
cmake --build --preset desktop-release
```

New `.cpp` files are auto-globbed; the first `--preset desktop` reconfigure regenerates the file list. Then optionally
inspect the live ECS via `tools/flecs-api.js` to confirm your components/systems registered.

## Include Engine Headers in Game Code

Game code (scenes, entities, game-specific systems) should import the centralized engine API:

```cpp
#include "engine/engine.hpp"  // Provides all engine modules, components, and singletons
```

This single header is safe for prototyping because the dependency is **one-way**: engine code never includes game code.
New modules added to `engine.hpp` are automatically available to all game code with zero additional includes needed.
