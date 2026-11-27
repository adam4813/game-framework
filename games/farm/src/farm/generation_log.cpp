#include "generation_log.hpp"

#include <fstream>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "farm_world_gen.hpp" // WorldGenParamsToJson — shared, faithful params serializer
#include "road_hubs.hpp"       // kHubMarkerKind — the object kind of a road-hub signpost

namespace game::farm {
namespace {

using nlohmann::json;

constexpr int kSchemaVersion = 1;

// The self-documenting schema string embedded in every log (see generation_log.hpp for the full
// prose). Keeps a log parseable by a future replay pass without reverse-engineering.
constexpr std::string_view kSchemaDoc =
	"Farm world generation log v1. Keys: seed (int); grid{width,height}; params{all WorldGenParams "
	"knobs}; history[{year,type,a,b,description}] ordered; towns[{id,name,location[x,y],size,status,"
	"foundingYear,collapseYear,isOutpost}]; townLayouts[{townId,ruined,landmarkKind,landmark[x,y],"
	"buildings[{kind,villagerArchetype,footprint[[x,y]],door[x,y],ruined}],floorTiles[[x,y]],"
	"fieldTiles[[x,y]]}]; objects[{kind,footprint[[x,y]],height,blocksMovement}]; "
	"hubs[{position[x,y],promotedToOutpost}]; creatures[{kind,tile[x,y],wanderRadius,homeTownId}]; "
	"bridges[{townA,townB,span,reason,year,description,deck[[x,y]]}].";

// Telltale substring of the history description a crossroads outpost is founded with (world_gen.cpp
// PromoteCrossroadsOutposts). Lets the log flag which towns — and which hub nodes — became outposts.
constexpr std::string_view kOutpostMarker = "crossroads outpost";

json Vec2(const glm::ivec2 v) { return json::array({v.x, v.y}); }

json Tiles(const std::vector<glm::ivec2>& tiles) {
	json arr = json::array();
	for (const glm::ivec2 t : tiles) {
		arr.push_back(Vec2(t));
	}
	return arr;
}

const char* HistoryTypeName(const HistoryEventType t) {
	switch (t) {
	case HistoryEventType::TownFounded: return "TownFounded";
	case HistoryEventType::RoadBuilt: return "RoadBuilt";
	case HistoryEventType::RoadRemoved: return "RoadRemoved";
	case HistoryEventType::TownCollapsed: return "TownCollapsed";
	case HistoryEventType::BridgeBuilt: return "BridgeBuilt";
	}
	return "Unknown";
}

const char* TownStatusName(const TownStatus s) { return s == TownStatus::Living ? "Living" : "Ruined"; }

const char* BridgeReasonName(const BridgeReason r) {
	return r == BridgeReason::HistoricalInvestment ? "HistoricalInvestment" : "RoutingFallback";
}

json BuildLog(const WorldData& world) {
	json log;
	log["schemaVersion"] = kSchemaVersion;
	log["schema"] = std::string{kSchemaDoc};
	log["seed"] = world.seed;

	json grid;
	grid["width"] = world.grid.width;
	grid["height"] = world.grid.height;
	log["grid"] = std::move(grid);

	log["params"] = WorldGenParamsToJson(world.params);

	// A crossroads outpost is a hub promoted to a young town; its founding event carries a telltale
	// description. Collecting those town ids lets the log flag which towns, and which hub nodes, are
	// outposts without the generation layer having to store an extra field.
	std::set<int> outpostTownIds;
	for (const HistoryEvent& ev : world.history) {
		if (ev.type == HistoryEventType::TownFounded
			&& ev.description.find(kOutpostMarker) != std::string::npos) {
			outpostTownIds.insert(ev.a);
		}
	}

	json history = json::array();
	for (const HistoryEvent& ev : world.history) {
		json entry;
		entry["year"] = ev.year;
		entry["type"] = HistoryTypeName(ev.type);
		entry["a"] = ev.a;
		entry["b"] = ev.b;
		entry["description"] = ev.description;
		history.push_back(std::move(entry));
	}
	log["history"] = std::move(history);

	json towns = json::array();
	std::set<std::pair<int, int>> outpostLocations;
	for (const TownSite& t : world.towns) {
		const bool isOutpost = outpostTownIds.contains(t.id);
		if (isOutpost) {
			outpostLocations.insert({t.location.x, t.location.y});
		}
		json entry;
		entry["id"] = t.id;
		entry["name"] = t.name;
		entry["location"] = Vec2(t.location);
		entry["size"] = t.size;
		entry["status"] = TownStatusName(t.status);
		entry["foundingYear"] = t.foundingYear;
		entry["collapseYear"] = t.collapseYear;
		entry["isOutpost"] = isOutpost;
		towns.push_back(std::move(entry));
	}
	log["towns"] = std::move(towns);

	json layouts = json::array();
	for (const TownLayout& layout : world.townLayouts) {
		json buildings = json::array();
		for (const TownBuilding& b : layout.buildings) {
			json be;
			be["kind"] = b.kind;
			be["villagerArchetype"] = b.villagerArchetype;
			be["footprint"] = Tiles(b.footprint);
			be["door"] = Vec2(b.door);
			be["ruined"] = b.ruined;
			buildings.push_back(std::move(be));
		}
		json entry;
		entry["townId"] = layout.townId;
		entry["ruined"] = layout.ruined;
		entry["landmarkKind"] = layout.landmarkKind;
		entry["landmark"] = Vec2(layout.landmark);
		entry["buildings"] = std::move(buildings);
		entry["floorTiles"] = Tiles(layout.floorTiles);
		entry["fieldTiles"] = Tiles(layout.fieldTiles);
		layouts.push_back(std::move(entry));
	}
	log["townLayouts"] = std::move(layouts);

	// Object scatter + hub nodes in one pass. Hub signposts (kHubMarkerKind) ARE world-objects, so
	// they appear in both lists: the full scatter and, distilled, the hub-node list with whether the
	// hub was promoted to an outpost (an outpost town sits on the promoted hub's tile).
	json objects = json::array();
	json hubs = json::array();
	for (const WorldObject& obj : world.objects) {
		json entry;
		entry["kind"] = obj.kind;
		entry["footprint"] = Tiles(obj.footprint);
		entry["height"] = obj.height;
		entry["blocksMovement"] = obj.blocksMovement;
		objects.push_back(std::move(entry));

		if (const std::string_view kind = obj.kind; kind == kHubMarkerKind && !obj.footprint.empty()) {
			const glm::ivec2 pos = obj.footprint.front();
			json hub;
			hub["position"] = Vec2(pos);
			hub["promotedToOutpost"] = outpostLocations.contains({pos.x, pos.y});
			hubs.push_back(std::move(hub));
		}
	}
	log["objects"] = std::move(objects);
	log["hubs"] = std::move(hubs);

	json creatures = json::array();
	for (const CreatureSpawn& c : world.creatures) {
		json entry;
		entry["kind"] = c.kind;
		entry["tile"] = Vec2(c.tile);
		entry["wanderRadius"] = c.wanderRadius;
		entry["homeTownId"] = c.homeTownId;
		creatures.push_back(std::move(entry));
	}
	log["creatures"] = std::move(creatures);

	json bridges = json::array();
	for (const Bridge& br : world.bridges) {
		json entry;
		entry["townA"] = br.townA;
		entry["townB"] = br.townB;
		entry["span"] = br.Span();
		entry["reason"] = BridgeReasonName(br.reason);
		entry["year"] = br.year;
		entry["description"] = br.description;
		entry["deck"] = Tiles(br.deck);
		bridges.push_back(std::move(entry));
	}
	log["bridges"] = std::move(bridges);

	return log;
}

} // namespace

std::string FormatGenerationLog(const WorldData& world) { return BuildLog(world).dump(2) + "\n"; }

bool WriteGenerationLog(const WorldData& world, const std::string& path) {
	std::ofstream file(path, std::ios::binary);
	if (!file) {
		return false;
	}
	file << BuildLog(world).dump(2) << '\n';
	return static_cast<bool>(file);
}

} // namespace game::farm
