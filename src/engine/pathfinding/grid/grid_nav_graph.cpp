#include "grid_nav_graph.hpp"

#include <algorithm>
#include <bit>

namespace engine::pathfinding {

void GridNavGraph::Resize(const int w, const int h) {
	width = w;
	height = h;
	const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
	walkable.assign(n, true);
	costs.assign(n, 1.0f);
	heights.clear(); // empty = flat (Y = origin.y)
	adjacency_.clear(); // invalidated — caller must call BuildAdjacency() again

	// Precompute fast-path for ToXZ when width is a power of 2 (e.g. 64).
	// Replaces integer division + modulo with a bitshift + AND (1 cycle vs ~20–40 cycles).
	const bool pow2 = w > 0 && (w & (w - 1)) == 0;
	width_shift_ = pow2 ? std::countr_zero(static_cast<unsigned int>(w)) : -1;
	width_mask_  = pow2 ? (w - 1) : 0;
}

void GridNavGraph::SetWalkable(const int x, const int z, const bool value) {
	if (!InBounds(x, z)) return;
	walkable[static_cast<size_t>(z) * static_cast<size_t>(width) + static_cast<size_t>(x)] = value;
}

void GridNavGraph::SetCost(const int x, const int z, const float cost) {
	if (!InBounds(x, z)) return;
	costs[static_cast<size_t>(z) * static_cast<size_t>(width) + static_cast<size_t>(x)] = cost;
}

void GridNavGraph::SetHeight(const int x, const int z, const float y) {
	if (!InBounds(x, z)) return;
	if (heights.empty()) {
		heights.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0.0f);
	}
	heights[static_cast<size_t>(z) * static_cast<size_t>(width) + static_cast<size_t>(x)] = y;
}

bool GridNavGraph::IsWalkable(const int x, const int z) const {
	if (!InBounds(x, z)) return false;
	return walkable[static_cast<size_t>(z) * static_cast<size_t>(width) + static_cast<size_t>(x)];
}

float GridNavGraph::GetCost(const int x, const int z) const {
	if (!InBounds(x, z)) return 1.0f;
	return costs[static_cast<size_t>(z) * static_cast<size_t>(width) + static_cast<size_t>(x)];
}

float GridNavGraph::GetHeight(const int x, const int z) const {
	if (!InBounds(x, z) || heights.empty()) return 0.0f;
	return heights[static_cast<size_t>(z) * static_cast<size_t>(width) + static_cast<size_t>(x)];
}

glm::vec3 GridNavGraph::Position(const graph::NodeId id) const {
	const auto [x, z] = ToXZ(id);
	return {
		origin.x + (static_cast<float>(x) + 0.5f) * cell_size,
		origin.y + GetHeight(x, z),
		origin.z + (static_cast<float>(z) + 0.5f) * cell_size,
	};
}

graph::NodeId GridNavGraph::Locate(const glm::vec3 world_pos) const {
	if (width <= 0 || height <= 0) return graph::NullNode;
	int x = static_cast<int>((world_pos.x - origin.x) / cell_size);
	int z = static_cast<int>((world_pos.z - origin.z) / cell_size);
	x = std::clamp(x, 0, width - 1);
	z = std::clamp(z, 0, height - 1);
	return ToNodeId(x, z);
}

std::vector<NavEdge> GridNavGraph::Neighbors(const graph::NodeId from) const {
	std::vector<NavEdge> result;
	NeighborsInto(from, result);
	return result;
}

