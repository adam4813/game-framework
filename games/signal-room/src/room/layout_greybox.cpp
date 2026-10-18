#include "layout_greybox.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>

#include "engine/engine.hpp"
#include "greybox_catalogue.hpp"
#include "placements.hpp"
#include "room_components.hpp"
#include "room_constants.hpp"

using namespace engine;

namespace signal_room {

namespace {

// World-space centre of a footprint on the floor plane.
glm::vec3 FootprintCentre(const FloorGrid& grid, const glm::ivec2 cell, const glm::ivec2 footprint) {
	const glm::vec2 centre = (glm::vec2{cell} + glm::vec2{footprint} * 0.5F) * grid.cellSize;
	return grid.origin + glm::vec3{centre.x, 0.0F, centre.y};
}

glm::vec3 FitFor(const GreyboxModel& model, const FloorGrid& grid, const glm::ivec2 footprint, const float height) {
	if (!model.fitFootprint) {
		return glm::vec3{1.0F};
	}
	return {static_cast<float>(footprint.x) * grid.cellSize, height, static_cast<float>(footprint.y) * grid.cellSize};
}

// A grout slab the size of the whole floor, with one slightly inset tile per cell on top so the 1 m
// grid reads from the camera. Alternate tiles are shaded like a checkerboard.
void BuildFloor(const flecs::world& world, const FloorGrid& grid, const FloorStyle& style, const flecs::entity parent) {
	const float floorW = static_cast<float>(grid.width) * grid.cellSize;
	const float floorD = static_cast<float>(grid.height) * grid.cellSize;

	world.entity()
		.child_of(parent)
		.set_name("Floor")
		.set<spatial::Transform>(
			{.position = grid.origin + glm::vec3{floorW * 0.5F, -kFloorThickness * 0.5F, floorD * 0.5F}}
		)
		.set<render::CubePrimitive>({.size = {floorW, kFloorThickness, floorD}})
		.set<render::Material>({.color = style.grout, .cast_shadow = false});

	const float tileSize = grid.cellSize - kFloorTileInset;
	for (int y = 0; y < grid.height; ++y) {
		for (int x = 0; x < grid.width; ++x) {
			const bool alt = (x + y) % 2 == 1;
			world.entity()
				.child_of(parent)
				.add<FloorTile>()
				.set<spatial::Transform>({.position = grid.CellCentre({x, y}) + glm::vec3{0.0F, 0.005F, 0.0F}})
				.set<render::CubePrimitive>({.size = {tileSize, 0.01F, tileSize}})
				.set<render::Material>({.color = alt ? style.tileAlt : style.tile, .cast_shadow = false});
		}
	}
}

// A wall run is drawn as a slab `thickness` thick standing against the floor edge: its cells are
// clipped to the floor grown by the thickness, so a run on the ring around the grid hugs the floor.
void BuildWall(const flecs::entity e, const FloorGrid& grid, const Wall& wall, const WallStyle& style) {
	const glm::vec2 floorMin{grid.origin.x, grid.origin.z};
	const glm::vec2 floorMax =
		floorMin + glm::vec2{static_cast<float>(grid.width), static_cast<float>(grid.height)} * grid.cellSize;
	const glm::vec2 cellsMin = floorMin + glm::vec2{wall.cell} * grid.cellSize;
	const glm::vec2 cellsMax = cellsMin + glm::vec2{wall.footprint} * grid.cellSize;
	const glm::vec2 min = glm::max(cellsMin, floorMin - style.thickness);
	const glm::vec2 max = glm::min(cellsMax, floorMax + style.thickness);
	const glm::vec2 centre = (min + max) * 0.5F;
	const glm::vec2 extent = max - min;

	e.set<spatial::Transform>({.position = {centre.x, grid.origin.y + style.height * 0.5F, centre.y}})
		.set<render::CubePrimitive>({.size = {extent.x, style.height, extent.y}})
		.set<render::Material>({.color = style.colour});
}

} // namespace

// A placement's owner stands at its footprint centre; for fitted models `fit` is the footprint size
// and height.
void BuildGreyboxModel(
	const flecs::world& world,
	const flecs::entity owner,
	const GreyboxModel& model,
	const glm::vec3 fit,
	const core::Rgba identity
) {
	std::unordered_map<std::string, flecs::entity> named;
	for (const auto& part : model.parts) {
		auto e = world.entity().child_of(part.parent.empty() ? owner : named.at(part.parent));
		if (!part.name.empty()) {
			e.set_name(part.name.c_str());
			named.emplace(part.name, e);
		}
		const glm::vec3 size = part.size * fit;
		spatial::Transform transform{.position = part.offset * fit, .rotation = part.rotation};
		switch (part.shape) {
		case PartShape::Cube: e.set<render::CubePrimitive>({.size = size}); break;
		case PartShape::Sphere:
			e.set<render::SpherePrimitive>({.radius = 0.5F});
			transform.scale = size;
			break;
		case PartShape::Capsule:
			e.set<render::CapsulePrimitive>({.radius = size.x * 0.5F, .height = std::max(0.0F, size.y - size.x)});
			break;
		}
		e.set<spatial::Transform>(transform).set<render::Material>({.color = part.identity ? identity : part.colour});
		if (!part.visible) {
			e.disable();
		}
	}
}

void BuildLayoutGreybox(const flecs::world& world, const flecs::entity parent) {
	const auto& grid = world.get<FloorGrid>();
	const auto& catalogue = world.get<GreyboxCatalogue>();
	const auto& tarp = *catalogue.props.Get(kTarpPropKind);

	BuildFloor(world, grid, catalogue.floor, parent);

	for (const auto& [e, s] : PlacementsOf<Station>(world, parent)) {
		e.set<spatial::Transform>({.position = FootprintCentre(grid, s.cell, s.footprint)});
		const auto& model = *catalogue.stations.Get(s.kind);
		const auto& look = e.has<Tarped>() ? tarp : model;
		BuildGreyboxModel(world, e, look, FitFor(look, grid, s.footprint, model.height));
	}

	for (const auto& [e, c] : PlacementsOf<Counter>(world, parent)) {
		e.set<spatial::Transform>({.position = grid.CellCentre(c.cell)});
		BuildGreyboxModel(world, e, catalogue.counter, FitFor(catalogue.counter, grid, {1, 1}, catalogue.counter.height));
	}

	for (const auto& [e, p] : PlacementsOf<Prop>(world, parent)) {
		const glm::ivec2 footprint = AnchorFootprint(p);
		e.set<spatial::Transform>({.position = FootprintCentre(grid, p.cell, footprint)});
		const auto& model = *catalogue.props.Get(p.kind);
		BuildGreyboxModel(world, e, model, FitFor(model, grid, footprint, p.height > 0.0F ? p.height : model.height));
	}

	for (const auto& [e, w] : PlacementsOf<Wall>(world, parent)) {
		BuildWall(e, grid, w, *catalogue.walls.Get(w.heightClass));
	}

	for (const auto& [e, m] : PlacementsOf<SpawnMarker>(world, parent)) {
		e.set<spatial::Transform>({.position = grid.CellCentre(m.cell)});
	}
}

} // namespace signal_room
