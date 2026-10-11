#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "engine/graph/graph.hpp"
#include "pathfinding_graph.hpp"

namespace engine::pathfinding {

/// Computed world-space path from a start to a goal node.
struct NavPath {
	std::vector<glm::vec3> waypoints; ///< World-space positions from start to goal (inclusive)
	bool found{false};                ///< False if no path exists between start and goal
};

/// Abstract interface for a pathfinding algorithm.
/// Each implementation is stateless — all per-search state lives on the stack inside FindPath.
class INavAlgorithm {
public:
	virtual ~INavAlgorithm() = default;

	/// Compute a path from `start` to `goal` through the given navigation graph.
	[[nodiscard]] virtual NavPath FindPath(const INavGraph& graph, graph::NodeId start, graph::NodeId goal) const = 0;
};

} // namespace engine::pathfinding
