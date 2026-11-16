#pragma once

#include <flecs.h>

namespace engine::physics {

/// Exposes the physics queries to scripts: `Raycast`, `RaycastAll`, their `RaycastHit` and `RaycastFilter` value
/// types, and the `Layer_*` constants. Does nothing in a world without a script backend. Call after the script-facing
/// value types it uses (`vec3`, `Entity`) are registered, i.e. after the core and scripting modules.
void RegisterPhysicsScriptBindings(const flecs::world& world);

} // namespace engine::physics
