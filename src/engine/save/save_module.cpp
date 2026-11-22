#include "save_module.hpp"

#include <flecs.h>

#include "engine/ecs/ecs.hpp"
#include "save_registry.hpp"

namespace engine::save {

SaveModule::SaveModule(const flecs::world& world) {
	// The registry carries std::function bindings, so it is not reflected for the Explorer; it is a
	// plain owned singleton the game mutates during setup.
	world.set<SaveRegistry>({});

	ecs::Log(world).info("[SaveModule] Registered save registry");
}

} // namespace engine::save
