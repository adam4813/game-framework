#include "waves.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "arena.hpp"
#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "run.hpp"
#include "wind.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr float kCrestSpacing = constants::kWaveSpeed * constants::kWavePeriod;
constexpr core::Rgba kWaterColor{.r = 0x2f, .g = 0x8f, .b = 0xa8, .a = 255};
constexpr core::Rgba kOceanColor{.r = 0x22, .g = 0x6e, .b = 0x8e, .a = 255}; // past the barrier
constexpr core::Rgba kPoolColor{.r = 0x1d, .g = 0x66, .b = 0x80, .a = 255};
constexpr core::Rgba kFoamColor{.r = 0xea, .g = 0xf7, .b = 0xfa, .a = 255};
// Water tile sizes: the fine arena grid, then the near and far open ocean (see kOceanNearReach).
constexpr int kOceanLevels = 3;
// How far past the primary swell's crest a crest still counts as "arriving" for the warning.
constexpr int kBigCrestLookahead = 3;

// Crest index n of a swell reaches a point when this phase equals n.
float Phase(const Swell& swell, const float time, const glm::vec2 position) {
	const float along = glm::dot(position, BearingDirection(swell.direction));
	return (constants::kWaveSpeed * (time - swell.t0) - along) / kCrestSpacing;
}

bool IsBigCrest(const int n) {
	return ((n % 3) + 3) % 3 == 2;
}

// The newest swell that is still building or steady: the one players read.
const Swell* PrimarySwell(const WaveField& field) {
	for (int i = field.swell_count - 1; i >= 0; --i) {
		if (!field.swells[static_cast<size_t>(i)].fade && field.swells[static_cast<size_t>(i)].amp >= 0.01F) {
			return &field.swells[static_cast<size_t>(i)];
		}
	}
	return nullptr;
}

void RemoveSwell(WaveField& field, const int index) {
	for (int i = index; i + 1 < field.swell_count; ++i) {
		field.swells[static_cast<size_t>(i)] = field.swells[static_cast<size_t>(i + 1)];
	}
	--field.swell_count;
}

// A wind shift past kCrossSwellAt: every swell stops following and fades, and a new one builds along the
// shift's target bearing. Before then the one swell just follows the turning wind.
void AddCrossSwell(WaveField& field, const Wind& wind, ecs::RngState& rng) {
	for (int i = 0; i < field.swell_count; ++i) {
		field.swells[static_cast<size_t>(i)].follow = false;
		field.swells[static_cast<size_t>(i)].fade = true;
	}
	if (field.swell_count == kMaxSwells) {
		int oldest = 0;
		for (int i = 0; i < field.swell_count; ++i) {
			if (field.swells[static_cast<size_t>(i)].fade) {
				oldest = i;
				break;
			}
		}
		RemoveSwell(field, oldest);
	}
	field.swells[static_cast<size_t>(field.swell_count++)] = {
		.direction = wind.direction + wind.turn,
		.t0 = field.time + rng.NextFloatRange(0.0F, constants::kWavePeriod),
		.amp = 0.0F,
		.follow = true,
		.grow = true};
	field.notice_timer = constants::kCrossSwellNotice;
}

// One rogue, rising at the upwind edge of the open ocean so it rolls in across the arena.
void AddRogue(WaveField& field, const Wind& wind, ecs::RngState& rng) {
	if (field.rogue_count == kMaxRogues) {
		return;
	}
	field.rogues[static_cast<size_t>(field.rogue_count++)] = {
		.direction = wind.direction + rng.NextFloatRange(-constants::kRogueSpread, constants::kRogueSpread),
		.t0 = field.time + constants::kOceanFarReach / constants::kWaveSpeed,
		.amp = constants::kRogueAmp};
}

