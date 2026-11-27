#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "world_data.hpp"

// Data-driven town "style" contract: how a settlement's size and surroundings pick its ground
// material. A town's population maps to a TownTier; its biome selects a named TownStyle (falling
// back to "default"); the style lists, per tier, a weighted set of floor-material tile names. The
// tilemap builder rolls one dominant material per living town from this table, so a future town type
// (e.g. a forest hamlet paved with moss) is purely a JSON addition — zero C++ changes. The tier
// thresholds are a fixed design constant (constexpr); only the material weights are data-driven.
namespace game::farm {

// Settlement size bands. Larger bands bias toward more finished ground (dirt -> stone -> brick) and
// unlock more communal buildings (see building_catalog minTier). Values are 0-based so they index a
// per-tier weight list directly.
enum class TownTier : std::uint8_t {
	Hamlet = 0,
	Village = 1,
	Town = 2,
	City = 3,
	Count_,
};

// Population thresholds separating the tiers. A town with population < kVillagePop is a Hamlet, and
// so on. Fixed by design (not data-driven) so tier identity is stable across worlds and tests.
inline constexpr int kVillagePop = 30;
inline constexpr int kTownPop = 60;
inline constexpr int kCityPop = 95;

// Map a living town's population to its tier.
[[nodiscard]] TownTier TierForPopulation(int population);

// Canonical lowercase biome key used to look up a style ("plains", "forest", …). Stable so JSON
// authored against it keeps working.
[[nodiscard]] std::string_view BiomeName(Biome biome);

// One floor material and its relative selection weight within a tier.
struct FloorMaterialWeight {
	std::string material; // floor-tile name (resolved by the tilemap builder), e.g. "dirt"
	int weight{1};        // relative likelihood; clamped to >= 1 when rolled

	friend bool operator==(const FloorMaterialWeight&, const FloorMaterialWeight&) = default;
};

// A named ground-material palette: for each tier, the weighted materials a town of that tier may be
// paved with. A style with an empty list for a tier falls back to the table-wide default material.
struct TownStyle {
	std::vector<std::vector<FloorMaterialWeight>> tiers; // indexed by TownTier; sized Count_

	friend bool operator==(const TownStyle&, const TownStyle&) = default;
};

// The full style table: named styles, a biome->style mapping, and a last-resort material. Lookups
// fall back to the "default" style and then to `fallbackMaterial` so a living town is never left
// without ground.
struct TownStyleTable {
	std::vector<std::pair<std::string, TownStyle>> styles;      // style name -> palette
	std::vector<std::pair<std::string, std::string>> biomeStyle; // biome name -> style name
	std::string fallbackMaterial{"dirt"};

	[[nodiscard]] const TownStyle* FindStyle(std::string_view name) const;
	[[nodiscard]] std::string_view StyleForBiome(std::string_view biome) const;

	friend bool operator==(const TownStyleTable&, const TownStyleTable&) = default;
};

// Built-in palette (small-town dirt, biasing to cobblestone then brick as towns grow). Used when no
// town_styles.json is present and by unit tests so material selection works without file I/O.
[[nodiscard]] TownStyleTable DefaultTownStyleTable();

// Pure parse of a town-styles JSON document into a table. File-free (unit-testable). A missing or
// malformed document yields the built-in default so callers always get a usable table.
[[nodiscard]] TownStyleTable ParseTownStyleTable(const nlohmann::json& doc);

// Deterministically roll the dominant floor material for one living town from the table, keyed by
// its biome + tier and seeded by `seed` (so the same town always paves the same way).
[[nodiscard]] std::string
PickFloorMaterial(const TownStyleTable& table, Biome biome, TownTier tier, std::uint32_t seed);

} // namespace game::farm
