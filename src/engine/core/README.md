# Core Module

Flecs module registering foundational types, math types, and value types used throughout the engine.

## Structure

- **core_types.hpp** — Plain-data types: `Rect` (axis-aligned rectangle), `Rgba` (8-bit RGBA color).
- **core_module.hpp** — `CoreModule` class declaration (Flecs module entry point).
- **core_module.cpp** — Constructor: reflects core types and math types for the Flecs Explorer, registers value types
  and components with the scripting backend, and registers constructors and global constants.
- **core.hpp** — Umbrella header; included by `engine.hpp`.

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

