#pragma once

#include <array>
#include <functional>
#include <string>

#include <flecs.h>

#include "engine/engine.hpp"
#include "upgrades.hpp"

namespace crabnado {

// Identity tag for the arena scene (used to find and switch to it).
struct ArenaSceneTag {};

// The gameplay scene: the arena a run plays out in (see the spec). Load() starts a run: the player's
// hermit crab, the crabnado and a camera that follows the player, plus a HUD (HP, clock, kills, the shell
// bar toward the next molt, the stamina bar, the wind compass), wind streaks, the molt modal that offers upgrades, and the horde performance check's debug
// control (ticket 01). Everything lives under one root entity that Unload() destroys. Losing the run
// pauses the scene and opens the results modal; Esc opens the pause menu.
class ArenaScene : public engine::scene::SceneBase {
public:
	void InitializePipeline(const flecs::world& world) override;
	void RegisterLoaders(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void Unload(flecs::world& world) override;
	void Tick(const flecs::world& world, float deltaTime) override;

	// Called when the Title button is clicked.
	std::function<void()> onTitlePressed;

private:
	void BuildStage(const flecs::world& world);
	void BuildUI(const flecs::world& world);
	void BuildMoltModal(const flecs::world& world, float width, float height);
	void BuildResultsModal(const flecs::world& world, float width, float height);
	void BuildPauseModal(const flecs::world& world, float width, float height);
	void UpdateResults(const flecs::world& world) const;
	void UpdateMolt(const flecs::world& world) const;
	void UpdateStamina(flecs::entity player) const;
	void UpdateBoss(const flecs::world& world) const;
	void UpdateTelegraph(const flecs::world& world, flecs::entity player) const;
	// Whether the cue for the current big crest has played, so it plays once per crest.
	mutable bool crestCued_{false};
	void UpdateHud(const flecs::world& world, const std::string& readout) const;
	// The debug control: throws a batch of crabs (up to kHordeDebugMax) or clears them all.
	void SpawnBatch(int enemyCount) const;
	void ClearHorde(const flecs::world& world);

	flecs::entity sceneRoot_;
	flecs::entity horde_; // parent of every enemy, so clearing the horde is one destruct
	flecs::entity crabnado_;
	flecs::entity uiRoot_;
	flecs::entity hud_;
	flecs::entity readout_;
	flecs::entity music_;
	flecs::entity resultsModal_;
	flecs::entity resultsHeading_;
	flecs::entity resultsStats_;
	flecs::entity pauseModal_;
	flecs::entity moltBar_;
	flecs::entity moltLabel_;
	flecs::entity staminaBar_;
	flecs::entity stateLabel_;
	flecs::entity telegraph_;
	flecs::entity bossLabel_;
	flecs::entity bannerLabel_;
	flecs::entity moltModal_;
	std::array<flecs::entity, kMoltChoices> choiceButtons_;
	std::array<flecs::entity, kMoltChoices> choiceDescriptions_;
};

} // namespace crabnado
