#pragma once

#include <utility>
#include <vector>

#include <flecs.h>
#include <glm/glm.hpp>

#include "room_components.hpp"

namespace signal_room {

// Every direct child of `parent` carrying `T` (a Layout placement: Station, Counter, Prop, ...), with
// a copy of its `T`. Collected up front so callers can add components to the entities freely, which
// they can't while a query is iterating.
template<typename T>
std::vector<std::pair<flecs::entity, T>> PlacementsOf(const flecs::world& world, const flecs::entity parent) {
	std::vector<std::pair<flecs::entity, T>> placements;
	world.query_builder<const T>().with(flecs::ChildOf, parent).build().each([&](const flecs::entity e, const T& t) {
		placements.emplace_back(e, t);
	});
	return placements;
}

// The cells a prop is placed over: its floor footprint, or its 1x1 anchor cell when wall-mounted.
inline glm::ivec2 AnchorFootprint(const Prop& prop) {
	return prop.footprint == glm::ivec2{0} ? glm::ivec2{1} : prop.footprint;
}

// A wall-mounted prop has no floor footprint.
inline bool IsWallMounted(const Prop& prop) { return prop.footprint == glm::ivec2{0}; }

} // namespace signal_room
