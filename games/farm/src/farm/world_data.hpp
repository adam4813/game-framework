#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "engine/engine.hpp"

// Pure C++ world-generation data types for the farm prototype. No ECS or Raylib dependency —
// WorldGen::Generate consumes WorldGenParams + a seed and returns a fully populated WorldData.
// Everything here is a plain value type so a world can be generated, compared, saved, and
// previewed without a running game world.
namespace game::farm {

// Per-tile terrain classification. Water bodies refine Ocean/Lake below in WaterBody.
enum class Biome : uint8_t {
	Ocean,
	Lake,
	Beach,
	Plains,
	Forest,
	Hills,
	Mountain,
	River,
	Count_, // sentinel — do not use as a value
};

// A connected body of water. Oceans touch the map border; interior water is a Lake (large) or
// Pond (small). Water bodies are the nodes of the waterway network — rivers are the edges.
enum class WaterBodyType : uint8_t {
	Ocean,
	Lake,
	Pond,
};

enum class TownStatus : uint8_t {
	Living,
	Ruined,
};

enum class HistoryEventType : uint8_t {
	TownFounded,
	RoadBuilt,
	RoadRemoved,
	TownCollapsed,
	BridgeBuilt, // a prosperous town/network invests in a bridge to reach an isolated place (ticket 08)
};

// Why a bridge exists. A routing fallback is a last resort when the land router finds no path for a
// wanted connection; a historical investment is a deliberate, prosperity-gated act recorded in the
// timeline. Ticket 11 logs each bridge with its reason.
enum class BridgeReason : uint8_t {
	RoutingFallback,
	HistoricalInvestment,
};

// Kind of a generation-timeline snapshot, so the preview scene can render the right overlay.
enum class SnapshotKind : uint8_t {
	Landmass,
	WaterBodies,
	Rivers,
	TownFounded,
	RoadBuilt,
	RoadRemoved,
	TownCollapsed,
	BridgeBuilt,
};

// Row-major elevation + biome grid. Elevation is normalized to [0, 1].
struct Grid {
	int width{0};
	int height{0};
	std::vector<float> elevation; // size width*height, row-major
	std::vector<Biome> biome;     // size width*height, row-major

	[[nodiscard]] bool InBounds(const int x, const int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
	[[nodiscard]] int Index(const int x, const int y) const { return y * width + x; }
	[[nodiscard]] float ElevationAt(const int x, const int y) const { return elevation[Index(x, y)]; }
	[[nodiscard]] Biome BiomeAt(const int x, const int y) const { return biome[Index(x, y)]; }

	friend bool operator==(const Grid&, const Grid&) = default;
};

struct WaterBody {
	int id{0};
	WaterBodyType type{WaterBodyType::Lake};
	glm::ivec2 centroid{0, 0};
	int tileCount{0};
	std::string name; // assigned to notable bodies only; empty for tiny specks

	friend bool operator==(const WaterBody&, const WaterBody&) = default;
};

// A traced waterway from a source to a destination water body (the edge between two nodes).
struct River {
	int id{0};
	int fromBodyId{0}; // source water body (0 if traced from a highland spring)
	int toBodyId{0};   // destination water body
	std::vector<glm::ivec2> path;

	friend bool operator==(const River&, const River&) = default;
};

struct TownSite {
	int id{0};
	std::string name;
	glm::ivec2 location{0, 0};
	int size{0}; // final population
	TownStatus status{TownStatus::Living};
	int foundingYear{0};
	int collapseYear{0}; // meaningful only when status == Ruined

	friend bool operator==(const TownSite&, const TownSite&) = default;
};

// A road between two living towns, weighted by distance and the towns' trade value.
struct RoadSegment {
	int townA{0};
	int townB{0};
	float weight{0.0f};
	std::vector<glm::ivec2> path;

	friend bool operator==(const RoadSegment&, const RoadSegment&) = default;
};

struct RoadGraph {
	std::vector<RoadSegment> segments;

