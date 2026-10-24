#include "waves.hpp"

#include <algorithm>
#include <cmath>

#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "wind.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr float kCrestSpacing = constants::kWaveSpeed * constants::kWavePeriod;
constexpr core::Rgba kWaterColor{.r = 0x2f, .g = 0x8f, .b = 0xa8, .a = 255};
constexpr core::Rgba kFoamColor{.r = 0xea, .g = 0xf7, .b = 0xfa, .a = 255};
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

core::Rgba Lerp(const core::Rgba a, const core::Rgba b, const float t) {
	const auto channel = [t](const uint8_t from, const uint8_t to) {
		return static_cast<uint8_t>(std::lround(static_cast<float>(from) + (static_cast<float>(to) - from) * t));
	};
	return {.r = channel(a.r, b.r), .g = channel(a.g, b.g), .b = channel(a.b, b.b), .a = 255};
}

} // namespace

void RegisterWaves(const flecs::world& world) {
	world.component<Swell>()
		.member<float>("direction")
		.member<float>("t0")
		.member<float>("amp")
		.member<bool>("follow")
		.member<bool>("grow")
		.member<bool>("fade");
	world.component<WaveField>()
		.member<float>("time")
		.member<int>("swell_count")
		.member<Swell>("swells", kMaxSwells);
	world.set<WaveField>({});
	world.component<WaterTile>();

	world.system<WaveField>("WaveUpdate")
		.term_at(0)
		.singleton()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, WaveField& field) {
			field.time += it.delta_time();
			const float windDirection = it.world().get<Wind>().direction;
			for (int i = 0; i < field.swell_count; ++i) {
				Swell& swell = field.swells[static_cast<size_t>(i)];
				if (swell.follow) {
					swell.direction = windDirection;
				}
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// Height and tint of every water tile from the one wave field.
	world.system<const WaterTile, spatial::Transform, render::Material>("WaterTiles")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, const WaterTile&, spatial::Transform& transform,
				 render::Material& material) {
			const auto& field = it.world().get<WaveField>();
			const float height = SampleWaves(field, {}, {transform.position.x, transform.position.z}).height;
			transform.position.y = height * constants::kWaterHeightScale - constants::kWaterTileThickness / 2.0F;
			const float foam = std::clamp((height - constants::kFoamMinHeight) / constants::kFoamRamp, 0.0F, 1.0F);
			material.color = Lerp(kWaterColor, kFoamColor, foam);
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();
}

void StartWaves(const flecs::world& world) {
	WaveField field;
	field.swell_count = 1;
	field.swells[0] = {.direction = world.get<Wind>().direction, .t0 = 0.0F, .amp = 1.0F, .follow = true};
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
		height += k;
		push += BearingDirection(swell.direction) * k;
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

void SpawnWaterTiles(const flecs::world& world, const flecs::entity parent) {
	const flecs::entity water = world.entity("Water").child_of(parent);
	const float reach = constants::kArenaRadius + constants::kWaterMargin;
	const int half = static_cast<int>(std::ceil(reach / constants::kWaterCell));
	const float size = constants::kWaterCell * constants::kWaterFill;
	for (int i = -half; i <= half; ++i) {
		for (int j = -half; j <= half; ++j) {
			const glm::vec2 center{static_cast<float>(i) * constants::kWaterCell,
								   static_cast<float>(j) * constants::kWaterCell};
			if (glm::length(center) > reach) {
				continue;
			}
			std::ignore = world.entity()
							  .child_of(water)
							  .add<WaterTile>()
							  .set<spatial::Transform>({.position = {center.x, 0.0F, center.y}})
							  .set<render::CubePrimitive>({.size = {size, constants::kWaterTileThickness, size}})
							  .set<render::Material>({.color = kWaterColor, .cast_shadow = false});
		}
	}
}

} // namespace crabnado
