#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "engine/graph/graph.hpp"

namespace engine::pathfinding {

/// A weighted directed edge returned by INavGraph::Neighbors.
struct NavEdge {
	graph::NodeId to{graph::NullNode};
	float cost{1.0f}; ///< Movement cost for this edge (lower = preferred)
};

/// Abstract navigation graph interface.
/// Algorithms interact only through this interface — no knowledge of tiles, navmeshes, or the world.
/// Implement this to provide a navigation environment.
class INavGraph {
public:
	virtual ~INavGraph() = default;

	/// World-space centre of a node.
	[[nodiscard]] virtual glm::vec3 Position(graph::NodeId id) const = 0;

	/// Find the node nearest to a world-space position.
	[[nodiscard]] virtual graph::NodeId Locate(glm::vec3 world_pos) const = 0;

	/// Traversable neighbours of `from`, each with an edge cost.
	[[nodiscard]] virtual std::vector<NavEdge> Neighbors(graph::NodeId from) const = 0;

	/// Traversable neighbours of `from`, each with an edge cost.
	/// Default: delegates to Neighbors(); override to fill a caller buffer without allocating.
	virtual void NeighborsInto(const graph::NodeId from, std::vector<NavEdge>& out) const { out = Neighbors(from); }

	/// Admissible heuristic estimate of cost from `from` to `goal` (used by A*).
	/// Must never over-estimate the true cost.
	[[nodiscard]] virtual float Heuristic(graph::NodeId from, graph::NodeId goal) const = 0;

	/// True if `id` is a valid node in this graph.
	[[nodiscard]] virtual bool Valid(graph::NodeId id) const = 0;

	/// Total number of nodes in the graph.
	[[nodiscard]] virtual size_t NodeCount() const = 0;
};

} // namespace engine::pathfinding
