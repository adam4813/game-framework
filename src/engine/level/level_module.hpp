#pragma once

#include <string_view>

#include <flecs.h>

#include "level_components.hpp"

namespace engine::level {

// Level module: owns the LevelRegistry and registers loaders for the built-in engine components
// (transforms, render primitives/materials/lights, physics bodies, audio, scripts). Game code adds
// its own component loaders, then calls LoadLevel to instantiate a JSON level into the world.
class LevelModule {
public:
	explicit LevelModule(const flecs::world& world);
};

// Register (or overwrite) a loader for a component name. `name` must match the JSON key used under
// an entity's "components" object.
void RegisterComponentLoader(const flecs::world& world, std::string_view name, ComponentLoader loader);

// Register (or overwrite) a loader for a world singleton, keyed by its JSON name under "singletons".
void RegisterSingletonLoader(const flecs::world& world, std::string_view name, SingletonLoader loader);

// Parse a level JSON file and instantiate its singletons + entity tree into the world. Missing
// files or a malformed document are logged and abort the load; unknown component names are logged
// and skipped. Returns the number of top-level entities created (-1 on load failure).
int LoadLevel(const flecs::world& world, std::string_view path);

} // namespace engine::level
