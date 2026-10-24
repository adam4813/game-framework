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

// A rogue wave: one large crest travelling along `direction`. It reaches (x, z) when
// speed * (time - t0) equals the position's distance along the direction.
struct Rogue {
	float direction{0.0F};
	float t0{0.0F};
	float amp{0.0F};
};

// Singleton: the wave state. `time` is the wave clock, advanced by the pausable WaveUpdate system.
// Fixed-size so it stays reflectable. `last_shifts` is the Wind::shifts last seen, `rogue_timer` the seconds to
// the next rogue (counting down from the rogue stage), and `notice_timer` how long "A cross-swell is building" shows.
inline constexpr int kMaxSwells = 4;
inline constexpr int kMaxRogues = 4;
struct WaveField {
	float time{0.0F};
	int swell_count{0};
	std::array<Swell, kMaxSwells> swells{};
	int rogue_count{0};
	std::array<Rogue, kMaxRogues> rogues{};
	int last_shifts{0};
	float rogue_timer{0.0F};
	float notice_timer{0.0F};
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

// Starts a fresh wave field for a run: one swell following the wind and the first rogue timer rolled.
void StartWaves(const flecs::world& world);

// The height and push at a ground position (x, z). Pools calm both.
[[nodiscard]] WaveSample SampleWaves(const WaveField& field, std::span<const TidePool> pools, glm::vec2 position);

// Seconds until the next big crest of the primary swell reaches `position`, if within `within` seconds.
[[nodiscard]] std::optional<float> NextBigCrestIn(const WaveField& field, glm::vec2 position, float within);

// Seconds until the next rogue wave reaches `position`, if it is within `within` seconds.
[[nodiscard]] std::optional<float> NextRogueIn(const WaveField& field, glm::vec2 position, float within);

// The HUD name of the wave stage at run clock `clock`: "Steady swell", "Crossing swells" or "Rogue waves".
[[nodiscard]] const char* WaveStageName(float clock);

// Spawns the water tiles covering the arena as children of `parent`; tiles inside a TidePools pool are tinted darker.
void SpawnWaterTiles(const flecs::world& world, flecs::entity parent);

} // namespace crabnado
