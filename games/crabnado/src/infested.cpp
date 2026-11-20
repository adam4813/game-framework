#include "infested.hpp"

#include <algorithm>
#include <cmath>

#include "campaign.hpp"
#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "horde.hpp"
#include "run.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr std::string_view kSource = "infested_crest";
// Where on the swell's reverse bearing a crest is rolled: kBigCrestWarning seconds before it reaches the barrier.
constexpr float kRollReach = constants::kArenaRadius + constants::kWaveSpeed * constants::kBigCrestWarning;
constexpr float kSpawnAlong = -(constants::kArenaRadius + constants::kInfestedSpawnReach);

const SpawnEntry* FindEntry(const flecs::world& world) {
	const Stage* stage = CurrentStage(world);
	if (stage == nullptr || !IsRunning(world.get<RunState>().phase)) {
		return nullptr;
	}
	const auto found = std::ranges::find_if(stage->spawns, [](const SpawnEntry& entry) { return entry.source == kSource; });
	return found == stage->spawns.end() ? nullptr : &*found;
}

// Puts the infested crest's seahorses evenly along the part of its front that will cross the arena.
void SpawnRiders(const flecs::world& world, InfestState& state, const CrestFront& front) {
	const flecs::entity horde = FindHorde(world);
	const int count = std::min(state.riders, constants::kHordeCap - world.count<Enemy>());
	state.riders_out = true;
	if (!horde || count <= 0) {
		return;
	}
	auto& rng = world.get_mut<ecs::RngState>();
	const glm::vec2 side{-front.direction.y, front.direction.x};
	const float half = constants::kArenaRadius * constants::kInfestedChordFill;
	for (int i = 0; i < count; ++i) {
		const float across = -half + 2.0F * half * (static_cast<float>(i) + 0.5F) / static_cast<float>(count);
		const glm::vec2 jitter{
			rng.NextFloatRange(-constants::kInfestedJitter, constants::kInfestedJitter),
			rng.NextFloatRange(-constants::kInfestedJitter, constants::kInfestedJitter)
		};
		const glm::vec2 position = front.direction * front.along + side * across + jitter;
		SpawnSurfer(world, horde, state.rider_type, position, {.swell = state.crest_swell, .n = state.crest_n});
	}
}

void StepInfested(const flecs::world& world) {
	const auto& field = world.get<WaveField>();
	auto& state = world.get_mut<InfestState>();
	if (field.time < state.stamp) {
		state = {};
	}
	state.stamp = field.time;

	if (state.active) {
		const auto front = CrestFrontOf(field, {.swell = state.crest_swell, .n = state.crest_n});
		if (!front || front->along > constants::kArenaRadius + constants::kInfestedEndMargin) {
			state.active = false;
		} else if (!state.riders_out && front->along >= kSpawnAlong) {
			SpawnRiders(world, state, *front);
		}
	}

	int swell = 0;
	const auto phase = PrimaryPhaseUpwind(field, kRollReach, swell);
	if (!phase) {
		state.tracking = false;
		return;
	}
	const float t0 = field.swells[static_cast<size_t>(swell)].t0;
	const int newest = static_cast<int>(std::floor(*phase));
	if (!state.tracking || state.swell_t0 != t0) {
		state.tracking = true;
		state.swell_t0 = t0;
		state.last_n = newest;
		return;
	}
	if (newest <= state.last_n) {
		return;
	}
	state.last_n = newest;
	const SpawnEntry* entry = FindEntry(world);
	if (entry == nullptr || state.active || field.time - state.last_infest < entry->min_gap) {
		return;
	}
	auto& rng = world.get_mut<ecs::RngState>();
	if (rng.NextFloat() >= entry->chance) {
		return;
	}
	state.active = true;
	state.riders_out = false;
	state.last_infest = field.time;
	state.crest_swell = swell;
	state.crest_n = newest;
	state.rider_type = entry->type;
	state.riders = entry->riders_min + static_cast<int>(rng.NextFloat() * static_cast<float>(entry->riders_max - entry->riders_min + 1));
	state.riders = std::min(state.riders, entry->riders_max);
}

} // namespace

std::optional<float> InfestedCrestIn(const flecs::world& world) {
	const auto& state = world.get<InfestState>();
	if (!state.active) {
		return std::nullopt;
	}
	const auto front = CrestFrontOf(world.get<WaveField>(), {.swell = state.crest_swell, .n = state.crest_n});
	if (!front) {
		return std::nullopt;
	}
	const float seconds = (-constants::kArenaRadius - front->along) / constants::kWaveSpeed;
	return seconds > 0.0F ? std::optional{seconds} : std::nullopt;
}

float InfestedTint(const InfestState& state, const WaveField& field, const glm::vec2 position) {
	if (!state.active) {
		return 0.0F;
	}
	const auto front = CrestFrontOf(field, {.swell = state.crest_swell, .n = state.crest_n});
	if (!front) {
		return 0.0F;
	}
	const float distance = glm::dot(position, front->direction) - front->along;
	return constants::kInfestedTintMax * std::exp(-(distance * distance) / (constants::kWaveWidth * constants::kWaveWidth));
}

void RegisterInfested(const flecs::world& world) {
	world.component<InfestState>()
		.member<bool>("tracking")
		.member<float>("swell_t0")
		.member<int>("last_n")
		.member<float>("last_infest")
		.member<bool>("active")
		.member<int>("crest_swell")
		.member<int>("crest_n")
		.member<bool>("riders_out")
		.member<int>("rider_type")
		.member<int>("riders")
		.member<float>("stamp");
	world.set<InfestState>({});

	world.system("InfestedCrest")
		.kind(flecs::OnUpdate)
		.run([](flecs::iter& it) { StepInfested(it.world()); })
		.add<ecs::Pausable>()
		.add<scene::GameScene>();
}

} // namespace crabnado
