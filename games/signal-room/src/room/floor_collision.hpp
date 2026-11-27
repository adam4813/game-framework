#pragma once

#include <span>

#include <glm/glm.hpp>

namespace signal_room {

struct FloorGrid;

// A circle moving on the floor plane (x east, y south). `push` is filled in by MoveCircles: the total
// the solid cells pushed it back by, which points away from whatever it ran into (zero when clear).
struct FloorCircle {
	glm::vec2 centre{0.0F};
	glm::vec2 displacement{0.0F};
	float radius{0.0F};
	glm::vec2 push{0.0F};
};

// Move each circle by its displacement, keeping it out of the FloorGrid's solid cells (off-grid cells
// included, so nothing leaves the floor) and sliding it along whatever it hits. Long moves are split
// into steps shorter than the radius so nothing tunnels through a one-cell Counter. Takes every
// circle at once so circle-to-circle separation can be added here.
void MoveCircles(const FloorGrid& grid, std::span<FloorCircle> circles);

} // namespace signal_room
