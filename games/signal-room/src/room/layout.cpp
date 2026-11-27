#include "layout.hpp"

#include <algorithm>
#include <filesystem>
#include <format>
#include <memory>
#include <string_view>

#include "engine/engine.hpp"
#include "greybox_catalogue.hpp"
#include "placements.hpp"
#include "room_components.hpp"

using namespace engine;

namespace signal_room {

namespace {

using Result = std::expected<void, std::string>;

// A rectangle of cells: `cell` is its north-west corner, `size` its width (east) and depth (south).
struct CellRect {
	glm::ivec2 cell;
	glm::ivec2 size;
};

std::string Describe(const CellRect& rect) {
	return std::format("({},{}) {}x{}", rect.cell.x, rect.cell.y, rect.size.x, rect.size.y);
}

bool Within(const CellRect& rect, const glm::ivec2 min, const glm::ivec2 max) {
	return rect.size.x > 0
		   && rect.size.y > 0
		   && rect.cell.x >= min.x
		   && rect.cell.y >= min.y
		   && rect.cell.x + rect.size.x <= max.x
		   && rect.cell.y + rect.size.y <= max.y;
}

// Checks placements against the grid and claims their solid cells in the occupancy grid.
class Occupancy {
public:
	explicit Occupancy(FloorGrid& grid) : grid_(grid) {
		grid_.occupants.assign(static_cast<size_t>(grid_.width * grid_.height), 0);
	}

	// Floor furniture must lie wholly on the grid.
	[[nodiscard]] Result OnGrid(const flecs::entity e, const char* what, const CellRect& rect) const {
		if (Within(rect, {0, 0}, {grid_.width, grid_.height})) {
			return {};
		}
		return std::unexpected(
			std::format(
				"{} '{}' footprint {} runs off the {}x{} grid",
				what,
				e.name().c_str(),
				Describe(rect),
				grid_.width,
				grid_.height
			)
		);
	}

	// Walls and edge blocks may also stand on the one-cell ring around the grid.
	[[nodiscard]] Result OnGridOrRing(const flecs::entity e, const char* what, const CellRect& rect) const {
		if (Within(rect, {-1, -1}, {grid_.width + 1, grid_.height + 1})) {
			return {};
		}
		return std::unexpected(
			std::format(
				"{} '{}' cells {} run past the ring around the {}x{} grid",
				what,
				e.name().c_str(),
				Describe(rect),
				grid_.width,
				grid_.height
			)
		);
	}

