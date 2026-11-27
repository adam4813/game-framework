#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <flecs.h>
#include <glm/glm.hpp>

#include "engine/core/core.hpp"

namespace signal_room {

// Identity tag of the Room scene (the one scene Operators play in).
struct RoomSceneTag {};

// World singleton loaded from a Layout's "floor_grid": the 1 m grid everything on the floor snaps to.
// Cell (x, y) spans [origin.x + x * cellSize, +cellSize) east and [origin.z + y * cellSize, +cellSize)
// south; row 0 is the north edge, since the camera looks north (-z).
//
// It also owns the occupancy grid: the entity whose solid footprint covers each cell (0 = open),
// filled from the Layout's Stations, Counters, walls and solid props when the Layout loads.
struct FloorGrid {
	int width{0};
	int height{0};
	float cellSize{1.0F};
	glm::vec3 origin{0.0F};
	std::vector<flecs::entity_t> occupants; // [y * width + x]

	[[nodiscard]] bool InGrid(const glm::ivec2 cell) const {
		return cell.x >= 0 && cell.y >= 0 && cell.x < width && cell.y < height;
	}
	// Cells off the grid count as solid: nothing walks off the floor.
	[[nodiscard]] bool IsSolid(const glm::ivec2 cell) const { return !InGrid(cell) || OccupantAt(cell) != 0; }
	[[nodiscard]] flecs::entity_t OccupantAt(const glm::ivec2 cell) const {
		return InGrid(cell) && !occupants.empty() ? occupants[static_cast<size_t>(cell.y * width + cell.x)] : 0;
	}
	// The cell a floor point (x east, y south) lies in; it may be off the grid.
	[[nodiscard]] glm::ivec2 CellAt(const glm::vec2 point) const {
		return glm::ivec2{glm::floor((point - glm::vec2{origin.x, origin.z}) / cellSize)};
	}
	// A cell's north-west corner on the floor plane (x east, y south); it spans +cellSize each way.
	[[nodiscard]] glm::vec2 CellMin(const glm::ivec2 cell) const {
		return glm::vec2{origin.x, origin.z} + glm::vec2{cell} * cellSize;
	}
	// The floor vector from `point` to the nearest point of a cell (zero when `point` is inside it).
	[[nodiscard]] glm::vec2 ToNearestPointOf(const glm::ivec2 cell, const glm::vec2 point) const {
		const glm::vec2 min = CellMin(cell);
		return glm::clamp(point, min, min + cellSize) - point;
	}
	// World-space centre of a cell on the floor plane.
	[[nodiscard]] glm::vec3 CellCentre(const glm::ivec2 cell) const {
		return origin
			   + glm::vec3{
				   (static_cast<float>(cell.x) + 0.5F) * cellSize,
				   0.0F,
				   (static_cast<float>(cell.y) + 0.5F) * cellSize
			   };
	}
};

// World singleton: which Shift the Room scene loads, and why the last load failed (empty on success).
struct RoomState {
	std::string shiftPath;
	std::string loadError;
	std::string modifierCataloguePath; // empty: the shipped catalogue; tests point it at a fixture
};

// World singleton: which Room panel debug overlays are drawn. Kept across Reloads.
struct RoomOverlays {
	bool floorGrid{false};
	bool solidCells{false};
	bool workedSides{false};
	bool operatorCircle{false};
	bool targetingReach{false};
};

// World singleton: play options toggled from the Room panel, kept across Reloads. `autoGrab`: when an
// attended Job (one that needs the Operator: Hold, Tap, Instant) finishes with outputs, the Operator
// Targeting the Station picks one up if empty-handed. Unattended Jobs (WalkAway) never auto-grab.
struct RoomOptions {
	bool autoGrab{false};
};

// Tag on the floor's cell tiles built from the FloorGrid.
struct FloorTile {};

// The sides of a footprint, as a bitmask (reflected with flecs bit constants).
struct Sides : flecs::bitmask {
	static constexpr uint32_t North = 1U << 0U;
	static constexpr uint32_t East = 1U << 1U;
	static constexpr uint32_t South = 1U << 2U;
	static constexpr uint32_t West = 1U << 3U;
	static constexpr uint32_t All = North | East | South | West;
};

// A Station placement from the Layout. The entity's name is the placement name a Shift lists in
// `in_service`; `kind` keys the greybox catalogue's Stations. Its footprint is solid.
struct Station {
	std::string kind;
	glm::ivec2 cell{0};
	glm::ivec2 footprint{1};
	uint32_t workedSides{0}; // Sides bits
};

// On a Station its Shift doesn't list as in service: solid, drawn as a tarp, never worked.
struct Tarped {};

// A 1×1 solid surface that holds one Item, worked from every side that isn't solid.
struct Counter {
	glm::ivec2 cell{0};
};

// A run of wall cells. `heightClass` keys the greybox catalogue's wall classes (`full`, `low`). Walls may
// stand on the grid or on the one-cell ring around it; their on-grid cells are solid.
struct Wall {
	glm::ivec2 cell{0};
	glm::ivec2 footprint{1};
	std::string heightClass;
};

// Blocks movement over its cells but has no mesh (the open south edge).
struct EdgeBlock {
	glm::ivec2 cell{0};
	glm::ivec2 footprint{1};
};

// Dressing with no game rules. `kind` keys the greybox catalogue's props. A wall-mounted prop has
// no floor footprint (0×0) and is anchored at `cell`. `height` scales a footprint-fitted model
// (0 = the catalogue's height for the kind).
struct Prop {
	std::string kind;
	glm::ivec2 cell{0};
	glm::ivec2 footprint{0};
	bool solid{false};
	float height{0.0F};
};

// A named place an Operator can start (spawn_1, spawn_2, ...), referred to by a Shift's Roster.
struct SpawnMarker {
	glm::ivec2 cell{0};
};

// On each greybox part entity: the colour the catalogue built it in, so a tint drawn over its
// Material (the Target brighten) can be undone.
struct PartColour {
	engine::core::Rgba colour;
};

// On each named greybox part: where the catalogue put it (local), so an animation can offset from it.
struct PartHome {
	glm::vec3 position{0.0F};
};

// On a Station or Counter whose parts are drawn brightened as some Operator's Target. TargetBrighten
// restores the parts and removes it once no Operator targets the placement.
struct Brightened {};

// Problems the Layout's component loaders found while the Level was being built. Loaders can't fail
// the load themselves, so they record here and LoadLayout reports the first one.
struct LayoutErrors {
	std::vector<std::string> errors;
};

} // namespace signal_room
