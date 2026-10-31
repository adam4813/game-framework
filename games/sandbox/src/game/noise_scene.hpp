#pragma once

#include <functional>
#include <string>

#include <flecs.h>

#include "engine/engine.hpp"

namespace game {

struct NoiseSceneTag {};

/// A standalone 2D demo scene that visualises the engine's noise module.
/// Displays a live 256×256 greyscale texture driven by FractalNoise2D.
/// Controls (sliders + algorithm buttons) let you tune all parameters in real time.
class NoiseScene : public engine::scene::SceneBase {
public:
	explicit NoiseScene() = default;
	~NoiseScene() override = default;

	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void Tick(const flecs::world& world, float deltaTime) override;
	void Unload(flecs::world& world) override;

	/// Called when the Back button is pressed.
	std::function<void()> onBackPressed;

private:
	void BuildUI(const flecs::world& world);
	void RebuildTexture(const flecs::world& world);

	flecs::entity uiRoot_;
	flecs::entity imagePanel_;
	flecs::entity seedSlider_;
	flecs::entity seedValueLabel_;
	int noiseTexture_{-1};
	bool dirty_{true};

	float scale_{0.025F};
	int seed_{0};
	std::string algorithm_{"perlin"};
	engine::noise::FractalNoiseParams params_{.octaves = 4, .persistence = 0.5F, .lacunarity = 2.0F};
};

} // namespace game
