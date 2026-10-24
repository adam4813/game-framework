#include "player.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "controls.hpp"
#include "arena.hpp"
#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "run.hpp"
#include "waves.hpp"
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
	world.component<Stamina>()
		.member<float>("value")
		.member<float>("max")
		.member<float>("regen")
		.member<float>("drain")
		.member<bool>("exhausted")
		.member<bool>("dug_in");
	world.component<SurfKnock>().member<glm::vec2>("velocity");
	world.component<FollowCamera>().member<glm::vec3>("offset");

	world.system<const PlayerIntent, Stamina>("PlayerStamina")
		.term_at(0)
		.singleton()
		.with<Player>()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, const PlayerIntent& intent, Stamina& stamina) {
			const float dt = it.delta_time();
			if (stamina.exhausted && stamina.value >= stamina.max * constants::kStaminaRecoverFraction) {
				stamina.exhausted = false;
			}
			stamina.dug_in = intent.dig_in && !stamina.exhausted && stamina.value > 0.0F;
			if (stamina.dug_in) {
				stamina.value -= stamina.drain * dt;
				if (stamina.value <= 0.0F) {
					stamina.value = 0.0F;
					stamina.exhausted = true;
					stamina.dug_in = false;
				}
				return;
			}
			stamina.value = std::min(stamina.max, stamina.value + stamina.regen * dt);
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	world.system<const PlayerIntent, const PlayerStats, const Stamina, SurfKnock, spatial::Transform>("PlayerMove")
		.term_at(0)
		.singleton()
		.with<Player>()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, const PlayerIntent& intent, const PlayerStats& stats,
				  const Stamina& stamina, SurfKnock& surf, spatial::Transform& transform) {
			const float dt = it.delta_time();
			const glm::vec2 current{transform.position.x, transform.position.z};
			const WaveSample wave = SampleWaves(it.world().get<WaveField>(), it.world().get<TidePools>().pools, current);
			// Dug in: the crab is rooted, so wind, waves and steering are all ignored.
			if (stamina.dug_in) {
				surf.velocity = glm::vec2{0.0F};
			} else if (wave.height > constants::kSurfMinHeight) {
				surf.velocity = wave.push;
			} else {
				surf.velocity *= std::exp(-constants::kKnockDecay * dt);
			}
			glm::vec2 velocity{0.0F};
			const float length = stamina.dug_in ? 0.0F : glm::length(intent.move);
			if (!stamina.dug_in) {
				// The wind pushes the crab whether it walks or not, so walking upwind is slower.
				velocity = WindPush(it.world().get<Wind>(), constants::kPlayerWindPush) + surf.velocity;
			}
			if (length > 0.0F) {
				const glm::vec2 direction = intent.move / length;
				velocity += direction * stats.speed;
				// Scuttle sideways: the crab faces 90° off its direction of travel.
				transform.rotation.y = std::atan2(direction.x, direction.y) + std::numbers::pi_v<float> / 2.0F;
			}
			glm::vec2 position = current + velocity * dt;
			PushOutOfRocks(it.world().get<Rocks>().circles, position, constants::kPlayerContactRadius);
			const float distance = glm::length(position);
			if (distance > constants::kArenaRadius) {
				position *= constants::kArenaRadius / distance;
			}
			transform.position.x = position.x;
			transform.position.z = position.y;
			// The crab rides the water; digging in squashes and sinks it.
			const float sink = stamina.dug_in ? constants::kDigInSink : 0.0F;
			transform.position.y = wave.height * constants::kWaterHeightScale - sink;
			transform.scale.y = stamina.dug_in ? constants::kDigInSquash : 1.0F;
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
								   .set<Stamina>(
									   {.value = constants::kStaminaMax,
										.max = constants::kStaminaMax,
										.regen = constants::kStaminaRegen,
										.drain = constants::kStaminaDrain}
								   )
								   .set<SurfKnock>({})
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
