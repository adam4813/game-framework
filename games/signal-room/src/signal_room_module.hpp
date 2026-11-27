#pragma once

#include <flecs.h>

namespace signal_room {

// The Signal Room game module: imports the engine modules the game uses (no physics), pre-registers
// and reflects every game component, registers the Layout loaders and the Room scene. It does not
// activate a scene; call StartRoom (room/room_scene.hpp) to open the Room on a Shift.
class SignalRoomModule {
public:
	explicit SignalRoomModule(flecs::world& world);
};

} // namespace signal_room
