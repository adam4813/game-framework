#pragma once

#include <flecs.h>

// Riptide: the player's third weapon, unlocked by an upgrade. On its cooldown it raises the next crest of the
// primary swell through the wave field; it never pushes enemies itself.
namespace crabnado {

// Name of the player's child that carries Riptide's `timer::Cooldown`.
inline constexpr const char* kRiptideCooldownName = "RiptideCooldown";

// On the player, always present. `level` is 0 while locked; upgrades raise it. `cooldown` is the stat upgrades change;
// the countdown to the next fire is the `timer::Cooldown` on the player's child named kRiptideCooldownName, held at
// the first delay while locked. `multiplier` scales the boosted crest.
struct Riptide {
	float level{0.0F};
	float cooldown{0.0F};
	float multiplier{0.0F};
};

// Registers and reflects Riptide and the RiptideFire system.
void RegisterRiptide(const flecs::world& world);

// Gives `player` the locked Riptide.
void AddRiptide(const flecs::world& world, flecs::entity player);

} // namespace crabnado
