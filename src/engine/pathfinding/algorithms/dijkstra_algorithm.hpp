#pragma once

#include "../pathfinding_algorithm.hpp"

namespace engine::pathfinding {

/// Dijkstra's uniform-cost search algorithm.
/// Explores all nodes ordered by accumulated cost; optimal for arbitrary edge weights.
/// Slower than A* on grids since it uses no heuristic.
class DijkstraAlgorithm : public INavAlgorithm {
public:
	[[nodiscard]] NavPath
	FindPath(const INavGraph& graph, graph::NodeId start, graph::NodeId goal) const override;
};

} // namespace engine::pathfinding
