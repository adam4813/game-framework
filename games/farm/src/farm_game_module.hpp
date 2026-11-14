#pragma once

#include <flecs.h>

namespace farm {

// Global run-state singleton for the farm game.
struct FarmGameData {
	bool showDebugMenu = true;
};

// Top-level Flecs module for the farm game. Imports required engine subsystems,
// registers farm-specific modules and Scenes, and activates the title Scene.
class FarmGameModule {
public:
	explicit FarmGameModule(flecs::world& world);
};

} // namespace farm
