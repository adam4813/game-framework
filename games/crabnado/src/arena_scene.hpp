#pragma once

#include <functional>

#include <flecs.h>

#include "engine/engine.hpp"

namespace crabnado {

// Identity tag for the arena scene (used to find and switch to it).
struct ArenaSceneTag {};

// The gameplay scene: the tide-pool arena a run plays out in (see the spec). For now it holds the
// horde performance check (ticket 01): a sand floor under a fixed angled camera, a debug control that
// spawns crabs in batches, and an FPS / enemy-count readout. Load() builds everything under one root
// entity and Unload() destroys it.
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
	// The debug control: spawns a batch of crabs (up to kHordeDebugMax) or clears them all.
	void SpawnBatch(const flecs::world& world, int enemyCount) const;
	void ClearHorde(const flecs::world& world);

	flecs::entity sceneRoot_;
	flecs::entity horde_; // parent of every enemy, so clearing the horde is one destruct
	flecs::entity uiRoot_;
	flecs::entity readout_;
};

} // namespace crabnado
