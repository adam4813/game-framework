#pragma once

#include "../pathfinding_algorithm.hpp"

namespace engine::pathfinding {

/// A* pathfinding algorithm.
/// Uses INavGraph::Heuristic to guide search toward the goal.
/// Optimal and generally faster than Dijkstra on grids where a good heuristic is available.
class AStarAlgorithm : public INavAlgorithm {
public:
	[[nodiscard]] NavPath FindPath(const INavGraph& graph, graph::NodeId start, graph::NodeId goal) const override;
};

} // namespace engine::pathfinding
