#pragma once

#include <functional>

#include <flecs.h>

#include "engine/engine.hpp"

namespace game {

// Identity tag for the title scene (used to identify and switch to this scene)
struct TitleSceneTag {};

// Title screen scene. Its UI is built from retained engine::ui components in Load() and torn down
// in Unload().
class TitleScene : public engine::scene::SceneBase {
public:
	explicit TitleScene() = default;
	~TitleScene() override = default;

	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void Unload(flecs::world& world) override;

	// Callback when Play button is clicked.
	std::function<void()> onPlayPressed;

private:
	flecs::entity uiRoot_;
};

} // namespace game
