#include "crowd_grid.hpp"

namespace crabnado {

void CrowdGrid::Resize(const float cellSize, const float halfExtent) {
	cell_size = cellSize;
	half_extent = halfExtent;
	cells_per_side = static_cast<int>(std::ceil(2.0F * halfExtent / cellSize));
	cell_start.assign(static_cast<size_t>(cells_per_side * cells_per_side) + 1, 0);
}

void CrowdGrid::Rebuild() {
	std::ranges::fill(cell_start, 0);
	max_radius = 0.0F;
	const auto cellOf = [this](const CrowdEntry& entry) {
		return CellCoord(entry.position.y) * cells_per_side + CellCoord(entry.position.x);
	};

	// Count each cell into the slot after it, then prefix-sum so cell_start[c] is where cell c begins.
	for (const CrowdEntry& entry : scratch) {
		++cell_start[static_cast<size_t>(cellOf(entry)) + 1];
		max_radius = std::max(max_radius, entry.radius);
	}
	for (size_t c = 1; c < cell_start.size(); ++c) {
		cell_start[c] += cell_start[c - 1];
	}

	entries.resize(scratch.size());
	cursor.assign(cell_start.begin(), cell_start.end() - 1);
	for (const CrowdEntry& entry : scratch) {
		entries[static_cast<size_t>(cursor[static_cast<size_t>(cellOf(entry))]++)] = entry;
	}
}

} // namespace crabnado
