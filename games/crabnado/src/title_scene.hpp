#pragma once

#include <functional>

#include <flecs.h>

#include "engine/engine.hpp"

namespace crabnado {

// Identity tag for the title scene (used to find and switch to it).
struct TitleSceneTag {};

// Title screen: the game's name and a Play button. Its UI is built in Load() and destroyed in Unload().
class TitleScene : public engine::scene::SceneBase {
public:
	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void Unload(flecs::world& world) override;

	// Called when the Play button is clicked.
	std::function<void()> onPlayPressed;

private:
	flecs::entity uiRoot_;
};

} // namespace crabnado
