#pragma once

#include <flecs.h>

namespace game::football {

// Load all play + consumable definitions from games/football/assets/data/ into their registries and set
// them on the world as singletons. Returns the number of plays loaded.
int LoadContent(const flecs::world& world);

// Reset the run to its starting state: default GameMode, fresh FootballState, the starting playbook
// + consumable inventory, and cleared outcome / shop singletons. Called on scene load and "Play
// Again".
void InitRun(const flecs::world& world);

} // namespace game::football
