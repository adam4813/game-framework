#include "title_backdrop.hpp"

#include <cmath>
#include <numbers>

#include "crabnado.hpp"
#include "engine/engine.hpp"
#include "player.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

constexpr core::Rgba kDeepWater{.r = 28, .g = 110, .b = 160, .a = 255};
constexpr core::Rgba kShallowWater{.r = 70, .g = 190, .b = 190, .a = 255};
constexpr core::Rgba kSand{.r = 238, .g = 214, .b = 150, .a = 255};
constexpr core::Rgba kTrunk{.r = 130, .g = 92, .b = 58, .a = 255};
constexpr core::Rgba kFrond{.r = 52, .g = 150, .b = 70, .a = 255};
constexpr core::Rgba kSky{.r = 135, .g = 200, .b = 235, .a = 255};
constexpr core::Rgba kBreezeColor{.r = 250, .g = 252, .b = 255, .a = 255};

// The ocean: tiles of kWaterCell over a square of half-size kOceanReach around the island.
constexpr float kWaterCell = 5.0F;
constexpr float kOceanReach = 110.0F;
constexpr float kTileThickness = 1.0F;
constexpr float kSwellHeight = 0.35F;
constexpr float kSwellRate = 1.3F;     // radians per second
constexpr float kSwellSpacing = 0.22F; // radians of phase per unit of distance

constexpr float kIslandRadius = 13.0F;
constexpr float kIslandTop = 2.6F;

constexpr glm::vec3 kCrabPosition{0.0F, kIslandTop - 0.2F, -1.5F};
constexpr float kCrabScale = 2.2F;

// The crabnado wanders left and right behind the island.
constexpr glm::vec3 kCrabnadoOrigin{0.0F, 0.0F, -55.0F};
constexpr glm::vec3 kCrabnadoSweep{40.0F, 0.0F, 10.0F};
constexpr float kCrabnadoRate = 0.35F;
constexpr float kCrabnadoSpin = 2.0F;

constexpr int kBreezeCount = 60;
constexpr float kBreezeLength = 4.0F;
constexpr float kBreezeThickness = 0.1F;

glm::vec3 Scaled(const float x, const float y, const float z) {
	return {x, y, z};
}

flecs::entity AddBlob(const flecs::world& world, const flecs::entity parent, const glm::vec3 position,
	const glm::vec3 scale, const glm::vec3 rotation, const float radius, const core::Rgba color) {
	return world.entity()
		.child_of(parent)
		.set<spatial::Transform>({.position = position, .rotation = rotation, .scale = scale})
		.set<render::SpherePrimitive>({.radius = radius})
		.set<render::Material>({.color = color, .cast_shadow = false});
}

void BuildOcean(const flecs::world& world, const flecs::entity parent) {
	const flecs::entity ocean = world.entity("Ocean").child_of(parent);
	const int half = static_cast<int>(kOceanReach / kWaterCell);
	const float size = kWaterCell * 0.97F;
	for (int i = -half; i <= half; ++i) {
		for (int j = -half; j <= half; ++j) {
			const glm::vec2 center{static_cast<float>(i) * kWaterCell, static_cast<float>(j) * kWaterCell};
			const float distance = glm::length(center);
			// Shallows fade into deep water away from the island.
			const float deep = std::clamp((distance - 10.0F) / 30.0F, 0.0F, 1.0F);
			const core::Rgba color{
				.r = static_cast<uint8_t>(kShallowWater.r + (kDeepWater.r - kShallowWater.r) * deep),
				.g = static_cast<uint8_t>(kShallowWater.g + (kDeepWater.g - kShallowWater.g) * deep),
				.b = static_cast<uint8_t>(kShallowWater.b + (kDeepWater.b - kShallowWater.b) * deep),
				.a = 255,
			};
			std::ignore = world.entity()
							  .child_of(ocean)
							  .set<TitleBob>({.phase = distance * kSwellSpacing + center.x * 0.05F, .rest = -kTileThickness / 2.0F})
							  .set<spatial::Transform>({.position = {center.x, -kTileThickness / 2.0F, center.y}})
							  .set<render::CubePrimitive>({.size = {size, kTileThickness, size}})
							  .set<render::Material>({.color = color, .cast_shadow = false});
		}
	}
}

void BuildIsland(const flecs::world& world, const flecs::entity parent) {
	const flecs::entity island = world.entity("Island").child_of(parent);
	// A sand dome the swell rises and falls through.
	AddBlob(world, island, {0.0F, 0.0F, 0.0F}, Scaled(1.0F, kIslandTop / kIslandRadius, 1.0F), glm::vec3{0.0F}, kIslandRadius, kSand);

	// A leaning palm: a trunk of stacked spheres, then a crown of fronds.
	constexpr glm::vec3 base{6.0F, kIslandTop - 0.8F, -3.0F};
	constexpr glm::vec3 lean{0.45F, 1.0F, 0.1F};
	constexpr int segments = 14;
	for (int i = 0; i < segments; ++i) {
		const float t = static_cast<float>(i);
		AddBlob(world, island, base + lean * t * 0.45F, Scaled(1.0F, 1.0F, 1.0F), glm::vec3{0.0F}, 0.34F - 0.012F * t, kTrunk);
	}
	const glm::vec3 crown = base + lean * static_cast<float>(segments - 1) * 0.45F + glm::vec3{0.0F, 0.3F, 0.0F};
	AddBlob(world, island, crown, Scaled(1.0F, 1.0F, 1.0F), glm::vec3{0.0F}, 0.4F, kFrond);
	constexpr int fronds = 6;
	for (int i = 0; i < fronds; ++i) {
		const float yaw = 2.0F * kPi * static_cast<float>(i) / fronds;
		const glm::vec3 outward{std::sin(yaw), 0.0F, std::cos(yaw)};
		AddBlob(world, island, crown + outward * 1.5F - glm::vec3{0.0F, 0.25F, 0.0F}, Scaled(1.0F, 0.12F, 0.28F),
			{0.0F, yaw + kPi / 2.0F, 0.0F}, 1.6F, kFrond);
	}
}