	friend bool operator==(const RoadGraph&, const RoadGraph&) = default;
};

// A walkable crossing over otherwise-impassable water. The deck tiles are water tiles the bridge
// makes passable (the tilemap builder clears their non-walkable state along the span). Bridges are
// rare, deliberate structures: either a last-resort routing fallback when the land router finds no
// path for a wanted connection, or a prosperity-gated historical investment recorded in the
// timeline. The span (deck tile count) never exceeds WorldGenParams::maxBridgeSpan.
struct Bridge {
	int townA{0}; // the wanted connection's endpoint towns (min-first), 0 if unknown
	int townB{0};
	std::vector<glm::ivec2> deck; // water tiles the deck covers; its size is the crossing span in tiles
	BridgeReason reason{BridgeReason::RoutingFallback};
	int year{0};             // construction year (meaningful for a historical investment), else 0
	std::string description; // human-readable summary for the generation log (ticket 11)

	[[nodiscard]] int Span() const { return static_cast<int>(deck.size()); }

	friend bool operator==(const Bridge&, const Bridge&) = default;
};

struct HistoryEvent {
	int year{0};
	HistoryEventType type{HistoryEventType::TownFounded};
	int a{0}; // primary town id
	int b{0}; // secondary town id (road endpoint), else 0
	std::string description;

	friend bool operator==(const HistoryEvent&, const HistoryEvent&) = default;
};

// One cumulative generation step. Carries only the additions/removals at this step; the preview
// reconstructs the visible world at step N by applying the deltas of steps 0..N in order. Roads are
// identified by their (townA, townB) id pair so they can be built and later removed over time.
struct WorldSnapshot {
	SnapshotKind kind{SnapshotKind::Landmass};
	std::string label;
	std::vector<int> addedWaterBodyIds;
	std::vector<int> addedRiverIds;
	std::vector<int> addedTownIds;
	std::vector<std::pair<int, int>> addedRoads;   // (townA, townB) connections formed this step
	std::vector<std::pair<int, int>> removedRoads; // (townA, townB) connections lost this step

	friend bool operator==(const WorldSnapshot&, const WorldSnapshot&) = default;
};

struct GenerationTimeline {
	std::vector<WorldSnapshot> steps;

	friend bool operator==(const GenerationTimeline&, const GenerationTimeline&) = default;
};

// Tunable generation inputs. Defaults produce a coherent island world; the preview scene
// (ticket 01b) lets a developer vary the seed and re-generate.
struct WorldGenParams {
	int width{160};
	int height{160};
	engine::noise::FractalNoiseParams fractal{.octaves = 6, .persistence = 0.5f, .lacunarity = 2.0f};
	float noiseScale{0.025f}; // sample step per tile
	float seaLevel{0.34f};    // elevation <= this is water
	float beachLevel{0.38f};
	float plainsLevel{0.52f};
	float forestLevel{0.68f};
	float hillsLevel{0.82f};  // above this is Mountain
	float edgeFalloff{0.55f}; // radial falloff strength biasing map edges toward ocean
	int pondMaxTiles{40};     // interior water bodies up to this size are Ponds
	int lakeMaxTiles{400};    // up to this size are Lakes; larger interior is treated as Lake too
	int riverCount{4};        // rivers to attempt to trace
	int townCount{9};         // candidate town sites to place
	int minTownSpacing{14};   // minimum tile distance between town sites
	int coastalRadius{6};     // a town is "coastal" if ocean lies within this many tiles
	int historyEras{15};      // population-cycle iterations (simulated from deep history to year 0)
	int explorationRange{48}; // starting-village ruin proximity (tiles)

	// Town layout (ticket 04) — all proportional so a town stays ≤ 3% of the world at any scale.
	float buildingsPerCapita{0.08f}; // buildings per unit of population (clamped below)
	int minBuildingsPerTown{2};      // floor: a minimum-population living town still has a hamlet
	int maxBuildingsPerTown{12};     // cap: keeps the largest town compact and distinct
	int townPlotGap{1};              // walkable street gap (tiles) between adjacent building plots
	int townPlazaRadius{1};          // radius (tiles) of the walkable plaza around the landmark

	// ── Forest tree scatter (ticket 05) ──
	// Densities are PROPORTIONS (placement probability per tile) so they hold at any world size. A
	// secondary noise density field produces dense cores, sparse edges, and sub-threshold clearings.
	float forestTreeDensity{0.55f};           // peak tree probability on regular forest (Biome::Forest)
	float deepForestTreeDensity{0.85f};       // peak tree probability on deep forest (Biome::Hills) — denser
	float forestClearingThreshold{0.32f};     // density-field value below which a forest tile is a clearing
	float deepForestClearingThreshold{0.12f}; // lower threshold → deep forest has fewer/smaller clearings
	float forestCoreThreshold{0.58f};         // density-field value at/above which density reaches its peak
	float treeDensityNoiseScale{0.13f};       // secondary-noise sample step per tile (core/clearing size)

