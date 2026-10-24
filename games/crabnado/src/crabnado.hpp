#pragma once

#include <glm/glm.hpp>

#include <flecs.h>

// The crabnado in its normal phase: it roams the arena interior and throws crabs at the player.
namespace crabnado {

// The crabnado travels toward `waypoint` at `speed`, picking a new one inside kCrabnadoRoamFraction of
// the arena radius, biased downwind, when it arrives. `spawn_debt` accumulates the spawn rate; each whole unit is one throw.
struct Crabnado {
	glm::vec2 waypoint{0.0F};
	float speed{0.0F};
	float spawn_debt{0.0F};
	glm::vec2 knock{0.0F}; // the boss phase's wave knock, in units per second
};

// Relationship: (ThrowsInto, horde root). Thrown enemies become children of the target, so the scene that
// owns the horde root destroys them.
struct ThrowsInto {};

// Registers and reflects the crabnado components and the CrabnadoRoam and CrabnadoThrow systems.
void RegisterCrabnado(const flecs::world& world);

// Spawns the crabnado at `position` as a child of `parent`, throwing enemies into `horde`.
flecs::entity SpawnCrabnado(const flecs::world& world, flecs::entity parent, flecs::entity horde, glm::vec2 position);

// Throws up to `count` crabs at the player at once, stopping at the horde cap. The debug spawn control
// uses it.
void ThrowBurst(flecs::entity crabnado, int count);

} // namespace crabnado
