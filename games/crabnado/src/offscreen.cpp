#include "offscreen.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "campaign.hpp"
#include "crabnado.hpp"
#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "enemy_types.hpp"
#include "horde.hpp"
#include "player.hpp"
#include "run.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr std::string_view kSource = "offscreen";

// The horde root enemies are thrown into: the crabnado's ThrowsInto target. The crabnado may be disabled (an Act with
// no crabnado), which a plain query skips.
flecs::entity FindHorde(const flecs::world& world) {
	flecs::entity horde;
	world.query_builder<Crabnado>().build().each([&](const flecs::entity crabnado, Crabnado&) { horde = crabnado.target<ThrowsInto>(); });
	if (!horde) {
		world.query_builder().with<Crabnado>().with(flecs::Disabled).build().each([&](const flecs::entity crabnado) {
			horde = crabnado.target<ThrowsInto>();
		});
	}
	return horde;
}

// Throws one volley from `bearing` at the player, up to the horde cap. Each enemy is its own throw: its own type, jitter
// and flight time, so the volley doesn't land as one clump.
void ThrowVolley(const flecs::world& world, const OffscreenSpawner& tune, const std::vector<SpawnEntry>& spawns, const float bearing, int count) {
	count = std::min(count, constants::kHordeCap - world.count<Enemy>());
	const flecs::entity horde = FindHorde(world);
	if (count <= 0 || !horde) {
		return;
	}
	const glm::vec2 from = OrbitPoint(bearing, tune.radius);
	glm::vec2 target{0.0F};
	if (const flecs::entity player = FindPlayer(world)) {
		const glm::vec3 p = player.get<spatial::Transform>().position;
		target = {p.x, p.z};
	}
	for (int i = 0; i < count; ++i) {
		const int type = PickEnemyType(world, spawns, kSource);
		if (type >= 0) {
			ThrowEnemy(world, horde, type, from, EdgeLanding(world, from, target)).add<OffscreenThrown>();
		}
	}
}

void StepOffscreen(const flecs::world& world, const float dt) {
	const auto& run = world.get<RunState>();
	const Act* act = CurrentAct(world);
	const Stage* stage = CurrentStage(world);
	// Only a normal Stage's table throws from here; the Boss Stage's enemies come from the boss fight.
	if (act == nullptr || !act->offscreen || stage == nullptr || !IsRunning(run.phase) || run.act_clock < constants::kFirstThrowDelay) {
		return;
	}
	auto& state = world.get_mut<OffscreenState>();
	const bool hasEntries = std::ranges::any_of(stage->spawns, [](const SpawnEntry& entry) { return entry.source == kSource && entry.weight > 0.0F; });
	if (!hasEntries) {
		state.debt = 0.0F;
		return;
	}
	const OffscreenSpawner& tune = *act->offscreen;
	if (state.next_volley <= 0) {
		state.next_volley = std::min(
			tune.volley_max, static_cast<int>(world.get_mut<ecs::RngState>().NextFloatRange(
								 static_cast<float>(tune.volley_min), static_cast<float>(tune.volley_max) + 1.0F)));
	}
	state.debt += SpawnRate(stage->rate, run.stage_clock) * dt;
	if (state.debt < static_cast<float>(state.next_volley)) {
		return;
	}
	state.debt -= static_cast<float>(state.next_volley);
	state.bearing = PickVolleyBearing(world, state.has_bearing, state.bearing, tune.min_turn_deg);
	state.has_bearing = true;
	ThrowVolley(world, tune, stage->spawns, state.bearing, state.next_volley);
	state.next_volley = 0;
	// A long frame or a high rate must not queue volleys back to back.
	state.debt = std::min(state.debt, 1.0F);
}

} // namespace

float PickVolleyBearing(const flecs::world& world, const bool hasLast, const float last, const float minTurnDeg) {
	constexpr float tau = 2.0F * std::numbers::pi_v<float>;
	auto& rng = world.get_mut<ecs::RngState>();
	if (!hasLast) {
		return rng.NextFloatRange(0.0F, tau);
	}
	const float minTurn = std::clamp(minTurnDeg, 0.0F, 180.0F) * std::numbers::pi_v<float> / 180.0F;
	const float turn = rng.NextFloatRange(minTurn, std::numbers::pi_v<float>);
	const float side = rng.NextFloat() < 0.5F ? -1.0F : 1.0F;
	return std::remainder(last + side * turn, tau);
}

void RegisterOffscreen(const flecs::world& world) {
	world.component<OffscreenState>()
		.member<float>("debt")
		.member<int>("next_volley")
		.member<float>("bearing")
		.member<bool>("has_bearing");
	world.set<OffscreenState>({});

	world.system("OffscreenThrow")
		.kind(flecs::OnUpdate)
		.run([](flecs::iter& it) { StepOffscreen(it.world(), it.delta_time()); })
		.add<ecs::Pausable>()
		.add<scene::GameScene>();
}

} // namespace crabnado
