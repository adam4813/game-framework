#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "engine/core/registry.hpp"
#include "pathfinding_algorithm.hpp"
#include "pathfinding_graph.hpp"

namespace engine::pathfinding {

/// One-shot path request. Set on an entity to trigger a path computation.
///
/// Registered with the Flecs `CanToggle` trait, so PathRequestSystem solves enabled requests and then
/// disables the component. Re-request by updating `goal` and calling `entity.enable<PathRequest>()`.
struct PathRequest {
	glm::vec3 goal{0.0f};
	std::string algorithm; ///< Empty = use PathfinderRegistry::default_algorithm
};

/// Computed path and follow state. Written by PathRequestSystem; advanced by PathFollowSystem.
struct PathAgent {
	std::vector<glm::vec3> waypoints; ///< World-space positions from start to goal
	size_t current_waypoint{0};
	float arrival_radius{0.3f}; ///< Distance at which a waypoint is considered reached
	bool reached_goal{false};
};

/// Output mode for PathFollowSystem.
enum class PathFollowMode : uint8_t {
	SteeringTarget, ///< Write ai::SteeringTarget (entity must also carry SteeringAgent)
	Kinematic,      ///< Write desired_velocity; game / physics integrates it
	Direct,         ///< Move spatial::Transform directly (no physics, e.g. cutscenes)
};

/// Follow behaviour configuration. Attach alongside PathAgent.
struct PathFollow {
	PathFollowMode mode{PathFollowMode::SteeringTarget};
	float speed{3.0f};                ///< Used by Kinematic and Direct modes
	glm::vec3 desired_velocity{0.0f}; ///< Output written by Kinematic mode each frame
};

/// Periodic auto-replan. Attach alongside PathFollow and PathAgent.
/// Update `goal` each frame when chasing a moving target (e.g. the player).
struct PathAutoReplan {
	glm::vec3 goal{0.0f};  ///< Goal position; update each frame for dynamic targets
	std::string algorithm; ///< Empty = PathfinderRegistry::default_algorithm
	float interval{1.0f};  ///< Seconds between replans
	float timer{0.0f};     ///< Runtime: counts up to interval, then resets (not authored)
};

/// Type-erased pathfinding algorithm: takes a graph, start node, and goal node; returns a path.
using NavAlgorithmFn = std::function<NavPath(const INavGraph&, graph::NodeId, graph::NodeId)>;

/// World singleton: registry of named algorithm functions.
/// Extends core::Registry<NavAlgorithmFn> with a configurable default name.
struct PathfinderRegistry : core::Registry<NavAlgorithmFn> {
	std::string default_algorithm{"astar"};
};

/// World singleton: the active navigation graph for the current scene.
/// Scenes assign `graph` at load time; pathfinding systems read it each frame.
struct NavEnvironment {
	std::unique_ptr<INavGraph> graph;

	[[nodiscard]] bool IsReady() const { return graph != nullptr; }
};

} // namespace engine::pathfinding
