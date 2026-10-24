#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include <flecs.h>

namespace crabnado {

// One landed enemy as the grid saw it at the start of the frame.
struct CrowdEntry {
	flecs::entity_t entity{0};
	glm::vec2 position{0.0F};
	float radius{0.0F};
};

// Singleton: a uniform grid over the arena of every landed enemy, rebuilt each frame by the
// CrowdGridRebuild system. Crowding, contact damage and weapon hits query it instead of comparing every
// enemy with every other one. Enemies in the air are not in it.
struct CrowdGrid {
	float cell_size{1.0F};
	float half_extent{0.0F}; // covers [-half_extent, half_extent] on x and z; positions outside clamp to the edge
	int cells_per_side{0};
	float max_radius{0.0F}; // largest radius in the grid, so a query can widen its range to reach any edge
	std::vector<int> cell_start; // entries of cell c are entries[cell_start[c] .. cell_start[c + 1])
	std::vector<CrowdEntry> entries;
	std::vector<CrowdEntry> scratch; // unsorted entries; it and `cursor` are kept to reuse their capacity
	std::vector<int> cursor;

	// Sizes the grid. Call once before the first Rebuild.
	void Resize(float cellSize, float halfExtent);

	// Sorts `scratch` into cells (a counting sort), leaving the result in `entries`.
	void Rebuild();

	// Calls fn(const CrowdEntry&) for every entry in the cells overlapping the square of half-size `range`
	// around `point`. Callers check the exact distance themselves.
	template<typename Fn>
	void ForEachNear(const glm::vec2 point, const float range, Fn&& fn) const {
		const int x0 = CellCoord(point.x - range);
		const int x1 = CellCoord(point.x + range);
		const int z0 = CellCoord(point.y - range);
		const int z1 = CellCoord(point.y + range);
		for (int z = z0; z <= z1; ++z) {
			for (int x = x0; x <= x1; ++x) {
				const int cell = z * cells_per_side + x;
				for (int i = cell_start[cell]; i < cell_start[cell + 1]; ++i) {
					fn(entries[static_cast<size_t>(i)]);
				}
			}
		}
	}

	[[nodiscard]] int CellCoord(const float value) const {
		const int coord = static_cast<int>(std::floor((value + half_extent) / cell_size));
		return std::clamp(coord, 0, cells_per_side - 1);
	}
};

} // namespace crabnado
