#include "module_registry.hpp"

#include <spdlog/spdlog.h>

namespace engine::core {

void ModuleRegistry::Import(flecs::world& world, std::string_view name) const {
	const auto* import_fn = entries_.Get(std::string(name));
	if (!import_fn) {
		spdlog::warn("[ModuleRegistry] Unknown module '{}' — skipping import", name);
		return;
	}
	(*import_fn)(world);
}

void ModuleRegistry::Import(flecs::world& world, const std::initializer_list<std::string_view> names) const {
	for (const auto name : names) {
		Import(world, name);
	}
}

void ModuleRegistry::ImportAll(flecs::world& world) {
	for (const auto& import_fn : entries_.items | std::views::values) {
		import_fn(world);
	}
}

} // namespace engine::core
