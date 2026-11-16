#pragma once

#include <optional>
#include <vector>

#include <flecs.h>

#include "physics_types.hpp"

namespace engine::physics {

// Queries against a world's physics bodies (and character inner bodies), as of the last finished Tick: no
// interpolation, though a body teleported since (its Transform set) is queried where it was moved to. Call them from
// any system or from outside `progress()`, but not from inside `PhysicsStep`: a query made while the world steps logs
// an error and hits nothing. A world without physics hits nothing.

/// The closest hit along `ray` that `filter` allows. A ray starting inside a convex shape hits it at distance 0, with
/// the normal facing back along the ray.
[[nodiscard]] std::optional<RaycastResult>
Raycast(const flecs::world& world, const Ray& ray, const RaycastFilter& filter = {});

/// Every entity hit along `ray` that `filter` allows, one hit (the nearest) per body, sorted by distance.
[[nodiscard]] std::vector<RaycastResult>
RaycastAll(const flecs::world& world, const Ray& ray, const RaycastFilter& filter = {});

} // namespace engine::physics
