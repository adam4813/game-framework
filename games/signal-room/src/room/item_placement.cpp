#include "item_placement.hpp"

#include "engine/engine.hpp"
#include "greybox_catalogue.hpp"
#include "item_components.hpp"
#include "operator_components.hpp"
#include "room_components.hpp"
#include "room_constants.hpp"

using namespace engine;

namespace signal_room {

void PlaceOnCounter(const flecs::entity item, const flecs::entity counter) {
	const float top = counter.world().get<GreyboxCatalogue>().counter.Top();
	item.set<spatial::Transform>({.position = counter.get<spatial::Transform>().position + glm::vec3{0.0F, top, 0.0F}});
}

void PlaceOnStation(const flecs::entity item, const flecs::entity station) {
	const auto* model = station.world().get<GreyboxCatalogue>().stations.Get(station.get<Station>().kind);
	if (model != nullptr && model->itemSlide) {
		item.set<spatial::Transform>({.position = station.get<spatial::Transform>().position + model->itemSlide->from});
		return;
	}
	const float southEdge = static_cast<float>(station.get<Station>().footprint.y) * 0.5F;
	item.set<spatial::Transform>(
		{.position = station.get<spatial::Transform>().position
					 + glm::vec3{0.0F, kStationItemHeight, southEdge - kStationItemInset}}
	);
}

void RegisterCarriedItemPlacement(const flecs::world& world, const flecs::entity roomRoot) {
	world
		.system<const OperatorMotion, const spatial::Transform>("CarriedItemPlacement")
		// Read as a field so the sync point after this frame's pick-ups comes before it.
		.with<Carries>(flecs::Wildcard)
		.in()
		.kind(flecs::OnUpdate)
		.each([](flecs::iter& it, size_t, const OperatorMotion& motion, const spatial::Transform& carrier) {
			const auto item = it.pair(2).second();
			const glm::vec2 ahead = FacingVector(motion.facing) * kCarryAhead;
			auto& transform = item.get_mut<spatial::Transform>();
			transform.position = carrier.position + glm::vec3{ahead.x, kCarryHeight, ahead.y};
			transform.rotation = item.get<Item>().tiltToCamera ? glm::vec3{kCarryTiltToCamera, 0.0F, 0.0F}
															   : glm::vec3{0.0F, motion.facing, 0.0F};
		})
		.add<ecs::Pausable>()
		.child_of(roomRoot);
}

} // namespace signal_room
