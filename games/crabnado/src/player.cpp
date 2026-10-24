#include "player.hpp"

#include <cmath>
#include <numbers>

#include "controls.hpp"
#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "run.hpp"
#include "wind.hpp"

using namespace engine;

namespace crabnado {

namespace {

// Hermit crab look: a small body, a big shell on its back and two claws ahead (local +z is forward).
constexpr core::Rgba kBodyColor{.r = 240, .g = 150, .b = 110, .a = 255};
constexpr core::Rgba kShellColor{.r = 245, .g = 235, .b = 215, .a = 255};

void AddPart(const flecs::world& world, const flecs::entity crab, const glm::vec3 position, const glm::vec3 scale,
	const float radius, const core::Rgba color) {
	std::ignore = world.entity()
					  .child_of(crab)
					  .set<spatial::Transform>({.position = position, .scale = scale})
					  .set<render::SpherePrimitive>({.radius = radius})
					  .set<render::Material>({.color = color});
}

} // namespace

void RegisterPlayer(const flecs::world& world) {
	world.component<Player>();
	world.component<PlayerStats>()
		.member<float>("speed")
		.member<float>("hp")
		.member<float>("max_hp")
		.member<float>("pickup_range");
	world.component<FollowCamera>().member<glm::vec3>("offset");

	world.system<const PlayerIntent, const PlayerStats, spatial::Transform>("PlayerMove")
		.term_at(0)
		.singleton()
		.with<Player>()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, const PlayerIntent& intent, const PlayerStats& stats,
				  spatial::Transform& transform) {
			// The wind pushes the crab whether it walks or not, so walking upwind is slower.
			glm::vec2 velocity = WindPush(it.world().get<Wind>(), constants::kPlayerWindPush);
			const float length = glm::length(intent.move);
			if (length > 0.0F) {
				const glm::vec2 direction = intent.move / length;
				velocity += direction * stats.speed;
				// Scuttle sideways: the crab faces 90° off its direction of travel.
				transform.rotation.y = std::atan2(direction.x, direction.y) + std::numbers::pi_v<float> / 2.0F;
			}
			glm::vec2 position = glm::vec2{transform.position.x, transform.position.z} + velocity * it.delta_time();
			const float distance = glm::length(position);
			if (distance > constants::kArenaRadius) {
				position *= constants::kArenaRadius / distance;
			}
			transform.position.x = position.x;
			transform.position.z = position.y;
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	world.system<PlayerStats>("PlayerDefeat")
		.with<Player>()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, PlayerStats& stats) {
			if (stats.hp <= 0.0F) {
				stats.hp = 0.0F;
				LoseRun(it.world());
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	world.system<const FollowCamera, spatial::Transform>("CameraFollow")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, const FollowCamera& follow, spatial::Transform& transform) {
			const flecs::entity player = FindPlayer(it.world());
			if (player) {
				transform.position = player.get<spatial::Transform>().position + follow.offset;
			}
		})
		.add<scene::GameScene>();
}

flecs::entity SpawnPlayer(const flecs::world& world, const flecs::entity parent) {
	const flecs::entity crab = world.entity("Player")
								   .child_of(parent)
								   .add<Player>()
								   .set<PlayerStats>(
									   {.speed = constants::kPlayerSpeed,
										.hp = constants::kPlayerMaxHp,
										.max_hp = constants::kPlayerMaxHp,
										.pickup_range = constants::kPlayerPickupRange}
								   )
								   .set<spatial::Transform>({});
	AddPart(world, crab, {0.0F, 0.4F, 0.2F}, {1.0F, 0.5F, 0.8F}, 0.8F, kBodyColor);
	AddPart(world, crab, {0.0F, 0.9F, -0.4F}, {1.0F, 0.9F, 1.1F}, 0.9F, kShellColor);
	AddPart(world, crab, {-0.7F, 0.45F, 0.8F}, glm::vec3{1.0F}, 0.3F, kBodyColor);
	AddPart(world, crab, {0.7F, 0.45F, 0.8F}, glm::vec3{1.0F}, 0.3F, kBodyColor);
	return crab;
}

flecs::entity FindPlayer(const flecs::world& world) {
	return world.query_builder<const Player>().build().first();
}

} // namespace crabnado
