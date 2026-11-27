#include "starting_village.hpp"

#include <algorithm>
#include <limits>

#include <glm/glm.hpp>

#include "world_data.hpp"

namespace game::farm {
namespace {

bool IsCoastal(const WorldData& world, const glm::ivec2 p, const int radius) {
	for (int y = p.y - radius; y <= p.y + radius; ++y) {
		for (int x = p.x - radius; x <= p.x + radius; ++x) {
			if (world.grid.InBounds(x, y) && world.grid.BiomeAt(x, y) == Biome::Ocean) {
				return true;
			}
		}
	}
	return false;
}

bool HasRoad(const WorldData& world, const int townId) {
	return std::ranges::any_of(world.roads.segments, [townId](const RoadSegment& s) {
		return s.townA == townId || s.townB == townId;
	});
}

bool HasRuinWithinRange(const WorldData& world, const glm::ivec2 pos, const int rangeSq) {
	return std::ranges::any_of(world.towns, [&](const TownSite& t) {
		if (t.status != TownStatus::Ruined) {
			return false;
		}
		const glm::ivec2 d = t.location - pos;
		return (d.x * d.x + d.y * d.y) <= rangeSq;
	});
}

} // namespace

std::optional<TownSite> SelectStartingVillage(const WorldData& world) {
	const int rangeSq = world.params.explorationRange * world.params.explorationRange;
	const glm::ivec2 centre{world.grid.width / 2, world.grid.height / 2};

	// Among all qualifying living towns, prefer the one closest to the map centre for a central start.
	const TownSite* best = nullptr;
	int bestDist = std::numeric_limits<int>::max();
	for (const TownSite& town : world.towns) {
		if (town.status != TownStatus::Living) {
			continue;
		}
		if (world.grid.BiomeAt(town.location.x, town.location.y) == Biome::Mountain) {
			continue;
		}
		if (IsCoastal(world, town.location, world.params.coastalRadius)) {
			continue;
		}
		if (!HasRoad(world, town.id)) {
			continue;
		}
		if (!HasRuinWithinRange(world, town.location, rangeSq)) {
			continue;
		}
		const glm::ivec2 delta = town.location - centre;
		const int dist = delta.x * delta.x + delta.y * delta.y;
		if (dist < bestDist) {
			bestDist = dist;
			best = &town;
		}
	}

	if (best == nullptr) {
		return std::nullopt;
	}
	return *best;
}

} // namespace game::farm
