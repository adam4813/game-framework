#include "operator_targeting.hpp"

#include "engine/engine.hpp"
#include "operator_components.hpp"
#include "room_components.hpp"
#include "room_tuning.hpp"

using namespace engine;

namespace signal_room {

namespace {

// Whether `position` is beyond one of the Station's worked sides and level with that side: due north,
// east, south or west of its footprint, not off a corner.
bool OnWorkedSide(const FloorGrid& grid, const Station& station, const glm::vec2 position) {
	const glm::vec2 min = grid.CellMin(station.cell);
	const glm::vec2 max = min + glm::vec2{station.footprint} * grid.cellSize;
	const bool levelX = position.x >= min.x && position.x <= max.x;
	const bool levelY = position.y >= min.y && position.y <= max.y;
	return ((station.workedSides & Sides::North) != 0 && levelX && position.y < min.y)
		   || ((station.workedSides & Sides::South) != 0 && levelX && position.y > max.y)
		   || ((station.workedSides & Sides::West) != 0 && levelY && position.x < min.x)
		   || ((station.workedSides & Sides::East) != 0 && levelY && position.x > max.x);
}

// Whether an Operator at `position` could work the cell's occupant: a Counter, or an in-service
// Station from one of its worked sides.
bool IsWorkable(
	const flecs::world& world,
	const FloorGrid& grid,
	const flecs::entity_t occupant,
	const glm::vec2 position
) {
	if (occupant == 0) {
		return false;
	}
	const auto e = world.entity(occupant);
	if (e.has<Counter>()) {
		return true;
	}
	return e.has<Station>() && !e.has<Tarped>() && OnWorkedSide(grid, e.get<Station>(), position);
}

flecs::entity_t PickTarget(
	const flecs::world& world,
	const FloorGrid& grid,
	const TargetingTuning& tuning,
	const glm::vec2 position,
	const glm::vec2 facing
) {
	const auto faced = grid.OccupantAt(grid.CellAt(position + facing * tuning.rayLength));
	if (IsWorkable(world, grid, faced, position)) {
		return faced;
	}

	// The fallback: every cell within reach whose nearest point lies inside the cone. A thing is as
	// near as its nearest such cell, so a big Station counts by the part of it the Operator is aimed at.
	const float minCos = std::cos(glm::radians(tuning.coneHalfAngle));
	const glm::ivec2 minCell = grid.CellAt(position - tuning.coneRange);
	const glm::ivec2 maxCell = grid.CellAt(position + tuning.coneRange);
	flecs::entity_t nearest = 0;
	float nearestDistance = tuning.coneRange;
	for (int y = minCell.y; y <= maxCell.y; ++y) {
		for (int x = minCell.x; x <= maxCell.x; ++x) {
			const auto occupant = grid.OccupantAt({x, y});
			if (!IsWorkable(world, grid, occupant, position)) {
				continue;
			}
			const glm::vec2 toCell = grid.ToNearestPointOf({x, y}, position);
			const float distance = glm::length(toCell);
			if (distance > nearestDistance || (distance > 0.0F && glm::dot(toCell / distance, facing) < minCos)) {
				continue;
			}
			nearest = occupant;
			nearestDistance = distance;
		}
	}
	return nearest;
}

} // namespace

void RegisterOperatorTargeting(const flecs::world& world, const flecs::entity roomRoot) {
	world.system<const OperatorMotion, const spatial::Transform>("OperatorTargeting")
		.with<Operator>()
		// Its pair changes are commands: declaring them puts a sync point before the systems reading them.
		.write<Targets>(flecs::Wildcard)
		.kind(flecs::OnUpdate)
		.each([](const flecs::entity op, const OperatorMotion& motion, const spatial::Transform& transform) {
			const auto world = op.world();
			if (!world.has<FloorGrid>()) {
				return;
			}
			const glm::vec2 position{transform.position.x, transform.position.z};
			const auto target = PickTarget(
				world,
				world.get<FloorGrid>(),
				world.get<TargetingTuning>(),
				position,
				FacingVector(motion.facing)
			);
			if (target == op.target<Targets>()) {
				return;
			}
			if (target != 0) {
				op.add<Targets>(target);
			}
			else {
				op.remove<Targets>(flecs::Wildcard);
			}
		})
		.child_of(roomRoot);
}

} // namespace signal_room
