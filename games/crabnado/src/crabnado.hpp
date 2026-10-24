#pragma once

#include <glm/glm.hpp>

#include "engine/core/core.hpp"

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

// The arena crabnado's storm: `storm` rises from 0 to 1 over the boss wind-up and stays 1 through the boss,
// darkening the funnel and making it spin and sway harder. `flash` jumps to 1 on each slam and fades, paling the
// funnel. `slams_seen` is the RunState::slams already flashed for. The title's crabnado has none, so it stays calm.
struct FunnelStorm {
	float storm{0.0F};
	float flash{0.0F};
	int slams_seen{0};
};

// A funnel part the storm darkens: its calm colour, which FunnelTint blends toward storm grey.
struct FunnelTint {
	engine::core::Rgba base{};
};

// One ring of the funnel: a pivot its cloud tubes, puffs and specks hang from. The FunnelSway system sways and
// twists each ring with a phase that lags further behind the higher it is (`lift`, 0 at the base to 1 at the top),
// so the top drifts out of sync with the bottom and a wave runs up the funnel. `time` is its accumulated seconds.
struct FunnelRing {
	float lift{0.0F};
	float time{0.0F};
};

// Relationship: (ThrowsInto, horde root). Thrown enemies become children of the target, so the scene that
// owns the horde root destroys them.
struct ThrowsInto {};

// Registers and reflects the crabnado components and the CrabnadoRoam, CrabnadoThrow, CrabnadoStorm, FunnelSway and
// FunnelTint systems.
void RegisterCrabnado(const flecs::world& world);

// Adds the crabnado's look (a wispy funnel of cloud tubes and puffs with crabs caught in it) as children of `crabnado`.
void AddFunnel(const flecs::world& world, flecs::entity crabnado);

// Spawns the crabnado at `position` as a child of `parent`, throwing enemies into `horde`.
flecs::entity SpawnCrabnado(const flecs::world& world, flecs::entity parent, flecs::entity horde, glm::vec2 position);

// Throws up to `count` crabs at the player at once, stopping at the horde cap. The debug spawn control
// uses it.
void ThrowBurst(flecs::entity crabnado, int count);

} // namespace crabnado
