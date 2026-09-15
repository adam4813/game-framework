#pragma once

#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "../pathfinding_graph.hpp"

namespace engine::pathfinding {

/// Flat grid navigation graph for 2D tile-based maps.
///
/// Nodes are laid out on the X/Z plane (Y up). Each cell (x, z) maps to NodeId via:
///   NodeId = z * width + x + 1  (1-based; NullNode = 0)
///
/// Heights are optional: leave `heights` empty for a perfectly flat grid.
struct GridNavGraph : INavGraph {
	int width{0};
	int height{0}; ///< Number of rows along the Z axis
	float cell_size{1.0f};
	glm::vec3 origin{0.0f}; ///< World-space position of cell (0,0) bottom-left corner
	bool allow_diagonals{true};

	std::vector<bool> walkable; ///< [z * width + x] — is this cell traversable?
	std::vector<float> costs;   ///< [z * width + x] — edge cost multiplier (1.0 = default)
	std::vector<float> heights; ///< [z * width + x] — cell Y offset from origin.y (optional)

	/// Resize the grid and reset all cells to walkable with default costs.
	void Resize(int w, int h);
	void SetWalkable(int x, int z, bool value);
	void SetCost(int x, int z, float cost);
	void SetHeight(int x, int z, float y);
	[[nodiscard]] bool IsWalkable(int x, int z) const;
	[[nodiscard]] float GetCost(int x, int z) const;
	[[nodiscard]] float GetHeight(int x, int z) const;

	// INavGraph
	[[nodiscard]] glm::vec3 Position(graph::NodeId id) const override;
	[[nodiscard]] graph::NodeId Locate(glm::vec3 world_pos) const override;
	[[nodiscard]] std::vector<NavEdge> Neighbors(graph::NodeId from) const override;
	void NeighborsInto(graph::NodeId from, std::vector<NavEdge>& out) const override;
	[[nodiscard]] float Heuristic(graph::NodeId from, graph::NodeId goal) const override;
	[[nodiscard]] bool Valid(graph::NodeId id) const override;
	[[nodiscard]] size_t NodeCount() const override;

private:
	[[nodiscard]] graph::NodeId ToNodeId(int x, int z) const;
	[[nodiscard]] std::pair<int, int> ToXZ(graph::NodeId id) const;
	[[nodiscard]] bool InBounds(int x, int z) const;
};

} // namespace engine::pathfinding