void StepWaves(const flecs::world& world, WaveField& field, const float dt) {
	const float clock = world.get<RunState>().clock;
	const RunPhase phase = world.get<RunState>().phase;
	const auto& wind = world.get<Wind>();
	auto& rng = world.get_mut<ecs::RngState>();
	field.time += dt;
	field.notice_timer = std::max(0.0F, field.notice_timer - dt);

	if (wind.shifts != field.last_shifts) {
		field.last_shifts = wind.shifts;
		if (clock >= constants::kCrossSwellAt && phase != RunPhase::Boss) {
			AddCrossSwell(field, wind, rng);
		}
	}
	for (int i = 0; i < field.swell_count; ++i) {
		Swell& swell = field.swells[static_cast<size_t>(i)];
		if (swell.follow) {
			swell.direction = wind.direction;
		}
		if (swell.grow) {
			swell.amp = std::min(1.0F, swell.amp + dt / constants::kSwellGrowTime);
			swell.grow = swell.amp < 1.0F;
		}
		if (swell.fade) {
			swell.amp -= dt / constants::kSwellFadeTime;
		}
	}
	for (int i = field.swell_count - 1; i >= 0; --i) {
		const Swell& swell = field.swells[static_cast<size_t>(i)];
		if (swell.amp < 0.01F && !swell.grow) {
			RemoveSwell(field, i);
		}
	}

	if (clock >= constants::kRogueAt && phase != RunPhase::Boss) {
		field.rogue_timer -= dt;
		if (field.rogue_timer <= 0.0F) {
			field.rogue_timer = rng.NextFloatRange(constants::kRogueGapMin, constants::kRogueGapMax);
			AddRogue(field, wind, rng);
		}
	}
	int kept = 0;
	for (int i = 0; i < field.rogue_count; ++i) {
		const Rogue rogue = field.rogues[static_cast<size_t>(i)];
		if (constants::kWaveSpeed * (field.time - rogue.t0) < constants::kOceanFarReach) {
			field.rogues[static_cast<size_t>(kept++)] = rogue;
		}
	}
	field.rogue_count = kept;
}

core::Rgba Lerp(const core::Rgba a, const core::Rgba b, const float t) {
	const auto channel = [t](const uint8_t from, const uint8_t to) {
		return static_cast<uint8_t>(std::lround(static_cast<float>(from) + (static_cast<float>(to) - from) * t));
	};
	return {.r = channel(a.r, b.r), .g = channel(a.g, b.g), .b = channel(a.b, b.b), .a = 255};
}

// Places the water tile of size `level` whose first fine cell is (i, j). The tile sizes form a quadtree over the
// fine grid, each twice the last; a tile that reaches into the next finer size's disc splits into four, so the
// sizes meet without gaps or overlaps. `spawn` takes the tile's centre and cell size.
void PlaceWaterTile(const int level, const int i, const int j, const auto& spawn) {
	constexpr std::array<float, kOceanLevels> kReach{
		constants::kArenaRadius + constants::kWaterMargin, constants::kOceanNearReach, constants::kOceanFarReach};
	const int span = 1 << level; // fine cells along one side
	const float cell = constants::kWaterCell * static_cast<float>(span);
	// Fine cell i is centred on i * kWaterCell, so the tile's centre is half its span on from cell i's.
	const glm::vec2 center =
		(glm::vec2{static_cast<float>(i), static_cast<float>(j)} + (static_cast<float>(span) - 1.0F) * 0.5F) * constants::kWaterCell;
	if (level > 0) {
		const glm::vec2 nearest = glm::clamp(glm::vec2{0.0F}, center - cell * 0.5F, center + cell * 0.5F);
		if (glm::length(nearest) < kReach[static_cast<size_t>(level - 1)]) {
			const int half = span / 2;
			PlaceWaterTile(level - 1, i, j, spawn);
			PlaceWaterTile(level - 1, i + half, j, spawn);
			PlaceWaterTile(level - 1, i, j + half, spawn);
			PlaceWaterTile(level - 1, i + half, j + half, spawn);
			return;
		}
	}
	if (glm::length(center) <= kReach[static_cast<size_t>(kOceanLevels - 1)]) {
		spawn(center, cell);
	}
}

} // namespace

