#pragma once

#include <flecs.h>

namespace game {

// Game module: the game-layer Flecs module.
class GameModule {
public:
	explicit GameModule(flecs::world& world);
};

} // namespace game
