#pragma once

#include <flecs.h>

#include "engine/engine.hpp"

namespace game {

// Identity tag for the many-lights demo scene.
struct LightsSceneTag {};

// Singleton: the demo's settings and its measured frame time, reflected so the Remote API can set one and read the
// other. The scene respawns its lights when a count changes.
struct LightsDemo {
	int lights{256};       // point lights over the floor, all in view
	int outside_lights{0}; // point lights behind the camera, all culled
	bool uncapped{false};  // run Frames as fast as the machine allows, to measure
	float frame_ms{0.0F};  // average over the last window
};

// Demo of clustered point lights: any number of small lights over a floor of primitives, instanced and skinned models
// and translucent shapes, with keys to change the count and a readout of what the frame costs. Over the visible budget
// (1024) only the most important lights are kept. The set is authored in assets/levels/lights_demo.level.json; the
// lights are spawned here because their number changes. Escape returns to the title.
class LightsScene : public engine::scene::SceneBase {
public:
	// Registers LightsDemo. Call once, before the scene is activated.
	static void RegisterComponents(const flecs::world& world);

	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void HandleInput(const flecs::world& world, const engine::input::InputState& input) override;
	void Tick(const flecs::world& world, float deltaTime) override;
	void DrawUI(const flecs::world& world, const engine::input::InputState& input) override;
	void Unload(flecs::world& world) override;

private:
	void SpawnLights(const flecs::world& world, const LightsDemo& demo);

	flecs::entity sceneRoot_;
	flecs::entity lightsRoot_;
	// The settings in effect, to notice a change from a key or the Remote API.
	LightsDemo applied_;
	// The frame-time window being averaged.
	float windowSeconds_{0.0F};
	int windowFrames_{0};
};

} // namespace game
