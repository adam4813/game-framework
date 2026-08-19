#pragma once

#include <flecs.h>

namespace engine::timer {

// Flecs module: registers countdown Timer, Cooldown, and Tween components plus their
// per-frame advance systems. All three systems run in OnUpdate, carry Pausable (so they halt
// while the game is paused), and are untagged (run in every scene pipeline).
class TimerModule {
public:
	explicit TimerModule(const flecs::world& world);
};

} // namespace engine::timer
