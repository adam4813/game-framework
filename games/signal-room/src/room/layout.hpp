#pragma once

#include <expected>
#include <string>

#include <flecs.h>

namespace signal_room {

// Register the Layout's game loaders (floor_grid, ...) with the engine Level registry.
void RegisterLayoutLoaders(const flecs::world& world);

// Load a Layout (an engine Level JSON) under `parent` and build the floor from its FloorGrid. This
// is the game's one "load Layout" call, so a rename of the engine's Level concept touches only here.
[[nodiscard]] std::expected<void, std::string>
LoadLayout(const flecs::world& world, const std::string& path, flecs::entity parent);

// Remove what LoadLayout put in the world outside `parent` (the FloorGrid singleton).
void UnloadLayout(const flecs::world& world);

} // namespace signal_room
