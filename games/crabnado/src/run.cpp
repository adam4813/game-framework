#include "run.hpp"

#include <algorithm>
#include <cmath>

#include <spdlog/spdlog.h>

#include "crabnado_constants.hpp"
#include "controls.hpp"
#include "engine/engine.hpp"
#include "sfx.hpp"

using namespace engine;

namespace crabnado {

void RegisterRun(const flecs::world& world) {
	world.component<RunPhase>()
		.constant("Countdown", RunPhase::Countdown)
		.constant("Playing", RunPhase::Playing)
		.constant("Molting", RunPhase::Molting)
		.constant("Boss", RunPhase::Boss)
		.constant("Won", RunPhase::Won)
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
		.member<int>("slams")
		.member<float>("slam_cooldown")
		.member<float>("banner_timer")
		.member<uint64_t>("seed")
		.member<float>("countdown");
	world.set<RunState>({});

	world.system<RunState>("RunClock")
		.term_at(0)
		.singleton()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, RunState& run) {
			if (IsRunning(run.phase)) {
				run.clock += it.delta_time();
				run.banner_timer = std::max(0.0F, run.banner_timer - it.delta_time());
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// Not pausable: it runs while the countdown holds the scene paused. A click marks each number, a snap the "Go!".
	// It adds Paused itself rather than StartRun: during the scene's Load the arena isn't active yet, so the pause
	// would swap in the wrong scene's pipeline.
	world.system<RunState>("RunCountdown")
		.term_at(0)
		.singleton()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, RunState& run) {
			if (run.countdown <= -constants::kGoTime) {
				return;
			}
			if (run.phase == RunPhase::Countdown && !it.world().has<scene::Paused>()) {
				it.world().add<scene::Paused>();
			}
			const float before = run.countdown;
			run.countdown -= it.delta_time();
			if (before >= constants::kCountdownTime || (run.countdown > 0.0F && std::ceil(run.countdown) < std::ceil(before))) {
				PlaySfx(it.world(), Sfx::ClawClick);
			}
			if (run.phase == RunPhase::Countdown && run.countdown <= 0.0F) {
				run.phase = run.clock >= constants::kBossAt ? RunPhase::Boss : RunPhase::Playing;
				it.world().remove<scene::Paused>();
				PlaySfx(it.world(), Sfx::ClawSnap);
			}
		})
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
	return static_cast<int>(std::lround(
		static_cast<float>(constants::kShellsFirstMolt) * std::pow(constants::kShellsGrowth, static_cast<float>(moltLevel - 1))
	));
}

void StartRun(const flecs::world& world) {
	auto& rng = world.get_mut<ecs::RngState>();
	const uint64_t seed = rng.Next();
	rng.seed = seed;
	world.set<RunState>(
		{.phase = RunPhase::Countdown, .shells_needed = ShellsNeeded(1), .seed = seed, .countdown = constants::kCountdownTime}
	);
}

void SetMenuPaused(const flecs::world& world, const bool paused) {
	auto& run = world.get_mut<RunState>();
	if (!IsRunning(run.phase) || run.menu_paused == paused) {
		return;
	}
	run.menu_paused = paused;
	if (paused) {
		world.add<scene::Paused>();
	} else {
		world.remove<scene::Paused>();
	}
}

void WinRun(const flecs::world& world) {
	auto& run = world.get_mut<RunState>();
	if (run.phase != RunPhase::Boss) {
		return;
	}
	run.phase = RunPhase::Won;
	world.add<scene::Paused>();
	PlaySfx(world, Sfx::Victory);
	spdlog::info("[Run] Crabnado broken apart at {:.1f}s with {} kills", run.clock, run.kills);
}

void LoseRun(const flecs::world& world) {
	auto& run = world.get_mut<RunState>();
	if (!IsRunning(run.phase)) {
		return;
	}
	run.phase = RunPhase::Lost;
	world.add<scene::Paused>();
	PlaySfx(world, Sfx::PlayerDeath);
	spdlog::info("[Run] Swept away at {:.1f}s with {} kills", run.clock, run.kills);
}

} // namespace crabnado