void RegisterWaves(const flecs::world& world) {
	world.component<Swell>()
		.member<float>("direction")
		.member<float>("t0")
		.member<float>("amp")
		.member<bool>("follow")
		.member<bool>("grow")
		.member<bool>("fade")
		.member<int>("boost_n")
		.member<float>("boost_mult");
	world.component<Rogue>().member<float>("direction").member<float>("t0").member<float>("amp");
	world.component<WaveField>()
		.member<float>("time")
		.member<int>("swell_count")
		.member<Swell>("swells", kMaxSwells)
		.member<int>("rogue_count")
		.member<Rogue>("rogues", kMaxRogues)
		.member<int>("last_shifts")
		.member<float>("rogue_timer")
		.member<float>("notice_timer");
	world.set<WaveField>({});
	world.component<WaterTile>().member<core::Rgba>("base");

	world.system<WaveField>("WaveUpdate")
		.term_at(0)
		.singleton()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, WaveField& field) { StepWaves(it.world(), field, it.delta_time()); })
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// Height and tint of every water tile from the one wave field.
	world.system<const WaterTile, spatial::Transform, render::Material>("WaterTiles")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, const WaterTile& tile, spatial::Transform& transform,
				 render::Material& material) {
			const auto& field = it.world().get<WaveField>();
			const auto& pools = it.world().get<TidePools>().pools;
			const float height = SampleWaves(field, pools, {transform.position.x, transform.position.z}).height;
			transform.position.y = height * constants::kWaterHeightScale - constants::kWaterTileThickness / 2.0F;
			const float foam = std::clamp((height - constants::kFoamMinHeight) / constants::kFoamRamp, 0.0F, 1.0F);
			material.color = Lerp(tile.base, kFoamColor, foam);
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();
}

void StartWaves(const flecs::world& world) {
	WaveField field;
	field.swell_count = 1;
	field.swells[0] = {.direction = world.get<Wind>().direction, .t0 = 0.0F, .amp = 1.0F, .follow = true};
	field.last_shifts = world.get<Wind>().shifts;
	field.rogue_timer =
		world.get_mut<ecs::RngState>().NextFloatRange(constants::kRogueFirstMin, constants::kRogueFirstMax);
	world.set<WaveField>(field);
}

WaveSample SampleWaves(const WaveField& field, const std::span<const TidePool> pools, const glm::vec2 position) {
	float height = 0.0F;
	glm::vec2 push{0.0F};
	for (int i = 0; i < field.swell_count; ++i) {
		const Swell& swell = field.swells[static_cast<size_t>(i)];
		if (swell.amp < 0.01F) {
			continue;
		}
		const float q = Phase(swell, field.time, position);
		const float n = std::round(q);
		const float distance = (q - n) * kCrestSpacing;
		float k = std::exp(-(distance * distance) / (constants::kWaveWidth * constants::kWaveWidth)) * swell.amp;
		if (IsBigCrest(static_cast<int>(n))) {
			k *= constants::kWaveBigAmp;
		}
		if (swell.boost_mult > 0.0F && static_cast<int>(n) == swell.boost_n) {
			k *= swell.boost_mult;
		}
		height += k;
		push += BearingDirection(swell.direction) * k;
	}
	for (int i = 0; i < field.rogue_count; ++i) {
		const Rogue& rogue = field.rogues[static_cast<size_t>(i)];
		const float distance = constants::kWaveSpeed * (field.time - rogue.t0) -
							   glm::dot(position, BearingDirection(rogue.direction));
		const float k = std::exp(-(distance * distance) / (constants::kWaveWidth * constants::kWaveWidth *
															 constants::kRogueWidthK)) *
						rogue.amp;
		height += k;
		push += BearingDirection(rogue.direction) * k;
	}
	for (const TidePool& pool : pools) {
		const float distance = glm::length(position - pool.center);
		if (distance < pool.radius) {
			const float edge =
				std::clamp((pool.radius - distance) / (pool.radius * constants::kTidePoolCalmEdge), 0.0F, 1.0F);
			const float calm = 1.0F - constants::kTidePoolCalm * edge;
			height *= calm;
			push *= calm;
		}
	}
	return {.height = height, .push = push * constants::kWavePushK};
}

