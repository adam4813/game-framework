#pragma once

#include <glm/glm.hpp>

#include <flecs.h>

// Shells: the XP that defeated enemies drop. A shell rests where it fell until the player comes within
// pickup range, then flies to the player and adds its XP to the run's shell count.
namespace crabnado {

// A dropped shell worth `xp`. `flying` is set once the player comes within pickup range.
struct Shell {
	int xp{0};
	bool flying{false};
};

// Registers and reflects Shell and the ShellPickup system. Call from the module constructor.
void RegisterShells(const flecs::world& world);

// Drops a shell worth `xp` at `position` on the ground, as a child of `parent`.
void DropShell(const flecs::world& world, flecs::entity parent, glm::vec2 position, int xp);

} // namespace crabnado
