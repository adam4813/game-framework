#pragma once

#include <flecs.h>

namespace engine::tilemap {

class TileCallbackSystem {
public:
	// Register the tile callback system with Flecs
	// This system fires onenter/within/onleave callbacks as entities move across tiles
	static void Register(const flecs::world& world);
};

} // namespace engine::tilemap
