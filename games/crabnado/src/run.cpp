#include "run.hpp"

#include <spdlog/spdlog.h>

#include "crabnado_constants.hpp"
#include "controls.hpp"
#include "engine/engine.hpp"

using namespace engine;

namespace crabnado {

void RegisterRun(const flecs::world& world) {
	world.component<RunPhase>()
		.constant("Playing", RunPhase::Playing)
		.constant("Molting", RunPhase::Molting)
		.constant("Lost", RunPhase::Lost);
	world.component<RunState>()
		.member<RunPhase>("phase")
		.member<float>("clock")
		.member<int>("kills")
		.member<int>("rock_kills")
		.member<int>("molt_level")
		.member<int>("shells")
		.member<int>("shells_needed")
		.member<bool>("menu_paused")
		.member<uint64_t>("seed");
	world.set<RunState>({});

	world.system<RunState>("RunClock")
		.term_at(0)
		.singleton()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, RunState& run) {
			if (run.phase == RunPhase::Playing) {
				run.clock += it.delta_time();
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// Not pausable: Esc has to resume the menu it opened.
	world.system<const PlayerIntent, const RunState>("PauseToggle")
		.term_at(0)
		.singleton()
		.term_at(1)
		.singleton()
		.kind(flecs::PreUpdate)
		.each([](const flecs::iter& it, size_t, const PlayerIntent& intent, const RunState& run) {
			if (intent.pause) {
				SetMenuPaused(it.world(), !run.menu_paused);
			}
		})
		.add<scene::GameScene>();
}

int ShellsNeeded(const int moltLevel) {
	return constants::kShellsFirstMolt + (moltLevel - 1) * constants::kShellsPerMolt;
}

void StartRun(const flecs::world& world) {
	auto& rng = world.get_mut<ecs::RngState>();
	const uint64_t seed = rng.Next();
	rng.seed = seed;
	world.set<RunState>({.shells_needed = ShellsNeeded(1), .seed = seed});
}

void SetMenuPaused(const flecs::world& world, const bool paused) {
	auto& run = world.get_mut<RunState>();
	if (run.phase != RunPhase::Playing || run.menu_paused == paused) {
		return;
	}
	run.menu_paused = paused;
	if (paused) {
		world.add<scene::Paused>();
	} else {
		world.remove<scene::Paused>();
	}
}

void LoseRun(const flecs::world& world) {
	auto& run = world.get_mut<RunState>();
	if (run.phase != RunPhase::Playing) {
		return;
	}
	run.phase = RunPhase::Lost;
	world.add<scene::Paused>();
	spdlog::info("[Run] Swept away at {:.1f}s with {} kills", run.clock, run.kills);
}

} // namespace crabnado
