#pragma once

#include <flecs.h>

// The boss: when the Act reaches its Boss Stage the crabnado comes ashore and chases the player, and a big crest that
// pushes it into a rock is a slam. Enough slams beat it (the tunables are in bosses.json): the horde is swept, shells are
// vacuumed and it retreats to its orbit. It is the same Crabnado entity; its orbit gives way to the Breach and then this
// Fight while RunState::phase is Boss (see crabnado.cpp).
namespace crabnado {

// Registers the BossUpdate system. Call from the module constructor after RegisterCrabnado.
void RegisterBoss(const flecs::world& world);

} // namespace crabnado
