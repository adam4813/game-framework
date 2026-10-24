#pragma once

#include <functional>
#include <vector>

#include <flecs.h>

#include "engine/engine.hpp"

namespace crabnado {

// Identity tag for the title scene (used to find and switch to it).
struct TitleSceneTag {};

// Title screen: a backdrop of the crab's island, the ocean and a swaying crabnado, the game's name, Play and Credits buttons, a smaller Settings button under them, and the Credits and Settings modals. Its UI is built in Load() and destroyed in Unload().
// Play starts the intro: the title and buttons slide off, the camera tilts up, and the crab scurries off the island and
// swims toward the crabnado. Space, Enter or Esc skips it.
class TitleScene : public engine::scene::SceneBase {
public:
	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void Unload(flecs::world& world) override;
	void Tick(const flecs::world& world, float deltaTime) override;

	// Called when the intro that Play starts has finished.
	std::function<void()> onIntroFinished;

private:
	// A UI element that slides off screen during the intro: its resting y and the direction it leaves in (-1 up, +1 down).
	struct Slide {
		flecs::entity entity;
		float rest{0.0F};
		float direction{0.0F};
	};

	void BuildCreditsModal(const flecs::world& world, float width, float height);
	void StartIntro();
	[[nodiscard]] bool IntroPlaying() const { return introTime_ >= 0.0F; }

	flecs::entity backdrop_;
	flecs::entity uiRoot_;
	flecs::entity creditsModal_;
	flecs::entity settingsModal_;

	// The intro's state: seconds since Play (negative before it starts), and what it moves.
	float introTime_{-1.0F};
	bool introFinished_{false};
	std::vector<Slide> slides_;
	flecs::entity logo_;
	float logoRest_{0.0F};
};

} // namespace crabnado
