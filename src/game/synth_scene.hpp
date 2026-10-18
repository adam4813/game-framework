#pragma once

#include <functional>
#include <memory>

#include <flecs.h>

#include "engine/engine.hpp"
#include "synth_demo_voice.hpp"

namespace game {

struct SynthSceneTag {};

/// A standalone demo of the procedural audio stream (Platform::StartSynth). Sliders steer a
/// whistle and a hiss; the Click button or Space fires a relay click.
class SynthScene : public engine::scene::SceneBase {
public:
	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void Tick(const flecs::world& world, float deltaTime) override;
	void Unload(flecs::world& world) override;

	/// Called when the Back button is pressed.
	std::function<void()> onBackPressed;

private:
	void BuildUI(const flecs::world& world);

	flecs::entity uiRoot_;
	// Heap-allocated so its address is stable while the audio thread renders it.
	std::unique_ptr<SynthDemoVoice> voice_;
};

} // namespace game
