#include "transition.hpp"

#include <array>
#include <vector>

#include <spdlog/spdlog.h>

#include "arena.hpp"
#include "campaign.hpp"
#include "crabnado.hpp"
#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "horde.hpp"
#include "player.hpp"
#include "run.hpp"
#include "shells.hpp"
#include "upgrades.hpp"
#include "waves.hpp"
#include "wind.hpp"

using namespace engine;

namespace crabnado {

namespace {

// Whether the run has an Act after the current one.
bool HasNextAct(const flecs::world& world) {
	return world.get<RunState>().act + 1 < static_cast<int>(world.get<Campaign>().acts.size());
}

void ResetPlayer(const flecs::world& world) {
	const flecs::entity player = FindPlayer(world);
	if (!player) {
		return;
	}
	player.get_mut<spatial::Transform>().position = glm::vec3{0.0F};
	auto& stats = player.get_mut<PlayerStats>();
	stats.hp = stats.max_hp;
	auto& stamina = player.get_mut<Stamina>();
	stamina.value = stamina.max;
	stamina.exhausted = false;
	stamina.dug_in = false;
	player.get_mut<SurfKnock>().velocity = glm::vec2{0.0F};
}

void ResetCrabnado(const flecs::world& world) {
	const flecs::entity crabnado = world.lookup("Arena::Crabnado");
	if (!crabnado) {
		return;
	}
	auto& state = crabnado.get_mut<Crabnado>();
	state = {.bearing = PickOrbitBearing(world), .radius = state.radius, .shifts_seen = world.get<Wind>().shifts};
}

// Destroys the horde (thrown enemies, wherever they are) and every shell on the ground. Collected first, since
// destroying inside a query's each would change the tables under it.
void ClearField(const flecs::world& world) {
	std::vector<flecs::entity> doomed;
	world.query_builder<const Enemy>().build().each([&doomed](const flecs::entity e, const Enemy&) { doomed.push_back(e); });
	world.query_builder<const Shell>().build().each([&doomed](const flecs::entity e, const Shell&) { doomed.push_back(e); });
	for (const flecs::entity e : doomed) {
		e.destruct();
	}
}

} // namespace

void StartNextAct(const flecs::world& world) {
	if (!HasNextAct(world)) {
		return;
	}
	auto& run = world.get_mut<RunState>();
	++run.act;
	run.stage = 0;
	run.stage_clock = 0.0F;
	run.act_clock = 0.0F;
	run.slams = 0;
	run.storm_up = 0.0F;
	run.banner_timer = 0.0F;
	run.stage_banner = 0.0F;

	const flecs::entity root = world.lookup("Arena");
	if (root) {
		const std::array<RockCircle, 1> keepClear{{{.center = glm::vec2{0.0F}, .radius = constants::kStartClearance}}};
		RescatterRocks(world, root, keepClear);
		if (const flecs::entity water = root.lookup("Water")) {
			water.destruct();
		}
		SpawnWaterTiles(world, root);
	}
	ClearField(world);
	StartWaves(world);
	ResetPlayer(world);
	ResetCrabnado(world);
	spdlog::info("[Campaign] Act {} '{}' begins", run.act, CurrentAct(world) != nullptr ? CurrentAct(world)->name : "?");
}

void RegisterTransition(const flecs::world& world) {
	world.component<TransitionStep>();
	// Not pausable: the scene is paused for the whole Transition.
	world.system<RunState>("ActTransition")
		.term_at(0)
		.src<RunState>()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, RunState& run) {
			if (run.phase != RunPhase::Transition) {
				return;
			}
			const flecs::world w = it.world();
			switch (run.step) {
			case TransitionStep::ActClear:
				run.transition_timer -= it.delta_time();
				if (run.transition_timer <= 0.0F) {
					run.step = TransitionStep::Comic;
				}
				break;
			case TransitionStep::Comic: {
				const Act* act = CurrentAct(w);
				spdlog::info("[Campaign] comic '{}' (placeholder)", act != nullptr ? act->comic_after : std::string{});
				if (!HasNextAct(w)) {
					WinRun(w);
					break;
				}
				StartNextAct(w);
				OfferFreeMolt(w);
				break;
			}
			case TransitionStep::Molt: break;
			}
		})
		.add<scene::GameScene>();
}

} // namespace crabnado
