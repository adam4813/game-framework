#pragma once

#include <flecs.h>

namespace engine::tilemap {

// Tilemap module: registers tilemap components and rendering systems
class TilemapModule {
public:
	explicit TilemapModule(const flecs::world& world);
};

} // namespace engine::tilemap
