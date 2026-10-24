#pragma once

#include <flecs.h>

namespace crabnado {

// Crabnado's top-level Flecs module: registers the game's scenes and activates the title scene.
// Requires the EngineContext singleton the EngineContext constructor sets on the world.
class CrabnadoModule {
public:
	explicit CrabnadoModule(flecs::world& world);
};

} // namespace crabnado
