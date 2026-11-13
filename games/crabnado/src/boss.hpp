#pragma once

#include <flecs.h>

// The boss: at 5:00 the crabnado comes ashore and chases the player, and a big crest that pushes it into a rock is
// a slam. Three slams win the run. It is the same Crabnado entity; its roaming and normal spawn rate give way to
// this behaviour while RunState::phase is Boss (see crabnado.cpp).
namespace crabnado {

// Registers the BossUpdate system. Call from the module constructor after RegisterCrabnado.
void RegisterBoss(const flecs::world& world);

} // namespace crabnado
