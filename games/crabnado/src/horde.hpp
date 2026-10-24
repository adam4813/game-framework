#pragma once

#include <glm/glm.hpp>

#include <flecs.h>

#include "engine/engine.hpp"

// The horde: enemies the crabnado throws at the player. Each enemy is a root entity carrying Enemy and
// a Transform, with its body and claws as child sphere primitives that carry their own inline Material
// (a per-instance tint that keeps the render batch instanced). A thrown enemy is Airborne until it lands;
// only landed enemies crowd, deal damage or take it.
namespace crabnado {

// A landed or flying enemy. `flash` counts down the hit flash; `color` is its untinted look.
struct Enemy {
	float hp{0.0F};
	float speed{0.0F};
	float radius{0.0F};
	float contact_dps{0.0F};
	float flash{0.0F};
	engine::core::Rgba color{};
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

// Throws one crab from `from` (the crabnado's base) toward a landing point between it and `target`, as a
// child of `parent` (so the scene's Unload destroys it). Draws from the world's RngState; enemy HP grows
// with the run clock.
void ThrowCrab(const flecs::world& world, flecs::entity parent, glm::vec2 from, glm::vec2 target);

// Damages a landed enemy and starts its hit flash. The EnemyDefeat system removes it at 0 HP.
void DamageEnemy(flecs::entity enemy, float amount);

} // namespace crabnado
