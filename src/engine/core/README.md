# Core Module

Flecs module registering foundational types, math types, and value types used throughout the engine.

## Structure

- **core_types.hpp** — Plain-data types: `Rect` (axis-aligned rectangle), `Rgba` (8-bit RGBA color).
- **core_module.hpp** — `CoreModule` class declaration (Flecs module entry point).
- **core_module.cpp** — Constructor: reflects core types and math types for the Flecs Explorer, registers value types
  and components with the scripting backend, and registers constructors and global constants.
- **json_util.hpp** — `LoadJsonFile`, `LoadJsonDirectory` and the `J*` field parsers (`JVec3`, `JRgba`, ...).
- **json_file_ref.hpp/.cpp** — File References: `kFileRefSigil`, `IsFileRef`, `LoadFileRef`, `LoadJsonFileResolved`.
- **core.hpp** — Umbrella header; included by `engine.hpp`.

## File References

A JSON document can be split across files: the string `"@file:<path>"` stands in for the whole document at `<path>`.
Levels use them (`docs/architecture.md`, "File references in levels"), and so can any other JSON a game stores in
pieces, such as a save or a config.

- **Path rule**: `<path>` resolves against the directory of the file that holds the reference, not the working
  directory or the root file, so a nested file's own references keep working when its folder moves.
- **Only the sigil makes a reference.** A bare string, even one that names a file, is a plain value. Object keys are
  never references.
- `LoadJsonFileResolved(path, log_tag)` loads a file and replaces every reference in it, at any depth (an object
  member, an array element, inside a referenced file). It returns `std::nullopt` and logs when a file is missing or
  unparsable, or when a reference cycles back to a file it is already inside.
- `IsFileRef(value)` and `kFileRefSigil` let a writer emit references the reader resolves:
  `std::string{core::kFileRefSigil} + "world/chunks.json"`.
- `LoadFileRef(ref, base_dir, log_tag)` loads one reference without resolving inside it, and returns the directory its
  own references resolve against. The level loader builds on it, since it resolves references as it builds entities
  (only in entity arrays and component values) and skips one bad reference instead of failing the whole level.

```cpp
// save/world.json: {"player": "@file:player.json", "chunks": ["@file:chunks/0.json"]}
const auto save = engine::core::LoadJsonFileResolved("save/world.json", "Save");
if (save) {
    const int hp = (*save)["player"]["hp"].get<int>();
}
```

## Usage

The core module is **imported first** in `EngineContext` so that all other modules can safely use core types in their
components and registrations.

```cpp
// Core types are registered automatically; no direct usage needed in game code.
// However, they are accessible via the engine API:

#include "engine/engine.hpp"

// Use math types
glm::vec2 pos{10.0F, 20.0F};
glm::vec3 up = engine::core::GetVec3_Up();  // Global constant (via scripting)

// Use geometric types
auto rect = engine::platform::Rect{0.0F, 0.0F, 100.0F, 50.0F};
if (rect.Contains({50.0F, 25.0F})) { /* inside */ }

// Use color types
auto panel_color = engine::platform::colors::Panel;  // Predefined palette
auto custom = engine::core::Rgba{255, 128, 64, 255};  // RGBA color
```

### In Scripts (AngelScript)

```angelscript
// Vec2, Vec3, Vec4 value types with constructors and global constants
auto pos = vec3(1.0, 2.0, 3.0);
auto right = Vec3_Right;    // (1, 0, 0)
auto up    = Vec3_Up;       // (0, 1, 0)
auto fwd   = Vec3_Forward;  // (0, 0, -1)

// Rgba colors
auto red = Rgba();  // default (0, 0, 0, 255)
red.r = 255;

// Transform components (used in levels and entities)
entity.set<Transform>({
    .position = vec3(0.0, 0.0, 0.0),
    .rotation = vec3(0.0, 0.0, 0.0),
    .scale    = vec3(1.0, 1.0, 1.0),
});
```

## Registered Components & Types

| Type             | Members                         | Purpose                               |
|------------------|---------------------------------|---------------------------------------|
| `glm::vec2`      | `x`, `y`                        | 2D vector                             |
| `glm::vec3`      | `x`, `y`, `z`                   | 3D vector                             |
| `glm::vec4`      | `x`, `y`, `z`, `w`              | 4D vector / homogeneous coordinates   |
| `Rgba`           | `r`, `g`, `b`, `a`              | 8-bit RGBA color                      |
| `Transform`      | `position`, `rotation`, `scale` | Local-space transform (ECS component) |
| `WorldTransform` | `position`, `rotation`, `scale` | World-space transform (ECS component) |

## Registered Global Constants (Scripts)

| Name           | Value        | Use                                 |
|----------------|--------------|-------------------------------------|
| `Vec3_Right`   | `(1, 0, 0)`  | +X axis                             |
| `Vec3_Up`      | `(0, 1, 0)`  | +Y axis                             |
| `Vec3_Forward` | `(0, 0, -1)` | -Z axis (forward in RH coordinates) |

