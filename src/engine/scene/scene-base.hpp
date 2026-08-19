#pragma once

#include <flecs.h>

#include "engine/input/input.hpp"

namespace engine::scene {

// Forward declarations
struct GameScene;
struct MenuScene;

// Base interface for a scene. A scene represents a distinct state or screen in the game
// (e.g., title screen, gameplay, pause menu, level select).
class SceneBase {
public:
	virtual ~SceneBase() = default;

	// Initializes and creates the scene's pipeline.
	// Called when the scene component is added.
	// Concrete scenes should override to create a pipeline with their specific systems.
	//
	// Scene-specific systems should be a chld of the scene root entity.:
	//
	// The pipeline then excludes the other scene's tag:
	//
	//   pipeline_ = world.pipeline()
	//       .with(flecs::System)
	//       .without<MenuScene>()
	//       .build();
	virtual void InitializePipeline(const flecs::world& world) {
		// Default: use the world's default pipeline
		pipeline_ = world.pipeline().build();
	}

	// Called once when the scene becomes active.
	virtual void Load(flecs::world& world) {}

	// Called once when the scene is being deactivated.
	virtual void Unload(flecs::world& world) {}

	// Called every frame to update scene logic.
	// deltaTime: elapsed time since last frame in seconds.
	virtual void Tick(const flecs::world& world, float deltaTime) {}

	// Called every frame to handle input events.
	virtual void HandleInput(const flecs::world& world, const input::InputState& input) {}

	// Called every frame to render debug UI.
	virtual void DrawDebugUI() const {}

	// Called every frame to draw 2D scene UI (overlays, buttons, HUD). Runs in the engine
	// OnUI phase (after all OnStore systems) so 2D elements always composite on top of 3D
	// geometry regardless of module registration order.
	virtual void DrawUI(const flecs::world& world, const input::InputState& input) {}

	// Gets the pipeline entity for this scene (set during InitializePipeline).
	[[nodiscard]] flecs::entity GetPipeline() const { return pipeline_; }

	// Gets the pipeline to run while the game is paused. Defaults to the running pipeline
	// (i.e. no pausing). Scenes with pausable systems override InitializePipeline to build a
	// distinct pipeline that excludes engine::ecs::Pausable systems and assign it to
	// pausedPipeline_.
	[[nodiscard]] flecs::entity GetPausedPipeline() const { return pausedPipeline_ ? pausedPipeline_ : pipeline_; }

protected:
	// Set by scene during InitializePipeline
	flecs::entity pipeline_;

	// Optional pipeline used while paused; falls back to pipeline_ when unset.
	flecs::entity pausedPipeline_;
};

} // namespace engine::scene
