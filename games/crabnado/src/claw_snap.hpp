#pragma once

#include <flecs.h>

// Claw snap: the player's always-on weapon. On a cooldown it damages every landed enemy within its
// radius and flashes a ring that shows the reach.
namespace crabnado {

// On the player. `cooldown` is the stat upgrades change; the running countdown to the next snap is the
// `timer::Cooldown` on the player's child named kClawCooldownName, restarted from `cooldown` on each snap. `level` is
// what the HUD shows: 1, plus one per claw upgrade taken (the upgrades raise it in data).
struct ClawSnap {
	float damage{0.0F};
	float radius{0.0F};
	float cooldown{0.0F};
	float level{1.0F};
};

// Names of the player's children that carry the claw's `timer::Cooldown`s: the snap countdown and the ring's display.
inline constexpr const char* kClawCooldownName = "ClawCooldown";
inline constexpr const char* kClawRingName = "ClawRing";

// Tag on the ring entity, a child of the entity with ClawSnap. Its `timer::Cooldown` counts down the ring's display.
struct ClawRing {};

// Registers and reflects the claw snap components and the ClawSnapFire and ClawRingShow systems.
void RegisterClawSnap(const flecs::world& world);

// Gives `player` the starting claw snap and its ring.
void AddClawSnap(const flecs::world& world, flecs::entity player);

} // namespace crabnado
