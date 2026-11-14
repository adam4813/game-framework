#include "world_report.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "world_data.hpp"

namespace game::farm {
namespace {

const char* BiomeName(const Biome b) {
	switch (b) {
	case Biome::Ocean: return "Ocean";
	case Biome::Lake: return "Lake";
	case Biome::River: return "River";
	case Biome::Beach: return "Beach";
	case Biome::Plains: return "Plains";
	case Biome::Forest: return "Forest";
	case Biome::Hills: return "Hills";
	case Biome::Mountain: return "Mountain";
	default: break;
	}
	return "Unknown";
}

const char* WaterTypeName(const WaterBodyType t) {
	switch (t) {
	case WaterBodyType::Ocean: return "Ocean";
	case WaterBodyType::Lake: return "Lake";
	case WaterBodyType::Pond: return "Pond";
	}
	return "Water";
}

std::string Coord(const glm::ivec2 p) { return "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")"; }

} // namespace

std::string FormatWorldReport(const WorldData& world) {
	std::ostringstream out;

	out << "# World Report — seed " << world.seed << "\n\n";

	// ── Parameters (so a shared report fully reproduces the world) ──
	const WorldGenParams& p = world.params;
	out << "## Parameters\n\n";
	out << "- Size: " << p.width << " x " << p.height << "\n";
	out
		<< "- Noise: scale "
		<< p.noiseScale
		<< ", octaves "
		<< p.fractal.octaves
		<< ", persistence "
		<< p.fractal.persistence
		<< ", lacunarity "
		<< p.fractal.lacunarity
		<< "\n";
	out << "- Edge falloff: " << p.edgeFalloff << "\n";
	out
		<< "- Elevation bands: sea "
		<< p.seaLevel
		<< ", beach "
		<< p.beachLevel
		<< ", plains "
		<< p.plainsLevel
		<< ", forest "
		<< p.forestLevel
		<< ", hills "
		<< p.hillsLevel
		<< "\n";
	out
		<< "- Rivers: "
		<< p.riverCount
		<< ", towns: "
		<< p.townCount
		<< ", town spacing: "
		<< p.minTownSpacing
		<< ", coastal radius: "
		<< p.coastalRadius
		<< ", exploration range: "
		<< p.explorationRange
		<< "\n\n";

	// ── Notable bodies of water (largest first; tiny 1–2 tile specks are not "bodies") ──
	constexpr int kMinNotableTiles = 8;
	out << "## Notable bodies of water\n\n";
	std::vector<WaterBody> water;
	for (const WaterBody& b : world.waterBodies) {
		if (b.tileCount >= kMinNotableTiles) {
			water.push_back(b);
		}
	}
	std::ranges::sort(water, [](const WaterBody& a, const WaterBody& b) { return a.tileCount > b.tileCount; });
	if (water.empty()) {
		out << "- (none)\n";
	}
	for (const WaterBody& b : water) {
		const std::string label = b.name.empty() ? std::string(WaterTypeName(b.type)) : b.name;
		out
			<< "- "
			<< label
			<< " ("
			<< WaterTypeName(b.type)
			<< ") — "
			<< b.tileCount
			<< " tiles, centre "
			<< Coord(b.centroid)
			<< "\n";
	}
	out << "- Rivers traced: " << world.rivers.size() << "\n\n";

	// ── Cities and their connections ──
	std::map<int, std::vector<std::string>> connections;
	for (const RoadSegment& seg : world.roads.segments) {
		const TownSite* a = world.FindTown(seg.townA);
		const TownSite* b = world.FindTown(seg.townB);
		if (a != nullptr && b != nullptr) {
			connections[seg.townA].push_back(b->name);
			connections[seg.townB].push_back(a->name);
		}
	}

	int living = 0;
	int ruined = 0;
	for (const TownSite& t : world.towns) {
		(t.status == TownStatus::Living ? living : ruined)++;
	}
	out << "## Cities (" << living << " living, " << ruined << " ruined)\n\n";

	std::vector<TownSite> towns = world.towns;
	std::ranges::sort(towns, [](const TownSite& a, const TownSite& b) { return a.name < b.name; });
	for (const TownSite& t : towns) {
		out
			<< "- **"
			<< t.name
			<< "** — "
			<< (t.status == TownStatus::Living ? "living" : "ruined")
			<< ", founded year "
			<< t.foundingYear;
		if (t.status == TownStatus::Ruined) {
			out << ", collapsed year " << t.collapseYear;
		}
		out
			<< ", at "
			<< Coord(t.location)
			<< " on "
			<< BiomeName(world.grid.BiomeAt(t.location.x, t.location.y))
			<< ", pop "
			<< t.size
			<< "\n";

		auto it = connections.find(t.id);
		if (it == connections.end() || it->second.empty()) {
			out << "  - Connections: none (isolated)\n";
		}
		else {
			std::ranges::sort(it->second);
			out << "  - Connections: ";
			for (size_t i = 0; i < it->second.size(); ++i) {
				out << (i == 0 ? "" : ", ") << it->second[i];
			}
			out << "\n";
		}
	}
	out << "\n";

	// ── Full chronological history (the generation timeline) ──
	out << "## History (" << world.timeline.steps.size() << " steps)\n\n";
	for (size_t i = 0; i < world.timeline.steps.size(); ++i) {
		out << i + 1 << ". " << world.timeline.steps[i].label << "\n";
	}

	return out.str();
}

} // namespace game::farm
