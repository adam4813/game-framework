#include "station_systems.hpp"

#include "item_components.hpp"
#include "operator_components.hpp"
#include "room_components.hpp"
#include "station_components.hpp"
#include "station_job.hpp"

namespace signal_room {

void RegisterStationHolding(const flecs::world& world, const flecs::entity roomRoot) {
	world.system<Holding, const OperatorActions>("StationHolding")
		.with<Operator>()
		.write<Holding>()
		.kind(flecs::OnUpdate)
		.each([](flecs::iter& it, const size_t i, Holding& holding, const OperatorActions& actions) {
			const auto station = it.world().entity(holding.station);
			const bool moved = actions.move != glm::vec2{0.0F} || actions.dash;
			const bool working =
				station.is_alive() && station.has<Job>() && station.get<Job>().phase == JobPhase::Running;
			if (!actions.interactHeld || moved || !working) {
				it.entity(i).remove<Holding>();
				return;
			}
			holding.since += it.delta_time();
		})
		.child_of(roomRoot);
}

void RegisterStationSystems(const flecs::world& world, const flecs::entity roomRoot) {
	world.system<const OperatorActions>("StationInteraction")
		.with<Operator>()
		// Read as a field so the sync point after this frame's Target change comes before it.
		.with<Targets>(flecs::Wildcard)
		.in()
		// Its component and pair changes are commands: declaring them puts a sync point before the readers.
		.write<Carries>(flecs::Wildcard)
		.write<Holds>(flecs::Wildcard)
		.write<Holding>()
		.kind(flecs::OnUpdate)
		.each([holders =
				   world.query<const Holding>()](flecs::iter& it, const size_t i, const OperatorActions& actions) {
			if (!actions.interact) {
				return;
			}
			const auto station = it.pair(2).second();
			if (!station.has<Station>() || !station.has<Job>()) {
				return;
			}
			PressStation(it.world(), it.entity(i), station, holders);
		})
		.child_of(roomRoot);

	world.system("StationJobProgress")
		.with<Job>()
		.write<Holds>(flecs::Wildcard)
		.kind(flecs::OnUpdate)
		.each([holders = world.query<const Holding>()](flecs::iter& it, const size_t i) {
			const auto station = it.entity(i);
			const bool held = holders.find([&](const Holding& holding) { return holding.station == station.id(); });
			AdvanceJob(it.world(), station, it.delta_time(), held);
		})
		.child_of(roomRoot);
}

} // namespace signal_room
