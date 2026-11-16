#include "physics_queries.hpp"

#include "jolt/jolt_backend.hpp"

namespace engine::physics {

std::optional<RaycastResult> Raycast(const flecs::world& world, const Ray& ray, const RaycastFilter& filter) {
	const auto* backend = world.try_get<JoltBackend>();
	return backend ? backend->Raycast(ray, filter) : std::nullopt;
}

std::vector<RaycastResult> RaycastAll(const flecs::world& world, const Ray& ray, const RaycastFilter& filter) {
	const auto* backend = world.try_get<JoltBackend>();
	return backend ? backend->RaycastAll(ray, filter) : std::vector<RaycastResult>{};
}

} // namespace engine::physics