void BuildCrab(const flecs::world& world, const flecs::entity parent) {
	const flecs::entity crab = world.entity("TitleCrab")
								   .child_of(parent)
								   .set<TitleSway>({.origin = kCrabPosition, .amplitude = {0.15F, 0.0F, 0.0F}, .rate = 1.6F})
								   .set<spatial::Transform>({.position = kCrabPosition, .scale = glm::vec3{kCrabScale}});
	AddCrabBody(world, crab);
}

void BuildBreeze(const flecs::world& world, const flecs::entity parent) {
	const flecs::entity breeze = world.entity("Breeze").child_of(parent);
	auto& rng = world.get_mut<ecs::RngState>();
	for (int i = 0; i < kBreezeCount; ++i) {
		std::ignore = world.entity()
						  .child_of(breeze)
						  .set<TitleBreeze>({.speed = rng.NextFloatRange(9.0F, 16.0F)})
						  .set<spatial::Transform>(
							  {.position = {rng.NextFloatRange(-kOceanReach, kOceanReach), rng.NextFloatRange(1.0F, 14.0F),
											rng.NextFloatRange(-kOceanReach * 0.6F, 20.0F)},
							   .rotation = {0.0F, kPi / 2.0F, 0.0F},
							   .scale = {1.0F, 1.0F, rng.NextFloatRange(0.6F, 1.4F)}}
						  )
						  .set<render::CubePrimitive>({.size = {kBreezeThickness, kBreezeThickness, kBreezeLength}})
						  .set<render::Material>({.color = kBreezeColor, .cast_shadow = false, .unlit = true});
	}
}

} // namespace

void RegisterTitleBackdrop(const flecs::world& world) {
	world.component<TitleSway>()
		.member<glm::vec3>("origin")
		.member<glm::vec3>("amplitude")
		.member<float>("rate")
		.member<float>("spin")
		.member<float>("time");
	world.component<TitleBob>().member<float>("phase").member<float>("rest");
	world.component<TitleBreeze>().member<float>("speed");

	world.system<TitleSway, spatial::Transform>("TitleSway")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, TitleSway& sway, spatial::Transform& transform) {
			sway.time += it.delta_time();
			const float angle = sway.time * sway.rate;
			transform.position = sway.origin + sway.amplitude * glm::vec3{std::sin(angle), 0.0F, std::cos(angle)};
			transform.rotation.y += sway.spin * it.delta_time();
		})
		.add<scene::GameScene>();

	world.system<TitleBob, spatial::Transform>("TitleBob")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, TitleBob& bob, spatial::Transform& transform) {
			bob.phase += kSwellRate * it.delta_time();
			transform.position.y = bob.rest + kSwellHeight * std::sin(bob.phase);
		})
		.add<scene::GameScene>();

	world.system<const TitleBreeze, spatial::Transform>("TitleBreezeMove")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, const TitleBreeze& breeze, spatial::Transform& transform) {
			transform.position.x += breeze.speed * it.delta_time();
			if (transform.position.x > kOceanReach) {
				transform.position.x -= 2.0F * kOceanReach;
			}
		})
		.add<scene::GameScene>();
}

void BuildTitleBackdrop(const flecs::world& world, const flecs::entity parent) {
	world.set<render::AmbientLight>({.color = {.r = 160, .g = 170, .b = 190, .a = 255}, .intensity = 0.55F});
	std::ignore = world.entity("Sun").child_of(parent).set<render::DirectionalLight>({});

	const flecs::entity focus = world.entity("Focus").child_of(parent).set<spatial::Transform>({.position = {0.0F, 4.0F, 0.0F}});
	std::ignore = world.entity("Camera")
					  .child_of(parent)
					  .set<spatial::Transform>({.position = {0.0F, 10.0F, 24.0F}})
					  .set<render::Camera>({.fov = 55.0F, .far_plane = 250.0F})
					  .add<render::LookAt>(focus);

	// The sky: a huge unlit wall behind the ocean's far edge.
	std::ignore = world.entity("Sky")
					  .child_of(parent)
					  .set<spatial::Transform>({.position = {0.0F, 60.0F, -170.0F}})
					  .set<render::CubePrimitive>({.size = {900.0F, 260.0F, 1.0F}})
					  .set<render::Material>({.color = kSky, .cast_shadow = false, .unlit = true});

	BuildOcean(world, parent);
	BuildIsland(world, parent);
	BuildCrab(world, parent);
	BuildBreeze(world, parent);

	const flecs::entity crabnado = world.entity("TitleCrabnado")
									  .child_of(parent)
									  .set<TitleSway>({.origin = kCrabnadoOrigin,
													   .amplitude = kCrabnadoSweep,
													   .rate = kCrabnadoRate,
													   .spin = kCrabnadoSpin})
									  .set<spatial::Transform>({.position = kCrabnadoOrigin});
	AddFunnel(world, crabnado);
}

} // namespace crabnado
