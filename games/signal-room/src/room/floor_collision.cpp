#include "floor_collision.hpp"

#include <algorithm>
#include <cmath>

#include "room_components.hpp"

namespace signal_room {

namespace {

// Corners can need a second or third push once the first moves the circle into a neighbour.
constexpr int kResolveIterations = 4;
// The largest single step, as a fraction of the radius.
constexpr float kMaxStepFraction = 0.5F;
constexpr float kContactEpsilon = 1e-6F;

// Push `centre` out of every solid cell its circle overlaps; returns the total push.
glm::vec2 PushOutOfSolidCells(const FloorGrid& grid, glm::vec2& centre, const float radius) {
	const glm::vec2 origin{grid.origin.x, grid.origin.z};
	glm::vec2 total{0.0F};
	for (int iteration = 0; iteration < kResolveIterations; ++iteration) {
		bool pushed = false;
		const glm::ivec2 minCell{glm::floor((centre - radius - origin) / grid.cellSize)};
		const glm::ivec2 maxCell{glm::floor((centre + radius - origin) / grid.cellSize)};
		for (int y = minCell.y; y <= maxCell.y; ++y) {
			for (int x = minCell.x; x <= maxCell.x; ++x) {
				if (!grid.IsSolid({x, y})) {
					continue;
				}
				const glm::vec2 boxMin = origin + glm::vec2{x, y} * grid.cellSize;
				const glm::vec2 boxMax = boxMin + grid.cellSize;
				const glm::vec2 away = centre - glm::clamp(centre, boxMin, boxMax);
				const float distanceSq = glm::dot(away, away);
				if (distanceSq >= radius * radius) {
					continue;
				}

				glm::vec2 push;
				if (distanceSq > kContactEpsilon) {
					const float distance = std::sqrt(distanceSq);
					push = away / distance * (radius - distance);
				}
				else {
					// The centre is inside the cell: leave by the nearest face.
					const float toWest = centre.x - boxMin.x;
					const float toEast = boxMax.x - centre.x;
					const float toNorth = centre.y - boxMin.y;
					const float toSouth = boxMax.y - centre.y;
					const float nearest = std::min({toWest, toEast, toNorth, toSouth});
					if (nearest == toWest) {
						push = {-(toWest + radius), 0.0F};
					}
					else if (nearest == toEast) {
						push = {toEast + radius, 0.0F};
					}
					else if (nearest == toNorth) {
						push = {0.0F, -(toNorth + radius)};
					}
					else {
						push = {0.0F, toSouth + radius};
					}
				}
				centre += push;
				total += push;
				pushed = true;
			}
		}
		if (!pushed) {
			break;
		}
	}
	return total;
}

} // namespace

void MoveCircles(const FloorGrid& grid, const std::span<FloorCircle> circles) {
	for (auto& circle : circles) {
		circle.push = glm::vec2{0.0F};
		const float maxStep = std::max(circle.radius * kMaxStepFraction, kContactEpsilon);
		const int steps = std::max(1, static_cast<int>(std::ceil(glm::length(circle.displacement) / maxStep)));
		const glm::vec2 step = circle.displacement / static_cast<float>(steps);
		for (int i = 0; i < steps; ++i) {
			circle.centre += step;
			circle.push += PushOutOfSolidCells(grid, circle.centre, circle.radius);
		}
	}
}

} // namespace signal_room
