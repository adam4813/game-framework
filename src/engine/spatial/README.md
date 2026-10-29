# Spatial Module

Manages entity position, rotation, and scale in local and world space.

## Structure

| File                      | Purpose                                                      |
|---------------------------|--------------------------------------------------------------|
| `spatial_components.hpp`  | Transform and WorldTransform components.                     |
| `spatial_module.hpp`      | Module class declaration.                                    |
| `spatial_module.cpp`      | Component reflection, transform propagation, level loaders.  |
| `spatial.hpp`             | Public umbrella header.                                      |

## Usage

```cpp
#include "engine/engine.hpp"

// Author Transform (local space) — WorldTransform is derived automatically
entity.set<Transform>({
  .position = {0, 1, 0},
  .rotation = {0, 0, 0},
  .scale = {1, 1, 1}
});

// Query world-space transforms for rendering, physics, or culling
world.system<const WorldTransform>()
  .each([](const WorldTransform& wt) {
    // Use wt.position, wt.rotation, wt.scale, wt.matrix
  });
```

## Registered Systems

**TransformPropagation** (PreStore, GameScene)
- Cascades Transform changes to WorldTransform for parented and root entities.
- Respects parent matrix when computing world-space values.
- Skips rigid bodies (physics backend owns their WorldTransform).

**TransformSeedWorldTransform** (OnSet observer)
- Fires every time Transform is set (OnSet, so an undeferred set never seeds from a default Transform).
- Sets WorldTransform from Transform composed with the parent's WorldTransform, so it is right before the first frame.
- The only Transform -> WorldTransform path for rigid bodies; the physics module moves the body on that set.
