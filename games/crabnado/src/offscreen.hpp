#pragma once

#include <flecs.h>

#include <glm/glm.hpp>

// The `offscreen` spawner (Act 3's horde): an unseen source that throws volleys of enemies from one bearing at a time,
// with no crabnado orbiting. It uses the crabnado's landing rule (EdgeLanding) and the same landing shadow, with the
// origin standing in for the crabnado; nothing is drawn at the origin. Its tunables are the Act's OffscreenSpawner.
namespace crabnado {

// Singleton, reflected: the spawner's progress. `debt` accumulates the Stage's spawn rate; once it reaches `next_volley`
// (rolled from the Act's volley_min-volley_max and kept until thrown) that many enemies are thrown at once. `bearing` is
// the last volley's origin angle, in the same convention as Crabnado::bearing, valid when `has_bearing`.
struct OffscreenState {
	float debt{0.0F};
	int next_volley{0};
	float bearing{0.0F};
	bool has_bearing{false};
};

// A random origin bearing at least `minTurnDeg` degrees (and at most 180) from `last`, on a random side of it, or any
// bearing when `hasLast` is false. Draws from RngState.
[[nodiscard]] float PickVolleyBearing(const flecs::world& world, bool hasLast, float last, float minTurnDeg);

// Registers the OffscreenState singleton and the OffscreenThrow system. Call from the module constructor.
void RegisterOffscreen(const flecs::world& world);

} // namespace crabnado
