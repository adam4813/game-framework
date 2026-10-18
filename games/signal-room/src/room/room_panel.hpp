#pragma once

#include <flecs.h>

namespace signal_room {

// ImGui Room panel (main thread): the current Shift and its load error, the floor grid, debug overlay
// toggles, the Shift files to switch between, and a Reload button. Created as a child of the Room's root entity so it
// exists only while the Room scene is loaded.
void RegisterRoomPanel(const flecs::world& world, flecs::entity roomRoot);

} // namespace signal_room
