#pragma once

#include <array>
#include <optional>
#include <span>

#include <glm/glm.hpp>

#include <flecs.h>

#include "engine/engine.hpp"

// The waves: crests that travel downwind in sets, drawn as a grid of raised, tinted water tiles and tumbling
// the enemies they pass. Every consumer of wave force samples SampleWaves; nothing else computes waves.
namespace crabnado {

// One swell: crests moving along `direction` (a compass bearing, as Wind::direction). Crest n reaches a point
// when its phase equals n; every third crest is big. `t0` is the swell's start on WaveField::time and `amp`
// its strength. `follow` makes it track the wind's direction each frame; `grow` and `fade` mark a swell
// building up or dying away (used from ticket 07).
struct Swell {
	float direction{0.0F};
	float t0{0.0F};
	float amp{0.0F};
	bool follow{false};
	bool grow{false};
	bool fade{false};
};

// Singleton: the wave state. `time` is the wave clock, advanced by the pausable WaveUpdate system.
// Fixed-size so it stays reflectable.
inline constexpr int kMaxSwells = 4;
struct WaveField {
	float time{0.0F};
	int swell_count{0};
	std::array<Swell, kMaxSwells> swells{};
};

// A calm patch: waves fade inside `radius` of `center`. The arena level's pools fill the TidePools singleton.
struct TidePool {
	glm::vec2 center{0.0F};
	float radius{0.0F};
};

// A water tile; its height and foam tint come from the wave field. `base` is its calm colour (pools are darker).
struct WaterTile {
	engine::core::Rgba base{};
};

// The water height and push (units per second) at one point.
struct WaveSample {
	float height{0.0F};
	glm::vec2 push{0.0F};
};

// Registers and reflects the wave components and the WaveField singleton, and the WaveUpdate and WaterTiles
// systems. Call from the module constructor after RegisterWind.
void RegisterWaves(const flecs::world& world);

// Starts a fresh wave field for a run: one swell following the wind.
void StartWaves(const flecs::world& world);

// The height and push at a ground position (x, z). Pools calm both.
[[nodiscard]] WaveSample SampleWaves(const WaveField& field, std::span<const TidePool> pools, glm::vec2 position);

// Seconds until the next big crest of the primary swell reaches `position`, if within `within` seconds.
[[nodiscard]] std::optional<float> NextBigCrestIn(const WaveField& field, glm::vec2 position, float within);

// Spawns the water tiles covering the arena as children of `parent`. The ArenaBuild system later tints the ones inside a pool.
void SpawnWaterTiles(const flecs::world& world, flecs::entity parent);

} // namespace crabnado
