#pragma once

#include <glm/glm.hpp>

#include "engine/core/core.hpp"

#include <flecs.h>

// The crabnado outside a Boss Stage circles just outside the barrier, untouchable, and throws crabs at the player. In
// the Boss Stage it breaches nearest the player, fights, and once beaten retreats to its orbit.
namespace crabnado {

// What the crabnado is doing. Orbit: circling (and, during the storm-up, sliding to the player's bearing). Breach: moving
// in from the orbit to the arena edge, harmless and unhurtable. Fight: chasing the player (boss.cpp). Retreat: blown back
// out over the barrier after the last slam.
enum class CrabnadoMode { Orbit, Breach, Fight, Retreat };

// `bearing` is its angle around the arena centre in radians, in the ground plane (x = cos, z = sin, so it grows
// clockwise on the north-up map, like BarrierGlow::angle), and `radius` its distance from the centre. `direction` is
// +1 to circle counterclockwise and -1 clockwise; it flips on each wind shift (`shifts_seen` is the Wind::shifts already
// reacted to). `mode_time` is the seconds spent in Breach or Retreat and `leg_from` the radius that leg started at.
// `spawn_debt` accumulates the spawn rate; each whole unit is one throw.
struct Crabnado {
	float bearing{0.0F};
	int direction{1};
	float radius{0.0F};
	CrabnadoMode mode{CrabnadoMode::Orbit};
	float mode_time{0.0F};
	float leg_from{0.0F};
	int shifts_seen{0};
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

// Registers and reflects the crabnado components and the CrabnadoOrbit, CrabnadoThrow, CrabnadoStorm, FunnelSway and
// FunnelTint systems.
void RegisterCrabnado(const flecs::world& world);

// The point on the ground plane at `bearing` and `radius` from the arena centre, and the bearing of a ground point.
[[nodiscard]] glm::vec2 OrbitPoint(float bearing, float radius);
[[nodiscard]] float BearingOf(glm::vec2 point);

// A random orbit bearing at least kOrbitStartMinAngle from the player's bearing (any bearing while the player is within
// a unit of the centre). Draws from RngState.
[[nodiscard]] float PickOrbitBearing(const flecs::world& world);

// Adds the crabnado's look (a wispy funnel of cloud tubes and puffs with crabs caught in it) as children of `crabnado`.
void AddFunnel(const flecs::world& world, flecs::entity crabnado);

// Spawns the crabnado on its orbit, at PickOrbitBearing, as a child of `parent`, throwing enemies into `horde`. The
// player must already exist.
flecs::entity SpawnCrabnado(const flecs::world& world, flecs::entity parent, flecs::entity horde);

// Throws up to `count` crabs at the player at once, stopping at the horde cap. The debug spawn control
// uses it.
void ThrowBurst(flecs::entity crabnado, int count);

} // namespace crabnado
