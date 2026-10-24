#pragma once

#include <glm/glm.hpp>

#include <flecs.h>

// Crabnado's action layer: key bindings mapped to game actions. Gameplay systems read PlayerIntent,
// never raw key codes.
namespace crabnado {

// Singleton: what the player is asking for this frame. `move` is on the ground plane (x east, y south,
// i.e. world +z), each axis in [-1, 1]. `pick` is the molt upgrade selected this frame (1-3), or 0. `dig_in`
// is true while the dig-in action is held. `pause` is true on the frame the pause action is pressed, and
// `confirm` on the frame the confirm action (accepting the selected molt upgrade) is pressed.
struct PlayerIntent {
	glm::vec2 move{0.0F};
	int pick{0};
	bool dig_in{false};
	bool pause{false};
	bool confirm{false};
};

// Registers and reflects PlayerIntent and the ReadControls system (PreUpdate, not pausable).
void RegisterControls(const flecs::world& world);

} // namespace crabnado
