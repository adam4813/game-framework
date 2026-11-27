#pragma once

#include <expected>
#include <string>
#include <vector>

#include <flecs.h>

namespace signal_room {

// Register the Layout's game loaders (floor_grid, station, counter, wall, edge_block, prop,
// spawn_marker) with the engine Level registry.
void RegisterLayoutLoaders(const flecs::world& world);

// Load a Layout (an engine Level JSON) under `parent`: check every placement against the
// GreyboxCatalogue singleton and the floor grid, fill the FloorGrid's occupancy grid from the solid
// cells and hand it to the engine GridNavGraph. This is the game's one "load Layout" call, so a
// rename of the engine's Level concept touches only here. Builds no meshes (see BuildLayoutGreybox).
[[nodiscard]] std::expected<void, std::string>
LoadLayout(const flecs::world& world, const std::string& path, flecs::entity parent);

// Tarp every Station under `parent` that `inService` doesn't name. The error names `shiftPath` and
// the first name that isn't a Station placement in the Layout.
[[nodiscard]] std::expected<void, std::string> ApplyInService(
	const flecs::world& world,
	flecs::entity parent,
	const std::vector<std::string>& inService,
	const std::string& shiftPath
);

// Remove what LoadLayout put in the world outside `parent` (the FloorGrid and the nav graph).
void UnloadLayout(const flecs::world& world);

} // namespace signal_room
