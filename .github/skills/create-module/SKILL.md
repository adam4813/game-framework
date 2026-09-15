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
#include "engine/core/core.hpp"
#include "engine/scene/scene_components.hpp"
#include "engine/scripting/scripting_module.hpp"

#include "<name>_components.hpp"

namespace engine::<name> {

<Name>Module::<Name>Module(flecs::world& world) {
  // (1) Singletons
  world.set<MyState>({});

  // (2) Reflection — ALWAYS register every component type your module owns via world.component<T>(),
  // even tags and event-only components with no meta fields. Without explicit registration, the
  // type only exists when game code first calls entity.add<T>() or entity.set<T>(). In contexts
  // where that code path never runs (e.g. the headless scene-equivalence test), any query or
  // observer referencing that component will fail or produce wrong results.
  //
  // Preferred — sequential template deduction: short, readable, correct when every struct field
  // from the first registered to the last is listed in declaration order with no gaps:
  world.component<MyComponent>().member<glm::vec3>("value").member<float>("amount");
  world.component<MyTag>();    // zero-size tags still need explicit registration
  //
  // Fallback — pointer-to-member syntax: use this whenever ANY field between the first and
  // last registered member is skipped (functions, std::function callbacks, complex STL
  // containers, etc.). Sequential tracking would compute wrong offsets in those cases, causing
  // garbage reads in the Flecs Explorer, bad serialization, and crashes in entity.to_json():
  //
  //   world.component<MyComp>()
  //       .member("value",  &MyComp::value)   // safe — offset is always exact
  //       .member("amount", &MyComp::amount);
  //
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

- Components are usually plain data; systems are stateless functions; singletons via
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

## 8. Scene equivalence test — `tests/scene_equivalence/`

The scene equivalence test (`tests/scene_equivalence/scene_equivalence_test`) verifies that the
same scene produces identical authored component values when built through JSON, AngelScript,
and C++. **Every authored (non-transient, non-singleton) component with meta-registered fields
should be covered.** After creating a new module, update the test in five places:

### 8a. Import the module in `TestWorld` (`main.cpp`)

Add the include and `world.import<>()` call alongside the others in the `TestWorld` constructor:

```cpp
#include "engine/<name>/<name>.hpp"   // top of main.cpp

// Inside TestWorld constructor:
world.import<engine::<name>::<Name>Module>();
```

### 8b. Add test entities in `BuildCppWorld()` (`main.cpp`)

For each authored component with meta-registered fields, add one named entity as a
`child_of(scene_root)`. **Use fully-explicit designated initializers** for every meta-registered
field — do not rely on struct defaults, since C++ designated init zero-initializes unspecified
fields while JSON and AS paths use the struct's own defaults.

```cpp
test_world.world.entity()
    .child_of(scene_root)
    .set_name("Test<ComponentName>")
    .set<engine::<name>::MyComponent>({
        .value = {1.0F, 2.0F, 3.0F},
        .amount = 0.5F,
    });
```

### 8c. Add JSON entities to `kTestSceneJson` (`scene_data.hpp`)

Add a new entity for each test component using the JSON loader key registered in the module
via `level::RegisterComponentLoader(world, "<key>", ...)`:

```json
{
  "name": "Test<ComponentName>",
  "components": {
    "<loader_key>": {
      "value": [1.0, 2.0, 3.0],
      "amount": 0.5
    }
  }
}
```

### 8d. Add AngelScript entities to `kTestSceneScript` (`scene_data.hpp`)

In the `OnInit` function body, create the entity and set its components via the typed component
handle. Use **direct handle property assignment** — this works for all field types regardless of
component scope:

```angelscript
Entity my_ent = CreateNamedEntity("Test<ComponentName>");
MyComponent@ mc = my_ent.AddMyComponent();
mc.value = vec3(1.0f, 2.0f, 3.0f);
mc.amount = 0.5f;
```

- `std::string` fields: assign directly via the handle property — `mc.path = "./some/path"`.
- `Rgba` fields: use the `MakeRgba(r, g, b, a)` global registered by the test setup.
- Skip fields that are runtime-only (asset handles, per-frame state) — they are excluded from
  comparison automatically (handle fields are skipped; components serializing to `null` are
  skipped).

### 8e. Update `COMPONENT_COVERAGE.md` (`tests/scene_equivalence/`)

Add a row for every new component to
`tests/scene_equivalence/COMPONENT_COVERAGE.md` using the legend at the top of that file (✅ Tested, ⚠️ Partial, 🚫 Skip,
🔧 TODO, 🐛 Bug).

### 8f. Verify

```powershell
cmake --build --preset desktop-release --target scene_equivalence_test
cd build\desktop\tests\scene_equivalence\Release
.\scene_equivalence_test.exe
```

The test must exit 0 with **0 mismatches**.

## 9. Build & verify

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
