#pragma once

#include <functional>

#include <flecs.h>

#include "engine/engine.hpp"

namespace crabnado {

// Identity tag for the title scene (used to find and switch to it).
struct TitleSceneTag {};

// Title screen: a backdrop of the crab's island, the ocean and a swaying crabnado, the game's name, Play and Credits buttons, a smaller Settings button under them, and the Credits and Settings modals. Its UI is built in Load() and destroyed in Unload().
class TitleScene : public engine::scene::SceneBase {
public:
	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void Unload(flecs::world& world) override;

	// Called when the Play button is clicked.
	std::function<void()> onPlayPressed;

private:
	void BuildCreditsModal(const flecs::world& world, float width, float height);

	flecs::entity backdrop_;
	flecs::entity uiRoot_;
	flecs::entity creditsModal_;
	flecs::entity settingsModal_;
};

} // namespace crabnado
