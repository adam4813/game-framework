#pragma once

#include <string>

#include <glm/glm.hpp>

namespace signal_room {

// Identity tag of the Room scene (the one scene Operators play in).
struct RoomSceneTag {};

// World singleton loaded from a Layout's "floor_grid": the 1 m grid everything on the floor snaps to.
// Cell (x, y) spans [origin.x + x * cellSize, +cellSize) east and [origin.z + y * cellSize, +cellSize)
// south; row 0 is the north edge, since the camera looks north (-z).
struct FloorGrid {
	int width{0};
	int height{0};
	float cellSize{1.0F};
	glm::vec3 origin{0.0F};
};

// World singleton: which Shift the Room scene loads, and why the last load failed (empty on success).
struct RoomState {
	std::string shiftPath;
	std::string loadError;
};

// Tag on the floor's cell tiles built from the FloorGrid.
struct FloorTile {};

} // namespace signal_room
