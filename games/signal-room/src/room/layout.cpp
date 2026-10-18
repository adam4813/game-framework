#include "layout.hpp"

#include <filesystem>
#include <format>

#include <nlohmann/json.hpp>

#include "engine/engine.hpp"
#include "room_components.hpp"
#include "room_constants.hpp"

using namespace engine;

namespace signal_room {

namespace {

// A grout slab the size of the whole floor, with one slightly inset tile per cell on top so the 1 m
// grid reads from the camera. Alternate tiles are shaded like a checkerboard.
void BuildFloor(const flecs::world& world, const FloorGrid& grid, const flecs::entity parent) {
	const float floorW = static_cast<float>(grid.width) * grid.cellSize;
	const float floorD = static_cast<float>(grid.height) * grid.cellSize;

	world.entity("Floor")
		.child_of(parent)
		.set<spatial::Transform>(
			{.position = grid.origin + glm::vec3{floorW * 0.5F, -kFloorThickness * 0.5F, floorD * 0.5F}}
		)
		.set<render::CubePrimitive>({.size = {floorW, kFloorThickness, floorD}})
		.set<render::Material>({.color = kFloorGroutColour, .cast_shadow = false});

	const float tileSize = grid.cellSize - kFloorTileInset;
	for (int y = 0; y < grid.height; ++y) {
		for (int x = 0; x < grid.width; ++x) {
			const glm::vec3 centre =
				grid.origin + glm::vec3{(static_cast<float>(x) + 0.5F) * grid.cellSize,
										0.0F,
										(static_cast<float>(y) + 0.5F) * grid.cellSize};
			const bool alt = (x + y) % 2 == 1;
			world.entity()
				.child_of(parent)
				.add<FloorTile>()
				.set<spatial::Transform>({.position = centre + glm::vec3{0.0F, 0.005F, 0.0F}})
				.set<render::CubePrimitive>({.size = {tileSize, 0.01F, tileSize}})
				.set<render::Material>(
					{.color = alt ? kFloorTileAltColour : kFloorTileColour, .cast_shadow = false}
				);
		}
	}
}

} // namespace

void RegisterLayoutLoaders(const flecs::world& world) {
	level::RegisterSingletonLoader(world, "floor_grid", [](const flecs::world& w, const nlohmann::json& j) {
		FloorGrid grid{};
		grid.width = j.value("width", grid.width);
		grid.height = j.value("height", grid.height);
		grid.cellSize = j.value("cell_size", grid.cellSize);
		grid.origin = core::JVec3(j, "origin", grid.origin);
		w.set<FloorGrid>(grid);
	});
}

std::expected<void, std::string>
LoadLayout(const flecs::world& world, const std::string& path, const flecs::entity parent) {
	if (std::error_code ec; !std::filesystem::is_regular_file(path, ec)) {
		return std::unexpected(std::format("Layout file not found: {}", path));
	}

	UnloadLayout(world);
	if (level::LoadLevel(world, path, parent) < 0) {
		return std::unexpected(std::format("Layout file {} could not be loaded as a Level", path));
	}

	if (!world.has<FloorGrid>()) {
		return std::unexpected(std::format("Layout file {} has no floor_grid", path));
	}
	const auto& grid = world.get<FloorGrid>();
	if (grid.width <= 0 || grid.height <= 0 || grid.cellSize <= 0.0F) {
		return std::unexpected(std::format(
			"Layout file {} has a bad floor_grid ({}x{} cells of {} m)",
			path,
			grid.width,
			grid.height,
			grid.cellSize
		));
	}

	BuildFloor(world, grid, parent);
	return {};
}

void UnloadLayout(const flecs::world& world) { world.remove<FloorGrid>(); }

} // namespace signal_room
