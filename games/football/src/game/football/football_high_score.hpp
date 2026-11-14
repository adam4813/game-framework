#pragma once

#include <flecs.h>

namespace game::football {

// Registers HighScore with the save schema and restores it from the platform save slot, if one exists.
// Precondition: the engine "save" module is imported and HighScore is registered (FootballModule).
void LoadHighScore(flecs::world& world);

// Records a finished run's score. When it beats the stored best, updates the HighScore singleton,
// persists it, and returns true.
bool RecordScore(const flecs::world& world, int points);

} // namespace game::football