std::optional<float> NextBigCrestIn(const WaveField& field, const glm::vec2 position, const float within) {
	const Swell* swell = PrimarySwell(field);
	if (swell == nullptr) {
		return std::nullopt;
	}
	const float q = Phase(*swell, field.time, position);
	const int first = static_cast<int>(std::ceil(q));
	for (int n = first; n < first + kBigCrestLookahead; ++n) {
		if (IsBigCrest(n)) {
			const float seconds = (static_cast<float>(n) - q) * constants::kWavePeriod;
			return seconds <= within ? std::optional{seconds} : std::nullopt;
		}
	}
	return std::nullopt;
}

std::optional<float> BoostNextCrest(
	WaveField& field, const glm::vec2 position, const float lead, const float multiplier
) {
	const Swell* primary = PrimarySwell(field);
	if (primary == nullptr) {
		return std::nullopt;
	}
	Swell& swell = field.swells[static_cast<size_t>(primary - field.swells.data())];
	const float q = Phase(swell, field.time, position);
	int n = static_cast<int>(std::ceil(q));
	if ((static_cast<float>(n) - q) * constants::kWavePeriod < lead) {
		++n;
	}
	swell.boost_n = n;
	swell.boost_mult = multiplier;
	return (static_cast<float>(n) - q) * constants::kWavePeriod;
}

std::optional<float> BoostedCrestIn(const WaveField& field, const glm::vec2 position, const float within) {
	const Swell* swell = PrimarySwell(field);
	if (swell == nullptr || swell->boost_mult <= 0.0F) {
		return std::nullopt;
	}
	const float seconds = (static_cast<float>(swell->boost_n) - Phase(*swell, field.time, position)) * constants::kWavePeriod;
	return seconds > 0.0F && seconds <= within ? std::optional{seconds} : std::nullopt;
}

std::optional<float> NextRogueIn(const WaveField& field, const glm::vec2 position, const float within) {
	std::optional<float> soonest;
	for (int i = 0; i < field.rogue_count; ++i) {
		const Rogue& rogue = field.rogues[static_cast<size_t>(i)];
		const float eta = glm::dot(position, BearingDirection(rogue.direction)) / constants::kWaveSpeed + rogue.t0 -
						  field.time;
		if (eta > 0.0F && eta < within && (!soonest || eta < *soonest)) {
			soonest = eta;
		}
	}
	return soonest;
}

const char* WaveStageName(const float clock) {
	if (clock < constants::kCrossSwellAt) {
		return "Steady swell";
	}
	return clock < constants::kRogueAt ? "Crossing swells" : "Rogue waves";
}

void SpawnWaterTiles(const flecs::world& world, const flecs::entity parent) {
	const flecs::entity water = world.entity("Water").child_of(parent);
	const auto& pools = world.get<TidePools>().pools;
	const auto spawnTile = [&](const glm::vec2 center, const float cell) {
		core::Rgba base = kWaterColor;
		if (glm::length(center) > constants::kArenaRadius) {
			base = kOceanColor;
		} else if (std::ranges::any_of(pools, [&](const TidePool& pool) { return glm::length(center - pool.center) < pool.radius; })) {
			base = kPoolColor;
		}
		const float size = cell * constants::kWaterFill;
		std::ignore = world.entity()
						  .child_of(water)
						  .set<WaterTile>({.base = base})
						  .set<spatial::Transform>({.position = {center.x, 0.0F, center.y}})
						  .set<render::CubePrimitive>({.size = {size, constants::kWaterTileThickness, size}})
						  .set<render::Material>({.color = base, .cast_shadow = false});
	};

	const int top = 1 << (kOceanLevels - 1);
	const int blocks = static_cast<int>(std::ceil(constants::kOceanFarReach / (constants::kWaterCell * static_cast<float>(top))));
	for (int a = -blocks; a < blocks; ++a) {
		for (int b = -blocks; b < blocks; ++b) {
			PlaceWaterTile(kOceanLevels - 1, a * top, b * top, spawnTile);
		}
	}
}

} // namespace crabnado
