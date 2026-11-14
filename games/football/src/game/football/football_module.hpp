#pragma once

#include <flecs.h>

namespace game::football {

// Football module: registers and reflects the football game's ECS components so they are visible in
// the Flecs Explorer and available to scripting. Gameplay itself is orchestrated by FootballScene
// (via the engine::flow state machine) and the pure helpers in football_logic, so this module holds
// no per-frame systems — it is the component-registration seam for the football content.
class FootballModule {
public:
	explicit FootballModule(const flecs::world& world);
};

} // namespace game::football
