#pragma once

#include <flecs.h>

// Claw snap: the player's always-on weapon. On a cooldown it damages every landed enemy within its
// radius and flashes a ring that shows the reach.
namespace crabnado {

// On the player. `timer` counts down to the next snap; `ring_timer` counts down the ring's display. `level` is
// what the HUD shows: 1, plus one per claw upgrade taken (the upgrades raise it in data).
struct ClawSnap {
	float damage{0.0F};
	float radius{0.0F};
	float cooldown{0.0F};
	float timer{0.0F};
	float ring_timer{0.0F};
	float level{1.0F};
};

// Tag on the ring entity, a child of the entity with ClawSnap.
struct ClawRing {};

// Registers and reflects the claw snap components and the ClawSnapFire and ClawRingShow systems.
void RegisterClawSnap(const flecs::world& world);

// Gives `player` the starting claw snap and its ring.
void AddClawSnap(const flecs::world& world, flecs::entity player);

} // namespace crabnado
