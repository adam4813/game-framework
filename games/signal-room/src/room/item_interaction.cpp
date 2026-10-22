#include "item_interaction.hpp"

#include <cmath>

#include "engine/engine.hpp"
#include "item_components.hpp"
#include "item_placement.hpp"
#include "operator_components.hpp"
#include "palette.hpp"
#include "room_components.hpp"
#include "room_constants.hpp"
#include "room_tuning.hpp"
#include "station_job.hpp"

using namespace engine;

namespace signal_room {

namespace {

// The yaw that faces from `from` toward the nearest point of `counter`'s cell, or `current` when
// the Operator stands over it.
float FacingToward(const FloorGrid& grid, const Counter& counter, const glm::vec2 from, const float current) {
	const glm::vec2 toward = grid.ToNearestPointOf(counter.cell, from);
	return toward == glm::vec2{0.0F} ? current : std::atan2(-toward.x, -toward.y);
}

// Burst the Counter's dust emitter: its spawn debt is the puff's particle count, and with no rate
// nothing follows the burst.
void PuffDust(const flecs::world& world, const flecs::entity counter) {
	const auto dust = counter.lookup(kDustEmitterName);
	if (!dust || !dust.has<particles::ParticleEmitter>()) {
		return;
	}
	const auto& tuning = world.get<DustPuffTuning>();
	const auto* colour = world.get<Palette>().Get(kDustColourToken);
	const core::Rgba start = colour != nullptr ? *colour : core::Rgba{};
	dust.get_mut<particles::ParticleEmitter>() = {
		.rate = 0.0F,
		.accumulator = std::round(tuning.count),
		.particleLifetime = tuning.lifetime,
		.speed = tuning.speed,
		.startSize = tuning.size,
		.spread = tuning.spread,
		.direction = {0.0F, 1.0F, 0.0F},
		.colorStart = start,
		.colorEnd = {.r = start.r, .g = start.g, .b = start.b, .a = 0},
	};
}

} // namespace

void RegisterItemInteraction(const flecs::world& world, const flecs::entity roomRoot) {
	world.system<const OperatorActions, OperatorMotion, const spatial::Transform>("ItemInteraction")
		.with<Operator>()
		// Read as a field so the sync point after this frame's Target change comes before it.
		.with<Targets>(flecs::Wildcard)
		.in()
		// Its pair changes are commands: declaring them puts a sync point before the systems reading them.
		.write<Carries>(flecs::Wildcard)
		.write<Holds>(flecs::Wildcard)
		.kind(flecs::OnUpdate)
		.each([](flecs::iter& it,
				 const size_t i,
				 const OperatorActions& actions,
				 OperatorMotion& motion,
				 const spatial::Transform& transform) {
			if (!actions.interact) {
				return;
			}
			const auto op = it.entity(i);
			const auto counter = it.pair(4).second();
			if (!counter.has<Counter>()) {
				return;
			}
			const auto world = it.world();
			const auto carried = op.target<Carries>();
			const auto held = counter.target<Holds>();
			if (!carried && held) {
				counter.remove<Holds>(held);
				op.add<Carries>(held);
			}
			else if (carried && !held) {
				op.remove<Carries>(carried);
				counter.add<Holds>(carried);
				PlaceOnCounter(carried, counter);
				PuffDust(it.world(), counter);
			}
			else {
				Refuse(world, op, counter, RefusalReason::CounterFull);
				return;
			}
			const glm::vec2 position{transform.position.x, transform.position.z};
			motion.facing = FacingToward(it.world().get<FloorGrid>(), counter.get<Counter>(), position, motion.facing);
		})
		.add<ecs::Pausable>()
		.child_of(roomRoot);
}

} // namespace signal_room