	// Claim the rect's on-grid cells for `e` (cells off the grid are solid anyway).
	[[nodiscard]] Result Claim(const flecs::entity e, const CellRect& rect) const {
		for (int y = rect.cell.y; y < rect.cell.y + rect.size.y; ++y) {
			for (int x = rect.cell.x; x < rect.cell.x + rect.size.x; ++x) {
				if (!grid_.InGrid({x, y})) {
					continue;
				}
				auto& occupant = grid_.occupants[static_cast<size_t>(y * grid_.width + x)];
				if (occupant != 0) {
					return std::unexpected(
						std::format(
							"'{}' overlaps '{}' at cell ({},{})",
							e.name().c_str(),
							flecs::entity{e.world(), occupant}.name().c_str(),
							x,
							y
						)
					);
				}
				occupant = e.id();
			}
		}
		return {};
	}

private:
	FloorGrid& grid_;
};

// Check each `T` placement under `parent`, stopping at the first error.
template<typename T, typename Check>
Result CheckEach(const flecs::world& world, const flecs::entity parent, Check check) {
	for (const auto& [e, t] : PlacementsOf<T>(world, parent)) {
		if (auto result = check(e, t); !result) {
			return result;
		}
	}
	return {};
}

Result CheckPlacements(const flecs::world& world, const flecs::entity parent, FloorGrid& grid) {
	const auto& catalogue = world.get<GreyboxCatalogue>();
	const Occupancy occupancy{grid};

	const auto stations = CheckEach<Station>(world, parent, [&](const flecs::entity e, const Station& s) -> Result {
		if (!catalogue.stations.Contains(s.kind)) {
			return std::unexpected(std::format("Station '{}' has unknown kind '{}'", e.name().c_str(), s.kind));
		}
		const CellRect rect{s.cell, s.footprint};
		return occupancy.OnGrid(e, "Station", rect).and_then([&] { return occupancy.Claim(e, rect); });
	});
	const auto counters = [&] {
		return CheckEach<Counter>(world, parent, [&](const flecs::entity e, const Counter& c) -> Result {
			const CellRect rect{c.cell, {1, 1}};
			return occupancy.OnGrid(e, "Counter", rect).and_then([&] { return occupancy.Claim(e, rect); });
		});
	};
	const auto props = [&] {
		return CheckEach<Prop>(world, parent, [&](const flecs::entity e, const Prop& p) -> Result {
			if (!catalogue.props.Contains(p.kind)) {
				return std::unexpected(std::format("Prop '{}' has unknown kind '{}'", e.name().c_str(), p.kind));
			}
			// A wall-mounted prop has no floor footprint; only its anchor cell must be on the grid.
			const CellRect rect{p.cell, AnchorFootprint(p)};
			return occupancy.OnGrid(e, "Prop", rect).and_then([&]() -> Result {
				return IsWallMounted(p) || !p.solid ? Result{} : occupancy.Claim(e, rect);
			});
		});
	};
	const auto walls = [&] {
		return CheckEach<Wall>(world, parent, [&](const flecs::entity e, const Wall& w) -> Result {
			if (!catalogue.walls.Contains(w.heightClass)) {
				return std::unexpected(
					std::format("Wall '{}' has unknown height class '{}'", e.name().c_str(), w.heightClass)
				);
			}
			const CellRect rect{w.cell, w.footprint};
			return occupancy.OnGridOrRing(e, "Wall", rect).and_then([&] { return occupancy.Claim(e, rect); });
		});
	};
	const auto edgeBlocks = [&] {
		return CheckEach<EdgeBlock>(world, parent, [&](const flecs::entity e, const EdgeBlock& b) -> Result {
			const CellRect rect{b.cell, b.footprint};
			return occupancy.OnGridOrRing(e, "Edge block", rect).and_then([&] { return occupancy.Claim(e, rect); });
		});
	};
	const auto spawns = [&] {
		return CheckEach<SpawnMarker>(world, parent, [&](const flecs::entity e, const SpawnMarker& m) -> Result {
			if (!grid.IsSolid(m.cell)) {
				return {};
			}
			return std::unexpected(
				std::format(
					"Spawn marker '{}' at cell ({},{}) is off the grid or on a solid cell",
					e.name().c_str(),
					m.cell.x,
					m.cell.y
				)
			);
		});
	};
	return stations.and_then(counters).and_then(props).and_then(walls).and_then(edgeBlocks).and_then(spawns);
}

// Hand the occupancy grid to the engine pathfinder so the AI can path over the authored floor.
void PublishNavGraph(const flecs::world& world, const FloorGrid& grid) {
	auto graph = std::make_unique<pathfinding::GridNavGraph>();
	graph->Resize(grid.width, grid.height);
	graph->cell_size = grid.cellSize;
	graph->origin = grid.origin;
	for (int y = 0; y < grid.height; ++y) {
		for (int x = 0; x < grid.width; ++x) {
			graph->SetWalkable(x, y, !grid.IsSolid({x, y}));
		}
	}
	graph->BuildAdjacency();
	world.get_mut<pathfinding::NavEnvironment>().graph = std::move(graph);
	world.modified<pathfinding::NavEnvironment>();
}

} // namespace

Result LoadLayout(const flecs::world& world, const std::string& path, const flecs::entity parent) {
	if (std::error_code ec; !std::filesystem::is_regular_file(path, ec)) {
		return std::unexpected(std::format("Layout file not found: {}", path));
	}

	UnloadLayout(world);
	world.set<LayoutErrors>({});
	if (level::LoadLevel(world, path, parent) < 0) {
		return std::unexpected(std::format("Layout file {} could not be loaded as a Level", path));
	}
	if (const auto& errors = world.get<LayoutErrors>().errors; !errors.empty()) {
		return std::unexpected(std::format("Layout file {}: {}", path, errors.front()));
	}

	if (!world.has<FloorGrid>()) {
		return std::unexpected(std::format("Layout file {} has no floor_grid", path));
	}
	auto& grid = world.get_mut<FloorGrid>();
	if (grid.width <= 0 || grid.height <= 0 || grid.cellSize <= 0.0F) {
		return std::unexpected(
			std::format(
				"Layout file {} has a bad floor_grid ({}x{} cells of {} m)",
				path,
				grid.width,
				grid.height,
				grid.cellSize
			)
		);
	}

	if (const auto checked = CheckPlacements(world, parent, grid); !checked) {
		return std::unexpected(std::format("Layout file {}: {}", path, checked.error()));
	}
	PublishNavGraph(world, grid);
	return {};
}

Result ApplyInService(
	const flecs::world& world,
	const flecs::entity parent,
	const std::vector<std::string>& inService,
	const std::string& shiftPath
) {
	const auto stations = PlacementsOf<Station>(world, parent);

	for (const auto& name : inService) {
		if (std::ranges::none_of(stations, [&](const auto& placement) {
				return std::string_view{placement.first.name().c_str()} == name;
			})) {
			return std::unexpected(
				std::format(
					"Shift file {} lists '{}' in in_service, but its Layout has no Station by that name",
					shiftPath,
					name
				)
			);
		}
	}
	for (const auto& [e, _] : stations) {
		if (std::ranges::find(inService, std::string{e.name().c_str()}) == inService.end()) {
			e.add<Tarped>();
		}
	}
	return {};
}

void UnloadLayout(const flecs::world& world) {
	world.remove<FloorGrid>();
	if (world.has<pathfinding::NavEnvironment>()) {
		world.get_mut<pathfinding::NavEnvironment>().graph.reset();
		world.modified<pathfinding::NavEnvironment>();
	}
}

} // namespace signal_room
