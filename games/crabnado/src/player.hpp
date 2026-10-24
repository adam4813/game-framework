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

// A camera that keeps `offset` from the player (north-up: it never rotates).
struct FollowCamera {
	glm::vec3 offset{0.0F};
};

// Registers and reflects the player components and the PlayerMove, PlayerDefeat and CameraFollow
// systems. Call from the module constructor.
void RegisterPlayer(const flecs::world& world);

// Spawns the player's hermit crab at the arena centre as a child of `parent`.
flecs::entity SpawnPlayer(const flecs::world& world, flecs::entity parent);

// The player entity, or an invalid entity when no run is loaded.
[[nodiscard]] flecs::entity FindPlayer(const flecs::world& world);

} // namespace crabnado
