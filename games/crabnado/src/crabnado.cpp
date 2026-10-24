#include "crabnado.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "horde.hpp"
#include "player.hpp"
#include "run.hpp"

using namespace engine;

namespace crabnado {

namespace {

// Look: a funnel of flattened spheres widening upward, with crab-red specks caught in it. The whole
// crabnado spins, which carries the specks around the funnel.
constexpr core::Rgba kFunnelColor{.r = 205, .g = 220, .b = 228, .a = 255};
constexpr core::Rgba kSpeckColor{.r = 216, .g = 69, .b = 43, .a = 255};
constexpr int kFunnelLayers = 6;
constexpr float kFunnelLayerHeight = 3.6F;
constexpr float kFunnelBaseRadius = 1.4F;
constexpr float kFunnelFlare = 1.5F; // extra radius per layer
constexpr int kSpecks = 14;
constexpr float kSpin = 2.0F; // radians per second

// Throws up to `count` crabs from the crabnado toward the player, stopping at the horde cap.
void ThrowAtPlayer(const flecs::world& world, const flecs::entity crabnado, const glm::vec3 position, int count) {
	count = std::min(count, constants::kHordeCap - world.count<Enemy>());
	const flecs::entity horde = crabnado.target<ThrowsInto>();
	if (count <= 0 || !horde) {
		return;
	}
	const glm::vec2 from{position.x, position.z};
	glm::vec2 target = from;
	if (const flecs::entity player = FindPlayer(world)) {
		const glm::vec3 p = player.get<spatial::Transform>().position;
		target = {p.x, p.z};
	}
	for (int i = 0; i < count; ++i) {
		ThrowCrab(world, horde, from, target);
	}
}

glm::vec2 PickWaypoint(ecs::RngState& rng) {
	const float angle = rng.NextFloatRange(0.0F, 2.0F * std::numbers::pi_v<float>);
	// sqrt spreads waypoints evenly over the disc instead of bunching them at the centre.
	const float radius =
		constants::kArenaRadius * constants::kCrabnadoRoamFraction * std::sqrt(rng.NextFloat());
	return {std::cos(angle) * radius, std::sin(angle) * radius};
}

} // namespace

void RegisterCrabnado(const flecs::world& world) {
	world.component<Crabnado>()
		.member<glm::vec2>("waypoint")
		.member<float>("speed")
		.member<float>("spawn_debt");
	world.component<ThrowsInto>().add(flecs::Exclusive);

	world.system<Crabnado, spatial::Transform>("CrabnadoRoam")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, Crabnado& crabnado, spatial::Transform& transform) {
			const float dt = it.delta_time();
			transform.rotation.y += kSpin * dt;
			const glm::vec2 position{transform.position.x, transform.position.z};
			const glm::vec2 toWaypoint = crabnado.waypoint - position;
			const float distance = glm::length(toWaypoint);
			if (distance <= constants::kCrabnadoArriveRadius) {
				crabnado.waypoint = PickWaypoint(it.world().get_mut<ecs::RngState>());
				return;
			}
			const glm::vec2 step = toWaypoint / distance * std::min(crabnado.speed * dt, distance);
			transform.position.x += step.x;
			transform.position.z += step.y;
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// The spawn rate rises linearly with the run clock.
	world.system<Crabnado, const spatial::Transform>("CrabnadoThrow")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, Crabnado& crabnado, const spatial::Transform& transform) {
			const flecs::world world = it.world();
			const auto& run = world.get<RunState>();
			if (run.phase != RunPhase::Playing) {
				return;
			}
			crabnado.spawn_debt +=
				(constants::kSpawnRateBase + constants::kSpawnRateGrowth * run.clock) * it.delta_time();
			const int throws = static_cast<int>(crabnado.spawn_debt);
			crabnado.spawn_debt -= static_cast<float>(throws);
			ThrowAtPlayer(world, it.entity(i), transform.position, throws);
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();
}

flecs::entity SpawnCrabnado(
	const flecs::world& world, const flecs::entity parent, const flecs::entity horde, const glm::vec2 position
) {
	auto& rng = world.get_mut<ecs::RngState>();
	const flecs::entity crabnado = world.entity("Crabnado")
									  .child_of(parent)
									  .set<Crabnado>({.waypoint = PickWaypoint(rng), .speed = constants::kCrabnadoSpeed})
									  .add<ThrowsInto>(horde)
									  .set<spatial::Transform>({.position = {position.x, 0.0F, position.y}});

	for (int layer = 0; layer < kFunnelLayers; ++layer) {
		const float radius = kFunnelBaseRadius + kFunnelFlare * static_cast<float>(layer);
		std::ignore = world.entity()
						  .child_of(crabnado)
						  .set<spatial::Transform>(
							  {.position = {0.0F, kFunnelLayerHeight * (static_cast<float>(layer) + 0.5F), 0.0F},
							   .scale = {1.0F, 0.45F, 1.0F}}
						  )
						  .set<render::SpherePrimitive>({.radius = radius})
						  .set<render::Material>({.color = kFunnelColor, .cast_shadow = false});
	}
	const float height = kFunnelLayerHeight * static_cast<float>(kFunnelLayers);
	for (int i = 0; i < kSpecks; ++i) {
		const float h = rng.NextFloat();
		const float angle = rng.NextFloatRange(0.0F, 2.0F * std::numbers::pi_v<float>);
		// Just outside the funnel wall at that height.
		const float radius = kFunnelBaseRadius + kFunnelFlare * static_cast<float>(kFunnelLayers) * h + 0.6F;
		std::ignore = world.entity()
						  .child_of(crabnado)
						  .set<spatial::Transform>(
							  {.position = {std::cos(angle) * radius, h * height, std::sin(angle) * radius}}
						  )
						  .set<render::SpherePrimitive>({.radius = 0.45F})
						  .set<render::Material>({.color = kSpeckColor});
	}
	return crabnado;
}

void ThrowBurst(const flecs::entity crabnado, const int count) {
	ThrowAtPlayer(crabnado.world(), crabnado, crabnado.get<spatial::Transform>().position, count);
}

} // namespace crabnado
