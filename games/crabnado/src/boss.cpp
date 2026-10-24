#include "boss.hpp"

#include <algorithm>
#include <cmath>

#include <spdlog/spdlog.h>

#include "arena.hpp"
#include "crabnado.hpp"
#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "player.hpp"
#include "run.hpp"
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
	if (run.phase == RunPhase::Playing && run.clock >= constants::kBossAt) {
		run.phase = RunPhase::Boss;
		run.banner_timer = constants::kBossBannerTime;
		spdlog::info("[Run] The crabnado comes ashore at {:.1f}s", run.clock);
	}
	if (run.phase != RunPhase::Boss) {
		return;
	}
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
		if (distance < constants::kBossContactRange) {
			player.get_mut<PlayerStats>().hp -= constants::kBossContactDps * dt;
		}
	}

	const WaveSample wave = SampleWaves(world.get<WaveField>(), world.get<TidePools>().pools, position);
	if (wave.height > constants::kBossKnockMinHeight) {
		crabnado.knock = wave.push * constants::kBossKnockPush;
	} else {
		crabnado.knock *= std::exp(-constants::kKnockDecay * dt);
	}

	position += (heading * crabnado.speed + crabnado.knock) * dt;
	const float reach = constants::kArenaRadius - constants::kBossEdgeMargin;
	const float fromCentre = glm::length(position);
	if (fromCentre > reach) {
		position *= reach / fromCentre;
	}
	if (PushOutOfRocks(world.get<Rocks>().circles, position, constants::kBossRadius)) {
		if (glm::length(crabnado.knock) > constants::kBossSlamMinSpeed && run.slam_cooldown <= 0.0F) {
			++run.slams;
			run.slam_cooldown = constants::kBossSlamCooldown;
			run.banner_timer = constants::kBossBannerTime;
			crabnado.knock *= constants::kBossSlamBounce;
			spdlog::info("[Run] Slam {}/{}", run.slams, constants::kBossSlamsToWin);
			if (run.slams >= constants::kBossSlamsToWin) {
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
