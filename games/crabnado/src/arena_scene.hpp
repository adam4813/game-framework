#pragma once

#include <functional>

#include <flecs.h>

#include "engine/engine.hpp"

namespace crabnado {

// Identity tag for the arena scene (used to find and switch to it).
struct ArenaSceneTag {};

// The gameplay scene: the tide-pool arena a run plays out in (see the spec). For now it shows a
// placeholder heading and a button back to the title. Its UI is built in Load() and destroyed in Unload().
class ArenaScene : public engine::scene::SceneBase {
public:
	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void Unload(flecs::world& world) override;

	// Called when the Title button is clicked.
	std::function<void()> onTitlePressed;

private:
	flecs::entity uiRoot_;
};

} // namespace crabnado
