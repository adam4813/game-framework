#pragma once

#include <glm/glm.hpp>

#include <flecs.h>

// Crabnado's action layer: key bindings mapped to game actions. Gameplay systems read PlayerIntent,
// never raw key codes.
namespace crabnado {

// Singleton: what the player is asking for this frame. `move` is on the ground plane (x east, y south,
// i.e. world +z), each axis in [-1, 1].
struct PlayerIntent {
	glm::vec2 move{0.0F};
};

// Registers and reflects PlayerIntent and the ReadControls system (PreUpdate, not pausable).
void RegisterControls(const flecs::world& world);

} // namespace crabnado
