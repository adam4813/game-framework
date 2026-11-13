#pragma once

#include <glm/glm.hpp>

#include <flecs.h>

// The player's hermit crab and the camera that follows it.
namespace crabnado {

// Tag on the player's hermit crab.
struct Player {};

// The player's movement speed (units/s), health and shell pickup range. The run is lost when hp
// reaches 0.
struct PlayerStats {
	float speed{0.0F};
	float hp{0.0F};
	float max_hp{0.0F};
	float pickup_range{0.0F};
};

// The player's dig-in stamina. Holding dig in drains `drain` per second and roots the crab (`dug_in`); otherwise
// it refills at `regen`. At 0 the crab is `exhausted` and cannot dig in until stamina refills to a quarter of
// `max`.
struct Stamina {
	float value{0.0F};
	float max{0.0F};
	float regen{0.0F};
	float drain{0.0F};
	bool exhausted{false};
	bool dug_in{false};
};

// The velocity (units/s) a crest gives the player on top of steering. Zero while dug in, decays after a crest.
struct SurfKnock {
	glm::vec2 velocity{0.0F};
};

// A camera that keeps `offset` from the player (north-up: it never rotates).
struct FollowCamera {
	glm::vec3 offset{0.0F};
};

// Registers and reflects the player components and the PlayerStamina, PlayerMove, PlayerDefeat and CameraFollow
// systems. Call from the module constructor.
void RegisterPlayer(const flecs::world& world);

// Spawns the player's hermit crab at the arena centre as a child of `parent`.
flecs::entity SpawnPlayer(const flecs::world& world, flecs::entity parent);

// Adds the hermit crab's look (body, shell and claws, forward is local +z) as children of `crab`.
void AddCrabBody(const flecs::world& world, flecs::entity crab);

// The player entity, or an invalid entity when no run is loaded.
[[nodiscard]] flecs::entity FindPlayer(const flecs::world& world);

} // namespace crabnado
