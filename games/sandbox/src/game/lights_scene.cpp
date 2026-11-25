#include "lights_scene.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "engine/engine.hpp"
#include "title_scene.hpp"

using namespace engine;

namespace game {

namespace {

// The lights spread over a disc on the floor, each reaching only a small part of the view.
constexpr float kDiscRadius = 22.0F;
constexpr float kLightHeight = 0.7F;
constexpr float kLightRange = 2.2F;
constexpr float kLightIntensity = 1.2F;
// Past this many lights each is dimmed in proportion, so the overlapping pools don't wash out.
constexpr float kDimAboveCount = 256.0F;
constexpr float kMarkerRadius = 0.07F;

// The outside lights' disc, centered behind the camera.
constexpr glm::vec3 kOutsideCenter{0.0F, 0.0F, -80.0F};

constexpr float kWindowSeconds = 1.0F;

// The light count each number key picks.
constexpr std::array<std::pair<int, int>, 5> kCountKeys{{
	{input::KeyCode::Key1, 4},
	{input::KeyCode::Key2, 64},
	{input::KeyCode::Key3, 256},
	{input::KeyCode::Key4, 512},
	{input::KeyCode::Key5, 1024},
}};
constexpr int kOutsideCount = 2000;

// Where light `i` of `count` goes on a disc: a sunflower spiral, which spreads any number evenly.
glm::vec3 SpiralPosition(const int i, const int count) {
	const float t = (static_cast<float>(i) + 0.5F) / static_cast<float>(count);
	const float radius = kDiscRadius * std::sqrt(t);
	const float angle = static_cast<float>(i) * glm::pi<float>() * (3.0F - std::sqrt(5.0F));
	return {radius * std::cos(angle), kLightHeight, radius * std::sin(angle)};
}

// A saturated color around the hue wheel, `hue` in [0, 1).
core::Rgba HueColor(const float hue) {
	const auto channel = [hue](const float offset) {
		const float wave = std::abs(std::fmod(hue + offset, 1.0F) * 6.0F - 3.0F) - 1.0F;
		return static_cast<uint8_t>(60.0F + 195.0F * std::clamp(wave, 0.0F, 1.0F));
	};
	return {.r = channel(0.0F), .g = channel(2.0F / 3.0F), .b = channel(1.0F / 3.0F), .a = 255};
}

} // namespace

void LightsScene::RegisterComponents(const flecs::world& world) {
	world.component<LightsDemo>()
		.member<int>("lights")
		.member<int>("outside_lights")
		.member<bool>("uncapped")
		.member<float>("frame_ms");
	world.set<LightsDemo>({});
}

void LightsScene::InitializePipeline(const flecs::world& world) {
	pipeline_ = ecs::FramePipeline(world).without<scene::MenuScene>().build();
	fixedStepPipeline_ = ecs::FixedStepPipeline(world).without<scene::MenuScene>().build();
}

void LightsScene::Load(flecs::world& world) {
	sceneRoot_ = world.entity("LightsSceneRoot");
	level::LoadLevel(world, assets::ResolveAsset(world, "levels/lights_demo.level.json"), sceneRoot_);
	world.set<LightsDemo>({});
	applied_ = {};
	windowSeconds_ = 0.0F;
	windowFrames_ = 0;
	SpawnLights(world, applied_);
}

void LightsScene::SpawnLights(const flecs::world& world, const LightsDemo& demo) {
	if (lightsRoot_) lightsRoot_.destruct();
	lightsRoot_ = world.entity().child_of(sceneRoot_);
	const auto spawn = [&](const int count, const glm::vec3 center) {
		const float intensity =
			kLightIntensity * std::min(1.0F, kDimAboveCount / static_cast<float>(std::max(count, 1)));
		for (int i = 0; i < count; ++i) {
			const core::Rgba color = HueColor(std::fmod(static_cast<float>(i) * 0.754878F, 1.0F));
			std::ignore = world.entity()
							  .child_of(lightsRoot_)
							  .set<spatial::Transform>({.position = center + SpiralPosition(i, count)})
							  .set<render::PointLight>({.color = color, .intensity = intensity, .range = kLightRange})
							  .set<render::SpherePrimitive>({.radius = kMarkerRadius})
							  .set<render::Material>({.color = color, .cast_shadow = false, .unlit = true});
		}
	};
	spawn(demo.lights, glm::vec3{0.0F});
	spawn(demo.outside_lights, kOutsideCenter);
}

void LightsScene::HandleInput(const flecs::world& world, const input::InputState& input) {
	if (input.keys[input::KeyCode::Escape].pressed) {
		scene::ActivateScene<TitleSceneTag>(world);
		return;
	}
	auto& demo = world.get_mut<LightsDemo>();
	for (const auto& [key, count] : kCountKeys) {
		if (input.keys[key].pressed) demo.lights = count;
	}
	if (input.keys[input::KeyCode::O].pressed) demo.outside_lights = demo.outside_lights > 0 ? 0 : kOutsideCount;
	if (input.keys[input::KeyCode::U].pressed) demo.uncapped = !demo.uncapped;
}

void LightsScene::Tick(const flecs::world& world, const float deltaTime) {
	auto& demo = world.get_mut<LightsDemo>();
	if (demo.lights != applied_.lights || demo.outside_lights != applied_.outside_lights) SpawnLights(world, demo);
	if (demo.uncapped != applied_.uncapped) {
		const auto mode = demo.uncapped ? platform::FrameRateMode::Uncapped : platform::FrameRateMode::Capped;
		world.get<platform::PlatformRef>().ptr->SetFrameRate({.mode = mode});
	}
	applied_ = demo;

	windowSeconds_ += deltaTime;
	++windowFrames_;
	if (windowSeconds_ >= kWindowSeconds) {
		demo.frame_ms = windowSeconds_ * 1000.0F / static_cast<float>(windowFrames_);
		windowSeconds_ = 0.0F;
		windowFrames_ = 0;
	}
}

void LightsScene::DrawUI(const flecs::world& world, const input::InputState&) {
	auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto& demo = world.get<LightsDemo>();
	const auto& stats = world.get<render::RenderStats>();
	const std::string counts = std::format(
		"Lights {}   visible {}   dropped {}   cluster entries dropped {}",
		stats.lights,
		stats.lights_visible,
		stats.lights_dropped,
		stats.lights_cluster_dropped
	);
	const std::string timing =
		std::format("Frame {:.2f} ms   cluster build {:.2f} ms", demo.frame_ms, stats.light_cluster_ms);
	platform->DrawText(counts, 16.0F, 14.0F, 20.0F, core::colors::Title);
	platform->DrawText(timing, 16.0F, 40.0F, 20.0F, core::colors::Title);
	platform->DrawText(
		"1-5: 4, 64, 256, 512, 1024 lights   O: 2000 lights out of view   U: uncap the frame rate   Esc: back",
		16.0F,
		66.0F,
		16.0F,
		core::colors::Subtle
	);
}

void LightsScene::Unload(flecs::world& world) {
	if (applied_.uncapped) world.get<platform::PlatformRef>().ptr->SetFrameRate({});
	if (sceneRoot_) {
		sceneRoot_.destruct();
		sceneRoot_ = flecs::entity{};
	}
	lightsRoot_ = flecs::entity{};
	if (world.has<render::AmbientLight>()) {
		world.remove<render::AmbientLight>();
	}
}

} // namespace game
