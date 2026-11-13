#pragma once

#include <flecs.h>

#include "engine/ecs/ecs.hpp"
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

	// Register any scene-specific loaders (e.g., level loaders, component loaders) with the world.
	virtual void RegisterLoaders([[maybe_unused]] const flecs::world& world) {}

	// Initializes and creates the scene's pipelines. Called when the scene component is added.
	//
	// A scene builds its Frame pipeline from ecs::FramePipeline, which already leaves out the FixedStep and FrameSample
	// systems the fixed step runs, and its fixed-step pipeline from ecs::FixedStepPipeline, both with the same mode
	// filter. A scene with Pausable systems also builds a paused Frame pipeline. Scene-specific systems should be a
	// child of the scene root entity. A gameplay scene excludes the menu tag:
	//
	//   pipeline_ = ecs::FramePipeline(world).without<MenuScene>().build();
	//   pausedPipeline_ = ecs::FramePipeline(world).without<MenuScene>().without<ecs::Pausable>().build();
	//   fixedStepPipeline_ = ecs::FixedStepPipeline(world).without<MenuScene>().build();
	virtual void InitializePipeline(const flecs::world& world) {
		// Default: every system, in every mode.
		pipeline_ = ecs::FramePipeline(world).build();
		fixedStepPipeline_ = ecs::FixedStepPipeline(world).build();
	}

	// Called once when the scene becomes active.
	virtual void Load([[maybe_unused]] flecs::world& world) {}

	// Called once when the scene is being deactivated.
	virtual void Unload([[maybe_unused]] flecs::world& world) {}

	// Called every frame to update scene logic.
	// deltaTime: elapsed time since last frame in seconds.
	virtual void Tick([[maybe_unused]] const flecs::world& world, [[maybe_unused]] float deltaTime) {}

	// Called every frame to handle input events.
	virtual void
	HandleInput([[maybe_unused]] const flecs::world& world, [[maybe_unused]] const input::InputState& input) {}

	// Called every frame to render debug UI.
	virtual void DrawDebugUI() const {}

	// Called every frame to draw 2D scene UI (overlays, buttons, HUD). Runs in the engine
	// OnUI phase (after all OnStore systems) so 2D elements always composite on top of 3D
	// geometry regardless of module registration order.
	virtual void DrawUI([[maybe_unused]] const flecs::world& world, [[maybe_unused]] const input::InputState& input) {}

	// Gets the pipeline entity for this scene (set during InitializePipeline).
	[[nodiscard]] flecs::entity GetPipeline() const { return pipeline_; }

	// Gets the pipeline to run while the game is paused. Defaults to the running pipeline
	// (i.e. no pausing). Scenes with pausable systems override InitializePipeline to build a
	// distinct pipeline that excludes engine::ecs::Pausable systems and assign it to
	// pausedPipeline_.
	[[nodiscard]] flecs::entity GetPausedPipeline() const { return pausedPipeline_ ? pausedPipeline_ : pipeline_; }

	// Gets the pipeline the fixed step runs once per Tick while this scene is active. Null when the scene built none,
	// and the fixed step then runs every FixedStep system.
	[[nodiscard]] flecs::entity GetFixedStepPipeline() const { return fixedStepPipeline_; }

protected:
	// Set by scene during InitializePipeline
	flecs::entity pipeline_;

	// Optional pipeline used while paused; falls back to pipeline_ when unset.
	flecs::entity pausedPipeline_;

	// Optional pipeline of the FixedStep systems this scene runs on each Tick.
	flecs::entity fixedStepPipeline_;
};

} // namespace engine::scene
