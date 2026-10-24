#include "horde.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/glm.hpp>

#include "crabnado_constants.hpp"
#include "engine/engine.hpp"

using namespace engine;

namespace crabnado {

namespace {

// Crab look, from the prototype: a flattened body sphere with two claw spheres ahead of it (local +z
// is forward).
constexpr core::Rgba kCrabColor{.r = 216, .g = 69, .b = 43, .a = 255};
constexpr glm::vec3 kBodyOffset{0.0F, 0.45F, 0.0F};
constexpr glm::vec3 kBodyScale{1.0F, 0.5F, 0.75F};
constexpr float kClawRadius = 0.32F;
constexpr glm::vec3 kClawOffset{0.85F, 0.5F, 0.7F};

void AddPart(const flecs::world& world, const flecs::entity crab, const glm::vec3 position, const glm::vec3 scale,
	const float radius) {
	std::ignore = world.entity()
					  .child_of(crab)
					  .set<spatial::Transform>({.position = position, .scale = scale})
					  .set<render::SpherePrimitive>({.radius = radius})
					  .set<render::Material>({.color = kCrabColor});
}

} // namespace

void RegisterHorde(const flecs::world& world) {
	world.component<Enemy>().member<float>("speed");
	world.component<HordeReadout>().member<int>("enemy_count").member<int>("fps");
	world.set<HordeReadout>({});

	// Straight-line walk toward the arena centre. Crowding and the wave push replace this in later tickets.
	world.system<const Enemy, spatial::Transform>("HordeWalk")
		.kind(flecs::OnUpdate)
		.multi_threaded()
		.each([](const flecs::iter& it, size_t, const Enemy& enemy, spatial::Transform& transform) {
			const glm::vec2 toCentre{-transform.position.x, -transform.position.z};
			const float distance = glm::length(toCentre);
			if (distance <= constants::kCrabArriveRadius) {
				return;
			}
			const glm::vec2 step = toCentre / distance * std::min(enemy.speed * it.delta_time(), distance);
			transform.position.x += step.x;
			transform.position.z += step.y;
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();
}

void SpawnCrabs(const flecs::world& world, const flecs::entity parent, const int count) {
	auto& rng = world.get_mut<ecs::RngState>();
	for (int i = 0; i < count; ++i) {
		const float angle = rng.NextFloatRange(0.0F, 2.0F * std::numbers::pi_v<float>);
		const float radius = constants::kArenaRadius * rng.NextFloatRange(0.85F, 1.0F);
		const glm::vec3 position{std::cos(angle) * radius, 0.0F, std::sin(angle) * radius};
		// Face the centre: yaw that turns local +z toward -position.
		const float yaw = std::atan2(-position.x, -position.z);

		const flecs::entity crab = world.entity()
									   .child_of(parent)
									   .set<Enemy>({.speed = constants::kCrabSpeed})
									   .set<spatial::Transform>({.position = position, .rotation = {0.0F, yaw, 0.0F}});
		AddPart(world, crab, kBodyOffset, kBodyScale, 1.0F);
		AddPart(world, crab, kClawOffset * glm::vec3{-1.0F, 1.0F, 1.0F}, glm::vec3{1.0F}, kClawRadius);
		AddPart(world, crab, kClawOffset, glm::vec3{1.0F}, kClawRadius);
	}
}

} // namespace crabnado
