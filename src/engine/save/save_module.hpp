#pragma once

#include <flecs.h>

namespace engine::save {

// Save module for Flecs ECS integration.
// Registers the SaveRegistry singleton, the schema that game code fills in to declare exactly what
// is persisted. The module registers no systems: saving/loading is invoked explicitly by game code
// (e.g. from a menu action) via SaveToJson / LoadFromJson in save_registry.hpp.
class SaveModule {
public:
	explicit SaveModule(const flecs::world& world);
};

} // namespace engine::save
