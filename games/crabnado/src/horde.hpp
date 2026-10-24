#pragma once

#include <flecs.h>

// The horde: enemies the crabnado flings at the player. Each enemy is a root entity carrying Enemy and
// a Transform, with its body and claws as child sphere primitives that carry their own inline Material
// (a per-instance tint that keeps the render batch instanced).
namespace crabnado {

// An enemy walking toward the arena centre at `speed` units per second.
struct Enemy {
	float speed{0.0F};
};

// Singleton: live performance readout, refreshed each frame by the arena scene and read over the Remote
// API during the horde performance check.
struct HordeReadout {
	int enemy_count{0};
	int fps{0};
};

// Registers and reflects the horde components and the HordeWalk system. Call from the module constructor.
void RegisterHorde(const flecs::world& world);

// Spawns `count` crabs on the arena edge as children of `parent` (so the scene's Unload destroys them),
// placed with the world's RngState.
void SpawnCrabs(const flecs::world& world, flecs::entity parent, int count);

} // namespace crabnado
