#pragma once

#include <flecs.h>

namespace engine::pathfinding {

/// Pathfinding module for Flecs ECS integration.
///
/// Registers components (PathRequest, PathAgent, PathFollow, PathAutoReplan) and the three
/// systems: PathRequestSystem, PathAutoReplanSystem, PathFollowSystem.
/// Also emplaces the NavEnvironment and PathfinderRegistry singletons and registers the
/// built-in "astar" and "dijkstra" algorithms.
class PathfindingModule {
public:
	explicit PathfindingModule(const flecs::world& world);
};

} // namespace engine::pathfinding
