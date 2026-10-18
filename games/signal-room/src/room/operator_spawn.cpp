#include "operator_spawn.hpp"

#include <algorithm>
#include <format>
#include <string_view>

#include "engine/engine.hpp"
#include "greybox_catalogue.hpp"
#include "layout_greybox.hpp"
#include "operator_components.hpp"
#include "palette.hpp"
#include "placements.hpp"
#include "room_components.hpp"

using namespace engine;

namespace signal_room {

std::expected<void, std::string> SpawnRoster(
	const flecs::world& world,
	const flecs::entity parent,
	const std::vector<RosterEntry>& roster,
	const std::string& shiftPath
) {
	const auto markers = PlacementsOf<SpawnMarker>(world, parent);
	const auto& grid = world.get<FloorGrid>();
	const auto& palette = world.get<Palette>();
	const auto& model = world.get<GreyboxCatalogue>().operatorModel;

	for (size_t i = 0; i < roster.size(); ++i) {
		const auto& entry = roster[i];
		const auto marker = std::ranges::find_if(markers, [&](const auto& placement) {
			return std::string_view{placement.first.name().c_str()} == entry.spawn;
		});
		if (marker == markers.end()) {
			return std::unexpected(
				std::format(
					"Shift file {}: roster entry {} names spawn marker '{}', but its Layout has none",
					shiftPath,
					i + 1,
					entry.spawn
				)
			);
		}
		const auto* colour = palette.Get(entry.colour);
		if (colour == nullptr) {
			return std::unexpected(
				std::format(
					"Shift file {}: roster entry {} uses unknown palette colour '{}'",
					shiftPath,
					i + 1,
					entry.colour
				)
			);
		}

		const auto op = world.entity()
							.child_of(parent)
							.set_name(std::format("Operator{}", i + 1).c_str())
							.set<Operator>({.colour = entry.colour})
							.set<OperatorActions>({})
							.set<OperatorMotion>({})
							.set<Dash>({})
							.set<spatial::Transform>({.position = grid.CellCentre(marker->second.cell)});
		BuildGreyboxModel(world, op, model, glm::vec3{1.0F}, *colour);
	}
	return {};
}

} // namespace signal_room
