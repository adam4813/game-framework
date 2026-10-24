#pragma once

#include <glm/glm.hpp>

#include <flecs.h>

// Bubbles: the player's second weapon. On a cooldown it fires a volley at the nearest landed enemy; each
// bubble drifts with the wind and the wave push as it flies and pops on its first hit.
namespace crabnado {

// On the player. `count` bubbles per volley, `timer` counts down to the next volley.
struct Bubbles {
	float damage{0.0F};
	float cooldown{0.0F};
	int count{1};
	float speed{0.0F};
	float timer{0.0F};
};

// A bubble in flight (units per second on the ground plane). Removed when `life` runs out or it hits.
struct Bubble {
	glm::vec2 velocity{0.0F};
	float life{0.0F};
};

// Registers and reflects the bubble components and the BubbleFire and BubbleFly systems.
void RegisterBubbles(const flecs::world& world);

// Gives `player` the starting bubbles weapon.
void AddBubbles(flecs::entity player);

} // namespace crabnado
