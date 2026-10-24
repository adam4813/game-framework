#pragma once

#include <functional>
#include <string>

#include <flecs.h>

#include "engine/engine.hpp"

namespace crabnado {

// Identity tag for the arena scene (used to find and switch to it).
struct ArenaSceneTag {};

// The gameplay scene: the arena a run plays out in (see the spec). Load() starts a run: the player's
// hermit crab, the crabnado and a camera that follows the player, plus a HUD (HP, clock, kills) and the
// horde performance check's debug control (ticket 01). Everything lives under one root entity that
// Unload() destroys. Losing the run pauses the scene.
class ArenaScene : public engine::scene::SceneBase {
public:
	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void Unload(flecs::world& world) override;
	void Tick(const flecs::world& world, float deltaTime) override;

	// Called when the Title button is clicked.
	std::function<void()> onTitlePressed;

private:
	void BuildStage(const flecs::world& world);
	void BuildUI(const flecs::world& world);
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
	flecs::entity sweptAway_;
};

} // namespace crabnado
