#pragma once

#include <glm/glm.hpp>

#include <flecs.h>

namespace game {

/// Tag marking the entity the player directly controls via keyboard input. Pure data-less tag —
/// the movement system queries `.with<PlayerControlled>()` to find the single driven entity.
struct PlayerControlled {};

/// Smooth per-tile interpolation state for a grid-locked mover. A mover is authored as
/// `grid_mover: {}` in a level (all runtime state is seeded from the entity's Transform on the
/// first move), with optional `step_size` / `duration` / `initial_delay` tuning overrides.
struct GridMover {
	glm::vec3 origin{};
	glm::vec3 target{};
	float step_size{0.5f};
	float duration{0.15f};
	float elapsed{0.0f};
	bool moving{false};
	float hold_elapsed{0.0f};
	float initial_delay{0.1f};
};

/// Register the level-loader factories for grid movement: the `player_controlled` tag and the
/// `grid_mover` component loader, plus the scripting registration for PlayerControlled. Call once
/// at startup (shared by every scene that authors a grid-locked player).
void RegisterGridMovementLoaders(const flecs::world& world);

/// Register the two grid-movement systems (input → initiate move, and smooth interpolation toward
/// the target tile) as children of `sceneRoot` so they are torn down with the scene. `tilemapEntity`
/// supplies the Tilemap + TileSet used for walkability checks.
void RegisterGridMovementSystems(
	const flecs::world& world,
	flecs::entity sceneRoot,
	flecs::entity tilemapEntity
);

} // namespace game