	// Road routing (ticket 06) — least-cost (A*) tile router cost model. Per-tile entry weights the
	// router minimises the sum of, so cheap biomes attract roads and costly ones repel them. A
	// NEGATIVE biome cost marks an impassable tile: no road crosses it without a bridge (ticket 08),
	// so a wanted connection separated only by blocked terrain simply yields no segment. Indexed by
	// Biome (Ocean, Lake, Beach, Plains, Forest, Hills, Mountain, River).
	std::array<float, static_cast<std::size_t>(Biome::Count_)> roadBiomeCost{
		-1.0f, // Ocean    — deep water, blocked
		-1.0f, // Lake     — deep water, blocked
		1.5f,  // Beach
		1.0f,  // Plains   — cheapest open ground
		4.0f,  // Forest   — costlier; the road clears a corridor through it
		3.0f,  // Hills
		-1.0f, // Mountain — blocked
		6.0f,  // River    — fordable but expensive (ticket 08 bridges will discount this)
	};
	float roadExistingDiscount{0.2f}; // multiplier (<1) on a tile already carrying a road, so new routes merge onto trunks
	float roadTownAvoidCost{50.0f};   // added to a town's walkable floor/field tiles so roads skirt the edge, not the interior
	float roadRedundancyDetour{1.6f}; // skip a wanted link if the realised network already joins the towns within this × direct distance

	// Road hubs + crossroads outposts (ticket 07). Hubs are a sparse set of geographic funnel points
	// (river fords/confluences, mountain passes, isthmuses) central to multiple towns. Each hub and
	// each town contributes a SOFT multiplicative discount to the router's per-tile cost field (a pull,
	// never a forced waypoint), so nearby roads tend to converge. After routing, a bounded number of
	// high-convergence hubs with no living town nearby are promoted into young living "crossroads
	// outpost" towns.
	int hubCount{3};               // max hub nodes placed at the strongest funnel points
	int hubMinSpacing{24};         // minimum tile distance between two hubs (tile-space, like minTownSpacing)
	float hubRouteDiscount{0.55f}; // cost multiplier (<1) at a hub centre; eases to 1.0 over hubDiscountRadius
	int hubDiscountRadius{12};     // tiles over which a hub's soft discount fades back to no change
	float townAttractorDiscount{0.40f};  // deepest cost discount a maximal (large, well-connected) town contributes
	int townAttractorRadius{16};         // tiles over which a town's attractor fades back to no change
	float townAttractorConnectivity{0.30f}; // per-incident-road boost to a town's attractor strength ("all roads lead to Rome")
	int hubMarkerRadius{2};        // a realised road "passes through" a hub when a path tile lies within this
	int outpostConvergenceRadius{10}; // radius of the junction neighbourhood over which outpost road convergence is counted
	int outpostMinConvergence{3};  // distinct realised roads meeting in that neighbourhood for it to count as a crossroads
	int outpostMinTownDistance{18}; // a hub this close to a living town is NOT promoted (it already has a town)
	int maxCrossroadsOutposts{2};  // cap on promoted crossroads-outpost towns per world
	int outpostPopulation{20};     // starting population of a young crossroads outpost

	// Bridges (ticket 08) — a bridge is the only way a route crosses water. A wanted connection whose
	// towns sit on different landmasses (no land path) triggers a routing-fallback bridge across the
	// narrowest water crossing; a prosperity-gated history event can also invest in one. A crossing
	// wider than this many deck (water) tiles is left UNBUILT and the place stays isolated, so no
	// implausible ocean-spanning bridge is ever placed.
	int maxBridgeSpan{8};

	// Wandering creatures (ticket 10) — sparse ambient livestock near towns + wildlife in the open.
	int creatureTownRadius{8};   // tiles around a living town centre to scatter livestock
	int minLivestockPerTown{1};  // minimum livestock placed near a living town
	int maxLivestockPerTown{4};  // maximum livestock placed near a living town
	int creatureAreaSize{64};    // wilderness sampling-cell size in tiles (one "forest area")
	int minWildlifePerArea{0};   // minimum wildlife placed per wilderness cell
	int maxWildlifePerArea{3};   // maximum wildlife placed per wilderness cell

