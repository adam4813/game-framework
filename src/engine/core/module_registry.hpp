#pragma once

#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include <flecs.h>

#include "registry.hpp"

namespace engine::core {

// Registry of optional engine modules backed by core::Registry<ImportFn>. Owned by EngineContext
// and populated during its construction; game code calls Import() to opt into the subsystems it
// needs. Flecs import is idempotent, so importing an already-imported module is safe. An entry's
// import function may pull in dependencies (e.g. the "ai" entry imports graph first).
class ModuleRegistry {
public:
	using ImportFn = std::function<void(flecs::world&)>;

	// Register a named module import.
	void Register(std::string name, ImportFn import_fn) { entries_.Add(std::move(name), std::move(import_fn)); }

	// Import one registered module (no-op with a warning when name is unknown).
	void Import(flecs::world& world, std::string_view name) const;

	// Import a specific set of named modules, in order.
	void Import(flecs::world& world, std::initializer_list<std::string_view> names) const;

	// Import all registered modules.
	void ImportAll(flecs::world& world);

	// Sorted list of registered module names — handy for tooling / debug output.
	[[nodiscard]] std::vector<std::string> Names() const { return entries_.Ids(); }

private:
	Registry<ImportFn> entries_;
};

} // namespace engine::core