void GridNavGraph::NeighborsInto(const graph::NodeId from, std::vector<NavEdge>& out) const {
	if (!adjacency_.empty()) {
		// Fast path: return precomputed list (BuildAdjacency was called).
		// Reserve first so the ws.neighbors reuse-buffer gets capacity on its first use per
		// worker thread, avoiding a silent heap allocation that contradicts the no-alloc design.
		if (from < adjacency_.size()) {
			out.reserve(adjacency_[from].size());
			out = adjacency_[from];
		}
		else {
			out.clear();
		}
		return;
	}

	// Fallback: on-the-fly computation (before BuildAdjacency is called).
	out.clear();
	if (!Valid(from)) return;
	const auto [x, z] = ToXZ(from);

	out.reserve(allow_diagonals ? 8 : 4);

	// Cardinal directions: N, S, W, E
	for (int d = 0; d < 4; ++d) {
		constexpr int kDX[] = {0, 0, -1, 1};
		constexpr int kDZ[] = {-1, 1, 0, 0};

		const int nx = x + kDX[d];
		const int nz = z + kDZ[d];
		if (!IsWalkable(nx, nz)) continue;
		out.push_back({ToNodeId(nx, nz), GetCost(nx, nz) * cell_size});
	}

	if (allow_diagonals) {
		for (int d = 0; d < 4; ++d) {
			constexpr int kDiagX[] = {-1, 1, -1, 1};
			constexpr int kDiagZ[] = {-1, -1, 1, 1};
			constexpr float kDiagCost = 1.41421356f; // sqrt(2)

			const int nx = x + kDiagX[d];
			const int nz = z + kDiagZ[d];
			// Prevent corner-cutting through walls
			if (!IsWalkable(nx, nz)) continue;
			if (!IsWalkable(x + kDiagX[d], z)) continue;
			if (!IsWalkable(x, z + kDiagZ[d])) continue;
			out.push_back({ToNodeId(nx, nz), GetCost(nx, nz) * cell_size * kDiagCost});
		}
	}
}

void GridNavGraph::BuildAdjacency() {
	const size_t node_count = static_cast<size_t>(width) * static_cast<size_t>(height);
	adjacency_.assign(node_count + 1, {}); // 1-indexed; adjacency_[0] stays empty (NullNode)

	for (int z = 0; z < height; ++z) {
		for (int x = 0; x < width; ++x) {
			const graph::NodeId id = ToNodeId(x, z);
			auto& edges = adjacency_[id];
			if (!IsWalkable(x, z)) continue;

			edges.reserve(allow_diagonals ? 8 : 4);

			for (int d = 0; d < 4; ++d) {
				constexpr int kDX[] = {0, 0, -1, 1};
				constexpr int kDZ[] = {-1, 1, 0, 0};
				const int nx = x + kDX[d];
				const int nz = z + kDZ[d];
				if (!IsWalkable(nx, nz)) continue;
				edges.push_back({ToNodeId(nx, nz), GetCost(nx, nz) * cell_size});
			}

			if (allow_diagonals) {
				constexpr float kDiagCost = 1.41421356f;
				for (int d = 0; d < 4; ++d) {
					constexpr int kDiagX[] = {-1, 1, -1, 1};
					constexpr int kDiagZ[] = {-1, -1, 1, 1};
					const int nx = x + kDiagX[d];
					const int nz = z + kDiagZ[d];
					if (!IsWalkable(nx, nz)) continue;
					if (!IsWalkable(x + kDiagX[d], z)) continue;
					if (!IsWalkable(x, z + kDiagZ[d])) continue;
					edges.push_back({ToNodeId(nx, nz), GetCost(nx, nz) * cell_size * kDiagCost});
				}
			}
		}
	}
}

float GridNavGraph::Heuristic(const graph::NodeId from, const graph::NodeId goal) const {
	const auto [fx, fz] = ToXZ(from);
	const auto [gx, gz] = ToXZ(goal);
	const float dx = std::abs(static_cast<float>(fx - gx));
	const float dz = std::abs(static_cast<float>(fz - gz));
	// Octile distance: optimal heuristic for 8-neighbour grids
	return cell_size * (dx + dz + (1.41421356f - 2.0f) * std::min(dx, dz));
}

bool GridNavGraph::Valid(const graph::NodeId id) const {
	if (id == graph::NullNode) return false;
	return (id - 1) < static_cast<graph::NodeId>(walkable.size());
}

size_t GridNavGraph::NodeCount() const { return walkable.size(); }

graph::NodeId GridNavGraph::ToNodeId(const int x, const int z) const {
	return static_cast<graph::NodeId>(static_cast<size_t>(z) * static_cast<size_t>(width) + static_cast<size_t>(x) + 1);
}

std::pair<int, int> GridNavGraph::ToXZ(const graph::NodeId id) const {
	const size_t idx = id - 1;
	if (width_shift_ >= 0) {
		return {
			static_cast<int>(idx & static_cast<size_t>(width_mask_)),
			static_cast<int>(idx >> static_cast<size_t>(width_shift_)),
		};
	}
	return {
		static_cast<int>(idx % static_cast<size_t>(width)),
		static_cast<int>(idx / static_cast<size_t>(width)),
	};
}

bool GridNavGraph::InBounds(const int x, const int z) const { return x >= 0 && z >= 0 && x < width && z < height; }

} // namespace engine::pathfinding
