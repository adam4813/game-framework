#pragma once

#include <flecs.h>

namespace game {

// Global game run state singleton.
struct GameData {
	bool showDebugMenu = true;
};

// Game module: the game-layer Flecs module.
class GameModule {
public:
	explicit GameModule(flecs::world& world);
};

} // namespace game