	// FractalNoiseParams (engine type) has no operator==, so compare member-by-member.
	friend bool operator==(const WorldGenParams& a, const WorldGenParams& b) {
		return a.width == b.width
			   && a.height == b.height
			   && a.fractal.octaves == b.fractal.octaves
			   && a.fractal.persistence == b.fractal.persistence
			   && a.fractal.lacunarity == b.fractal.lacunarity
			   && a.noiseScale == b.noiseScale
			   && a.seaLevel == b.seaLevel
			   && a.beachLevel == b.beachLevel
			   && a.plainsLevel == b.plainsLevel
			   && a.forestLevel == b.forestLevel
			   && a.hillsLevel == b.hillsLevel
			   && a.edgeFalloff == b.edgeFalloff
			   && a.pondMaxTiles == b.pondMaxTiles
			   && a.lakeMaxTiles == b.lakeMaxTiles
			   && a.riverCount == b.riverCount
			   && a.townCount == b.townCount
			   && a.minTownSpacing == b.minTownSpacing
			   && a.coastalRadius == b.coastalRadius
			   && a.historyEras == b.historyEras
			   && a.explorationRange == b.explorationRange
			   && a.buildingsPerCapita == b.buildingsPerCapita
			   && a.minBuildingsPerTown == b.minBuildingsPerTown
			   && a.maxBuildingsPerTown == b.maxBuildingsPerTown
			   && a.townPlotGap == b.townPlotGap
			   && a.townPlazaRadius == b.townPlazaRadius
			   && a.forestTreeDensity == b.forestTreeDensity
			   && a.deepForestTreeDensity == b.deepForestTreeDensity
			   && a.forestClearingThreshold == b.forestClearingThreshold
			   && a.deepForestClearingThreshold == b.deepForestClearingThreshold
			   && a.forestCoreThreshold == b.forestCoreThreshold
			   && a.treeDensityNoiseScale == b.treeDensityNoiseScale
			   && a.roadBiomeCost == b.roadBiomeCost
			   && a.roadExistingDiscount == b.roadExistingDiscount
			   && a.roadTownAvoidCost == b.roadTownAvoidCost
			   && a.roadRedundancyDetour == b.roadRedundancyDetour
			   && a.hubCount == b.hubCount
			   && a.hubMinSpacing == b.hubMinSpacing
			   && a.hubRouteDiscount == b.hubRouteDiscount
			   && a.hubDiscountRadius == b.hubDiscountRadius
			   && a.townAttractorDiscount == b.townAttractorDiscount
			   && a.townAttractorRadius == b.townAttractorRadius
			   && a.townAttractorConnectivity == b.townAttractorConnectivity
			   && a.hubMarkerRadius == b.hubMarkerRadius
			   && a.outpostConvergenceRadius == b.outpostConvergenceRadius
			   && a.outpostMinConvergence == b.outpostMinConvergence
			   && a.outpostMinTownDistance == b.outpostMinTownDistance
			   && a.maxCrossroadsOutposts == b.maxCrossroadsOutposts
			   && a.outpostPopulation == b.outpostPopulation
			   && a.maxBridgeSpan == b.maxBridgeSpan
			   && a.creatureTownRadius == b.creatureTownRadius
			   && a.minLivestockPerTown == b.minLivestockPerTown
			   && a.maxLivestockPerTown == b.maxLivestockPerTown
			   && a.creatureAreaSize == b.creatureAreaSize
			   && a.minWildlifePerArea == b.minWildlifePerArea
			   && a.maxWildlifePerArea == b.maxWildlifePerArea;
	}
};

// A placed thing that sits on top of the flat terrain tile: buildings, trees, props, bridge decks,
// hub markers, decorative fields, creatures. The terrain tile stays flat ground; the world-object
// supplies visual height and optional collision. This is the single shared representation every
// later ticket (04 buildings, 05 trees, 10 creatures) populates — keeping generation one pure,
// unit-testable function. `kind` is an open-taxonomy string key resolved against the data-driven
// ObjectKindRegistry (games/farm/assets/data/object_kinds.json); it is never a closed enum.
struct WorldObject {
	std::vector<glm::ivec2> footprint; // grid tile coordinate(s) the object occupies (x = column, y = row)
	std::string kind;                  // registry key, e.g. "tree_oak", "house"
	float height{0.0f};                // visual height in world units (0 = flush with the ground tile)
	bool blocksMovement{false};        // true => every footprint tile is non-walkable regardless of terrain

