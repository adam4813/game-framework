#pragma once

#include <flecs.h>

// The break between Acts. ClearAct (run.hpp) starts it when a boss has retreated; the ActTransition system then walks
// RunState::step: ACT CLEAR for kActClearTime, the comic page (a placeholder until ticket 06 logs its id and
// completes at once), the arena reset and the Act's free molt. Choosing the molt (upgrades.hpp) starts the next Act's
// countdown. After the last Act the comic is the ending and the run is won instead. The scene stays paused throughout,
// so every clock stands still.
namespace crabnado {

// Registers the ActTransition system. Call from the module constructor after RegisterRun and RegisterCampaign.
void RegisterTransition(const flecs::world& world);

// Moves the run into the next Act and resets the arena for it: the rocks are re-scattered with the Act's counts, the
// water re-tinted, the horde and loose shells cleared, the player put back in the centre at full HP and stamina, and
// the crabnado back on its orbit at a new bearing. Molts, upgrades, kills and the campaign clock carry over; the wind
// stays as it is. Does nothing past the last Act.
void StartNextAct(const flecs::world& world);

} // namespace crabnado
