#pragma once

#include <glm/glm.hpp>

#include <flecs.h>

#include "engine/engine.hpp"

// The horde: enemies the crabnado throws at the player. Each enemy is a root entity carrying Enemy and
// a Transform, with its body and claws as child sphere primitives that carry their own inline Material
// (a per-instance tint that keeps the render batch instanced). A thrown enemy is Airborne until it lands;
// only landed enemies crowd, deal damage or take it.
namespace crabnado {

// A landed or flying enemy. `xp` is the value of the shell it drops when defeated. `flash` counts down
// the hit flash; `color` is its untinted look.
struct Enemy {
	float hp{0.0F};
	float speed{0.0F};
	float radius{0.0F};
	float contact_dps{0.0F};
	int xp{0};
	// Index into the EnemyRegistry, and the type's tumble threshold and gait copied at spawn.
	int type{0};
	float tumble_min{0.0F};
	bool walks_sideways{true};
	float flash{0.0F};
	engine::core::Rgba color{};
	// Seconds left of a wave tumble: no chasing and no contact damage. `knock` is the velocity the crest
	// carries it with, in units per second; the root spins on its local x axis meanwhile.
	float tumble{0.0F};
	glm::vec2 knock{0.0F};
	// Set once a rock slam has hurt it, so its defeat counts as a rock kill.
	bool slammed{false};
};

// An enemy in flight on its ballistic arc. Removed when it lands.
struct Airborne {
	glm::vec3 velocity{0.0F};
};

// Singleton: live performance readout, refreshed each frame by the arena scene and read over the Remote
// API during the horde performance check.
struct HordeReadout {
	int enemy_count{0};
	int fps{0};
};

// Registers and reflects the horde components, the CrowdGrid singleton and the horde systems (flight,
// crowding chase, contact damage, defeat, hit flash). Call from the module constructor.
void RegisterHorde(const flecs::world& world);

// Throws one enemy of registry type `typeIndex` from `from` (the crabnado's base) toward a landing point between it and `target`, as a
// child of `parent` (so the scene's Unload destroys it). Draws from the world's RngState; enemy HP grows
// with the run clock.
void ThrowEnemy(const flecs::world& world, flecs::entity parent, int typeIndex, glm::vec2 from, glm::vec2 target);

// Damages a landed enemy and starts its hit flash. At 0 HP the EnemyDefeat system removes it and drops
// its shell.
void DamageEnemy(flecs::entity enemy, float amount);

} // namespace crabnado
