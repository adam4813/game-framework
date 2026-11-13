#pragma once

#include <flecs.h>

// Riptide: the player's third weapon, unlocked by an upgrade. On its cooldown it raises the next crest of the
// primary swell through the wave field; it never pushes enemies itself.
namespace crabnado {

// On the player, always present. `level` is 0 while locked; upgrades raise it. `timer` counts down to the next
// fire while unlocked, `multiplier` scales the boosted crest.
struct Riptide {
	float level{0.0F};
	float cooldown{0.0F};
	float multiplier{0.0F};
	float timer{0.0F};
};

// Registers and reflects Riptide and the RiptideFire system.
void RegisterRiptide(const flecs::world& world);

// Gives `player` the locked Riptide.
void AddRiptide(flecs::entity player);

} // namespace crabnado
