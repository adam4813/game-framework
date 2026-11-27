#include "room_overlays.hpp"

#include <array>
#include <cmath>

#include "engine/engine.hpp"
#include "operator_components.hpp"
#include "palette.hpp"
#include "placements.hpp"
#include "room_components.hpp"
#include "room_tuning.hpp"

using namespace engine;

namespace signal_room {

namespace {

constexpr float kLineWidth = 0.025F;
constexpr float kLineY = 0.03F;
constexpr float kSideBarWidth = 0.08F;
constexpr float kSolidMarkerY = 2.0F;
constexpr float kSolidMarkerSize = 0.5F;
constexpr const char* kOperatorCircleName = "CollisionCircle";
constexpr const char* kTargetingReachName = "TargetingReach";
// The reach lines float a little above the collision circle so both read when shown together.
constexpr float kReachY = 0.06F;
constexpr int kConeArcSegments = 10;

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

// Replace each Operator's overlay child `name` with a fresh one built by `build(child)` when `show`.
// It hangs off the Operator so it follows (and turns with) it; the rest of the overlays are static and
// live in the Overlays group.
template<typename Build>
void BuildOperatorOverlay(
	const flecs::world& world,
	const flecs::entity roomRoot,
	const char* name,
	const bool show,
	const Build& build
) {
	for (const auto& [e, op] : PlacementsOf<Operator>(world, roomRoot)) {
		if (auto old = e.lookup(name)) {
			old.destruct();
		}
		if (show) {
			build(world.entity().child_of(e).set_name(name));
		}
	}
}

// A flat disc under each Operator the size of its collision circle.
void BuildOperatorCircles(const flecs::world& world, const flecs::entity roomRoot, const bool show) {
	const auto colour = OverlayColour(world, "overlay_operator", {.r = 245, .g = 245, .b = 245, .a = 255});
	const float diameter = world.get<MovementTuning>().radius * 2.0F;
	BuildOperatorOverlay(world, roomRoot, kOperatorCircleName, show, [&](const flecs::entity circle) {
		circle.set<spatial::Transform>({.position = {0.0F, kLineY, 0.0F}, .scale = {diameter, 0.01F, diameter}})
			.set<render::SpherePrimitive>({.radius = 0.5F})
			.set<render::Material>({.color = colour, .cast_shadow = false});
	});
}

// A bar lying on the floor from the Operator's centre out to `length`, turned `yaw` from its facing.
void AddReachBar(
	const flecs::world& world,
	const flecs::entity group,
	const float yaw,
	const float length,
	const core::Rgba colour
) {
	const auto pivot = world.entity().child_of(group).set<spatial::Transform>({.rotation = {0.0F, yaw, 0.0F}});
	AddBox(world, pivot, {0.0F, kReachY, -length * 0.5F}, {kLineWidth, 0.01F, length}, colour);
}

// Each Operator's targeting reach in front of it: the facing ray, and the fallback cone's two edges and
// its arc at the cone range.
void BuildTargetingReach(const flecs::world& world, const flecs::entity roomRoot, const bool show) {
	const auto rayColour = OverlayColour(world, "overlay_ray", {.r = 250, .g = 220, .b = 60, .a = 255});
	const auto coneColour = OverlayColour(world, "overlay_cone", {.r = 250, .g = 150, .b = 60, .a = 255});
	const auto& tuning = world.get<TargetingTuning>();
	const float halfAngle = glm::radians(tuning.coneHalfAngle);
	const float range = tuning.coneRange;
	BuildOperatorOverlay(world, roomRoot, kTargetingReachName, show, [&](const flecs::entity group) {
		group.set<spatial::Transform>({});
		AddReachBar(world, group, 0.0F, tuning.rayLength, rayColour);
		AddReachBar(world, group, halfAngle, range, coneColour);
		AddReachBar(world, group, -halfAngle, range, coneColour);
		const float step = 2.0F * halfAngle / static_cast<float>(kConeArcSegments);
		const float chord = 2.0F * range * std::sin(step * 0.5F);
		for (int i = 0; i < kConeArcSegments; ++i) {
			const float yaw = -halfAngle + (static_cast<float>(i) + 0.5F) * step;
			const auto pivot = world.entity().child_of(group).set<spatial::Transform>({.rotation = {0.0F, yaw, 0.0F}});
			AddBox(
				world,
				pivot,
				{0.0F, kReachY, -range * std::cos(step * 0.5F)},
				{chord, 0.01F, kLineWidth},
				coneColour
			);
		}
	});
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
	BuildOperatorCircles(world, roomRoot, overlays.operatorCircle);
	BuildTargetingReach(world, roomRoot, overlays.targetingReach);
}

} // namespace signal_room