	friend bool operator==(const WorldObject&, const WorldObject&) = default;
};

// One building within a generated town layout (ticket 04). The walls/roof are emitted as a blocking
// WorldObject (ticket 03 renders them, ticket 02's walkability layer blocks them); the door and the
// interior are walkable terrain floor tiles so the player can enter. `kind` is the building-catalog
// key ("house", "shop", "barn", "hall"); `villagerArchetype` is the NPC role a later ticket (09)
// binds to this building ("shopkeeper", "elder", … or empty for none).
struct TownBuilding {
	std::string kind;                  // building-catalog key, e.g. "house"
	std::string villagerArchetype;     // NPC role to bind here (ticket 09); empty = none
	std::vector<glm::ivec2> footprint; // blocking wall/roof tiles (identical to the WorldObject's)
	glm::ivec2 door{0, 0};             // the single walkable door tile in the wall (terrain)
	bool ruined{false};                // true in a ruined town => rubble variant
	bool isHome{false};                // residential building — villagers are homed here (ticket 09)

	friend bool operator==(const TownBuilding&, const TownBuilding&) = default;
};

// The generated spatial layout of one town (ticket 04): a central landmark, population-scaled
// buildings, and the walkable floor/street/field terrain tiles the tilemap builder paints. This
// replaces the old solid grey square and is the data ticket 09 consumes to place NPCs in buildings.
struct TownLayout {
	int townId{0};                      // TownSite::id this layout belongs to
	bool ruined{false};                 // mirrors TownSite::status == Ruined
	std::string landmarkKind;           // landmark object kind ("well" living / "rubble" ruined)
	glm::ivec2 landmark{0, 0};          // landmark object tile (near the town centre)
	std::vector<TownBuilding> buildings;
	std::vector<glm::ivec2> floorTiles; // walkable floor/plot tiles -> town ground material (ruins if ruined)
	std::vector<glm::ivec2> pathTiles;  // walkable street tiles (2-wide spine + 1-wide door branches) -> path
	std::vector<glm::ivec2> fieldTiles; // decorative tilled_soil tiles near farm buildings

	friend bool operator==(const TownLayout&, const TownLayout&) = default;
};

// A placed ambient creature (ticket 10): wandering livestock near towns and wildlife in the open.
// Creatures are non-interactive — this record only carries where it spawns, which catalog kind it
// is (resolved against the CreatureCatalog for its sprite/height), how far it ambles, and the living
// town it belongs to (0 for wilderness wildlife). Generation guarantees `tile` is a walkable,
// unblocked grid tile. Ticket 11 logs these records as part of the generation log.
struct CreatureSpawn {
	glm::ivec2 tile{0, 0}; // walkable spawn grid tile (x = column, y = row)
	std::string kind;      // creature-catalog key, e.g. "cow", "deer"
	int wanderRadius{3};   // tiles the creature ambles from its spawn tile at runtime
	int homeTownId{0};     // living town id for livestock; 0 for wilderness wildlife

	friend bool operator==(const CreatureSpawn&, const CreatureSpawn&) = default;
};

// The full generated world.
struct WorldData {
	WorldGenParams params;
	int seed{0};
	Grid grid;
	std::vector<WaterBody> waterBodies;
	std::vector<River> rivers;
	std::vector<TownSite> towns;
	RoadGraph roads;
	std::vector<HistoryEvent> history;
	GenerationTimeline timeline;
	std::vector<WorldObject> objects; // world-object layer (ticket 02); empty until a stage places objects
	std::vector<TownLayout> townLayouts; // per-town village layouts (ticket 04); one per TownSite
	std::vector<Bridge> bridges;         // walkable water crossings (ticket 08); empty when no bridge is needed
	std::vector<CreatureSpawn> creatures; // ambient wandering creatures (ticket 10)

	[[nodiscard]] const TownSite* FindTown(int id) const;

	friend bool operator==(const WorldData&, const WorldData&) = default;
};

} // namespace game::farm
