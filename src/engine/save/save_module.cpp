#include "save_module.hpp"

#include <spdlog/spdlog.h>

#include <flecs.h>

#include "save_registry.hpp"

namespace engine::save {

SaveModule::SaveModule(const flecs::world& world) {
	// The registry carries std::function bindings, so it is not reflected for the Explorer; it is a
	// plain owned singleton the game mutates during setup.
	world.set<SaveRegistry>({});

	spdlog::info("[SaveModule] Registered save registry");
}

} // namespace engine::save
