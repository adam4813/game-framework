#include "boss.hpp"

#include <algorithm>
#include <cmath>

#include <spdlog/spdlog.h>

#include "arena.hpp"
#include "boss_registry.hpp"
#include "crabnado.hpp"
#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "player.hpp"
#include "run.hpp"
#include "sfx.hpp"
#include "waves.hpp"

using namespace engine;

namespace crabnado {

namespace {

// The crabnado breaks apart: its funnel and specks go, the root stays so the scene's handle stays valid.
void BreakApart(const flecs::world& world, const flecs::entity crabnado) {
	crabnado.children([](const flecs::entity part) { part.destruct(); });
	WinRun(world);
}

void StepBoss(const flecs::world& world, const flecs::entity entity, Crabnado& crabnado, spatial::Transform& transform, const float dt) {
	auto& run = world.get_mut<RunState>();
	const BossDef* boss = CurrentBoss(world);
	if (run.phase != RunPhase::Boss || boss == nullptr) {
		return;
	}
	const SlamTunables& tune = boss->slam;
	run.slam_cooldown = std::max(0.0F, run.slam_cooldown - dt);

	glm::vec2 position{transform.position.x, transform.position.z};
	const flecs::entity player = FindPlayer(world);
	glm::vec2 heading{0.0F};
	if (player) {
		const glm::vec3 p = player.get<spatial::Transform>().position;
		const glm::vec2 toPlayer = glm::vec2{p.x, p.z} - position;
		const float distance = glm::length(toPlayer);
		if (distance > 0.0001F) {
			heading = toPlayer / distance;
		}
		if (distance < tune.contact_range) {
			player.get_mut<PlayerStats>().hp -= tune.contact_dps * dt;
		}
	}

	const WaveSample wave = SampleWaves(world.get<WaveField>(), world.get<TidePools>().pools, position);
	if (wave.height > tune.knock_min_height) {
		crabnado.knock = wave.push * tune.knock_push;
	} else {
		crabnado.knock *= std::exp(-constants::kKnockDecay * dt);
	}

	position += (heading * tune.speed + crabnado.knock) * dt;
	const float reach = constants::kArenaRadius - tune.edge_margin;
	const float fromCentre = glm::length(position);
	if (fromCentre > reach) {
		position *= reach / fromCentre;
	}
	if (PushOutOfRocks(world.get<Rocks>().circles, position, tune.radius)) {
		if (glm::length(crabnado.knock) > tune.slam_min_speed && run.slam_cooldown <= 0.0F) {
			++run.slams;
			run.slam_cooldown = tune.slam_cooldown;
			run.banner_timer = constants::kBossBannerTime;
			crabnado.knock *= tune.slam_bounce;
			PlaySfx(world, Sfx::BossSlam);
			spdlog::info("[Run] Slam {}/{}", run.slams, tune.slams_to_win);
			if (run.slams >= tune.slams_to_win) {
				BreakApart(world, entity);
			}
		}
	}
	transform.position.x = position.x;
	transform.position.z = position.y;
}

} // namespace

void RegisterBoss(const flecs::world& world) {
	world.system<Crabnado, spatial::Transform>("BossUpdate")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, Crabnado& crabnado, spatial::Transform& transform) {
			StepBoss(it.world(), it.entity(i), crabnado, transform, it.delta_time());
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();
}

} // namespace crabnado
