#include "room_overlays.hpp"

#include <array>

#include "engine/engine.hpp"
#include "palette.hpp"
#include "placements.hpp"
#include "room_components.hpp"

using namespace engine;

namespace signal_room {

namespace {

constexpr float kLineWidth = 0.025F;
constexpr float kLineY = 0.03F;
constexpr float kSideBarWidth = 0.08F;
constexpr float kSolidMarkerY = 2.0F;
constexpr float kSolidMarkerSize = 0.5F;

// Overlay colours are palette tokens too; a palette without them still draws the overlay.
core::Rgba OverlayColour(const flecs::world& world, const char* token, const core::Rgba fallback) {
	const auto* colour = world.get<Palette>().Get(token);
	return colour != nullptr ? *colour : fallback;
}

void AddBox(
	const flecs::world& world,
	const flecs::entity group,
	const glm::vec3 centre,
	const glm::vec3 size,
	const core::Rgba colour
) {
	world.entity()
		.child_of(group)
		.set<spatial::Transform>({.position = centre})
		.set<render::CubePrimitive>({.size = size})
		.set<render::Material>({.color = colour, .cast_shadow = false});
}

void BuildGridLines(const flecs::world& world, const flecs::entity group, const FloorGrid& grid) {
	const auto colour = OverlayColour(world, "overlay_grid", {.r = 20, .g = 20, .b = 20, .a = 255});
	const float w = static_cast<float>(grid.width) * grid.cellSize;
	const float d = static_cast<float>(grid.height) * grid.cellSize;
	for (int x = 0; x <= grid.width; ++x) {
		const float px = static_cast<float>(x) * grid.cellSize;
		AddBox(world, group, grid.origin + glm::vec3{px, kLineY, d * 0.5F}, {kLineWidth, 0.01F, d}, colour);
	}
	for (int y = 0; y <= grid.height; ++y) {
		const float pz = static_cast<float>(y) * grid.cellSize;
		AddBox(world, group, grid.origin + glm::vec3{w * 0.5F, kLineY, pz}, {w, 0.01F, kLineWidth}, colour);
	}
}

// The Room camera's view direction (from the camera to its look_at target), or straight down when
// the Layout has no aimed camera. Reads the local Transform on purpose: overlays are built during Room
// load, before TransformPropagation has filled WorldTransform, and the camera and its target are
// top-level Layout entities (under the unmoved RoomRoot), so local is world.
glm::vec3 ViewDirection(const flecs::world& world, const flecs::entity roomRoot) {
	glm::vec3 view{0.0F, -1.0F, 0.0F};
	world.query_builder<const spatial::Transform, const render::Camera>()
		.with(flecs::ChildOf, roomRoot)
		.build()
		.each([&](const flecs::entity camera, const spatial::Transform& eye, const render::Camera&) {
			const auto target = camera.target<render::LookAt>();
			if (target && target.has<spatial::Transform>()) {
				const glm::vec3 dir = target.get<spatial::Transform>().position - eye.position;
				view = dir.y < 0.0F ? glm::normalize(dir) : view;
			}
		});
	return view;
}

void BuildSolidCells(
	const flecs::world& world,
	const flecs::entity group,
	const flecs::entity roomRoot,
	const FloorGrid& grid
) {
	const auto colour = OverlayColour(world, "overlay_solid", {.r = 230, .g = 60, .b = 50, .a = 255});
	// A flat marker floating over each solid cell, above the furniture that would hide a floor mark
	// (wireframe would read better, but WebGL has no wire mode). Each marker slides back along the
	// camera's view so it appears over its own cell rather than one further north.
	const glm::vec3 view = ViewDirection(world, roomRoot);
	const glm::vec3 lift = -view * (kSolidMarkerY / -view.y);
	const float size = grid.cellSize * kSolidMarkerSize;
	for (int y = 0; y < grid.height; ++y) {
		for (int x = 0; x < grid.width; ++x) {
			if (grid.IsSolid({x, y})) {
				AddBox(world, group, grid.CellCentre({x, y}) + lift, {size, 0.02F, size}, colour);
			}
		}
	}
}

// One bar per worked side, lying on the floor just outside the footprint edge.
void AddSideBars(
	const flecs::world& world,
	const flecs::entity group,
	const FloorGrid& grid,
	const glm::ivec2 cell,
	const glm::ivec2 footprint,
	const uint32_t sides,
	const core::Rgba colour
) {
	const glm::vec2 min = glm::vec2{cell} * grid.cellSize;
	const glm::vec2 max = glm::vec2{cell + footprint} * grid.cellSize;
	const glm::vec2 mid = (min + max) * 0.5F;
	const glm::vec2 span = max - min;
	const float off = kSideBarWidth * 0.5F;
	struct Bar {
		uint32_t side;
		glm::vec2 centre;
		glm::vec2 size;
	};
	const std::array bars{
		Bar{Sides::North, {mid.x, min.y - off}, {span.x, kSideBarWidth}},
		Bar{Sides::South, {mid.x, max.y + off}, {span.x, kSideBarWidth}},
		Bar{Sides::West, {min.x - off, mid.y}, {kSideBarWidth, span.y}},
		Bar{Sides::East, {max.x + off, mid.y}, {kSideBarWidth, span.y}},
	};
	for (const auto& bar : bars) {
		if ((sides & bar.side) != 0) {
			AddBox(
				world,
				group,
				grid.origin + glm::vec3{bar.centre.x, kLineY, bar.centre.y},
				{bar.size.x, 0.02F, bar.size.y},
				colour
			);
		}
	}
}

// A Counter is worked from each side whose neighbouring cell is open.
uint32_t OpenSides(const FloorGrid& grid, const glm::ivec2 cell) {
	uint32_t sides = 0;
	sides |= grid.IsSolid(cell + glm::ivec2{0, -1}) ? 0 : Sides::North;
	sides |= grid.IsSolid(cell + glm::ivec2{0, 1}) ? 0 : Sides::South;
	sides |= grid.IsSolid(cell + glm::ivec2{-1, 0}) ? 0 : Sides::West;
	sides |= grid.IsSolid(cell + glm::ivec2{1, 0}) ? 0 : Sides::East;
	return sides;
}

void BuildWorkedSides(
	const flecs::world& world,
	const flecs::entity group,
	const flecs::entity roomRoot,
	const FloorGrid& grid
) {
	const auto colour = OverlayColour(world, "overlay_worked", {.r = 90, .g = 220, .b = 110, .a = 255});
	for (const auto& [e, s] : PlacementsOf<Station>(world, roomRoot)) {
		if (!e.has<Tarped>()) {
			AddSideBars(world, group, grid, s.cell, s.footprint, s.workedSides, colour);
		}
	}
	for (const auto& [e, c] : PlacementsOf<Counter>(world, roomRoot)) {
		AddSideBars(world, group, grid, c.cell, {1, 1}, OpenSides(grid, c.cell), colour);
	}
}

} // namespace

void BuildRoomOverlays(const flecs::world& world, const flecs::entity roomRoot) {
	auto group = roomRoot.lookup("Overlays");
	if (group) {
		world.delete_with(flecs::ChildOf, group);
	}
	else {
		group = world.entity().child_of(roomRoot).set_name("Overlays");
	}
	if (!world.has<FloorGrid>()) {
		return;
	}

	const auto& grid = world.get<FloorGrid>();
	const auto& overlays = world.get<RoomOverlays>();
	if (overlays.floorGrid) {
		BuildGridLines(world, group, grid);
	}
	if (overlays.solidCells) {
		BuildSolidCells(world, group, roomRoot, grid);
	}
	if (overlays.workedSides) {
		BuildWorkedSides(world, group, roomRoot, grid);
	}
}

} // namespace signal_room
