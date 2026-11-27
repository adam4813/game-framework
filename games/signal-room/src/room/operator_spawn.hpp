#pragma once

#include <expected>
#include <string>
#include <vector>

#include <flecs.h>

#include "shift.hpp"

namespace signal_room {

// Put the Roster's Operators on the floor under `parent`, in order (Operator1, Operator2, ...): each
// at the centre of its spawn marker's cell, facing north, drawn from the catalogue's operator model in
// its identity colour. The error names `shiftPath` and the first spawn marker the Layout lacks or
// colour token the Palette lacks. Call after LoadLayout.
[[nodiscard]] std::expected<void, std::string> SpawnRoster(
	const flecs::world& world,
	flecs::entity parent,
	const std::vector<RosterEntry>& roster,
	const std::string& shiftPath
);

} // namespace signal_room
