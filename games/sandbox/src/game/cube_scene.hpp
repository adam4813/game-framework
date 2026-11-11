#pragma once

#include <functional>

#include <flecs.h>

#include "engine/engine.hpp"

namespace game {

// Identity tag for the cube/3D demo scene (used to identify and switch to this scene)
struct CubeSceneTag {};

// Main gameplay scene with 3D physics demo.
class CubeScene : public engine::scene::SceneBase {
public:
	explicit CubeScene() = default;
	~CubeScene() override = default;

	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void Tick(const flecs::world& world, float deltaTime) override;
	void HandleInput(const flecs::world& world, const engine::input::InputState& input) override;
	void Unload(flecs::world& world) override;

	// Callback when quit button is pressed.
	std::function<void()> onQuitPressed;

private:
	void SetupPhysicsDemo(const flecs::world& world) const;
	void SetupRuntimeAlbedo(const flecs::world& world);
	void TickRuntimeAlbedo(const flecs::world& world, float deltaTime);
	void BuildUI(const flecs::world& world);

	flecs::entity sceneRoot_;
	flecs::entity uiRoot_;
	flecs::entity hud_;
	flecs::entity pauseModal_;

	// Runtime albedo demo: a texture this scene uploads and owns, shown on the level's RuntimeAlbedoQuad and
	// refilled once a second.
	int runtimeTexture_{-1};
	int runtimeTextureStep_{0};
	float runtimeTextureTimer_{0.0F};
};

} // namespace game
