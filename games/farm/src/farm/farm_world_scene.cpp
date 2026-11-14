#include "farm_world_scene.hpp"

#include <algorithm>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "building_catalog.hpp"
#include "creature_catalog.hpp"
#include "creature_system.hpp"
#include "crop_growth_system.hpp"
#include "engine/engine.hpp"
#include "farm_components.hpp"
#include "farm_map_rasterizer.hpp"
#include "farm_save.hpp"
#include "farm_tilemap_builder.hpp"
#include "farm_world_gen.hpp"
#include "game_time.hpp"
#include "generation_log.hpp"
#include "object_kind_registry.hpp"
#include "occluder_fade.hpp"
#include "player_inventory.hpp"
#include "starting_village.hpp"
#include "tree_species_catalog.hpp"
#include "upright_object_mesh_builder.hpp"
#include "villager_components.hpp"
#include "villager_system.hpp"
#include "world_interaction_system.hpp"

using namespace engine;
using tilemap::GridMover;
using tilemap::PlayerControlled;
using tilemap::RegisterGridMovementSystems;

namespace game::farm {

namespace {

// Tile-world scale: world-unit distance per tile (tile_size_px / pixels_per_world_unit = 16/32).
constexpr float kStep = 0.5F;
// Upright object quads are one tile wide.
constexpr float kObjectQuadWidthTiles = 1.0F;

// Hotbar layout constants (in screen pixels).
constexpr float kSlotSize = 48.0F;
constexpr float kSlotGap = 4.0F;
constexpr float kHotbarPadBot = 12.0F;
constexpr int kHotbarSlots = 8; // show first 8 of the 16 inventory slots on-screen

constexpr core::Rgba kSlotBg = {.r = 30, .g = 30, .b = 35, .a = 200};
constexpr core::Rgba kSlotBorder = {.r = 80, .g = 80, .b = 90, .a = 255};
constexpr core::Rgba kEquipBorder = {.r = 255, .g = 215, .b = 0, .a = 255};
constexpr core::Rgba kTextColor = {.r = 220, .g = 220, .b = 220, .a = 255};
constexpr core::Rgba kTimeColor = {.r = 220, .g = 220, .b = 240, .a = 255};

// Map / minimap layout constants.
constexpr int kBigMapPxPerTile = 1; // 1px per tile — keeps the full-world texture manageable
constexpr int kHudPx = 256;         // HUD minimap panel size in pixels (slightly larger than before)
constexpr int kHudRadiusTiles = 60; // half the tile span shown in the HUD minimap
constexpr float kDotSizeHud = 5.0F; // pin dot diameter on the HUD minimap
constexpr float kDotSizeBig = 5.0F; // pin dot diameter on the full map (smaller for the denser 1px/tile map)

// Colours for distinct pin types.
constexpr core::Rgba kPinPlayer = {.r = 255, .g = 70, .b = 70, .a = 255};
constexpr core::Rgba kPinLiving = {.r = 255, .g = 240, .b = 120, .a = 255};
constexpr core::Rgba kPinRuined = {.r = 130, .g = 110, .b = 95, .a = 255};

// Resolve season/phase to a display string.
std::string_view SeasonName(const Season s) {
	switch (s) {
	case Season::Spring: return "Spring";
	case Season::Summer: return "Summer";
	case Season::Autumn: return "Autumn";
	case Season::Winter: return "Winter";
	}
	return "?";
}

std::string_view PhaseName(const DayPhase p) {
	switch (p) {
	case DayPhase::Dawn: return "Dawn";
	case DayPhase::Day: return "Day";
	case DayPhase::Dusk: return "Dusk";
	case DayPhase::Night: return "Night";
	}
	return "?";
}

} // namespace

void FarmWorldScene::InitializePipeline(const flecs::world& world) {
	pipeline_ = ecs::FramePipeline(world).without<scene::MenuScene>().build();
	pausedPipeline_ = ecs::FramePipeline(world).without<scene::MenuScene>().without<ecs::Pausable>().build();
	fixedStepPipeline_ = ecs::FixedStepPipeline(world).without<scene::MenuScene>().build();
}

void FarmWorldScene::Load(flecs::world& world) {
	sceneRoot_ = world.entity("FarmWorldSceneRoot");
	world.set<render::AmbientLight>({.color = {.r = 180, .g = 180, .b = 200, .a = 255}, .intensity = 0.5F});

	// Clean up any stale farm tile entities left from a previous session.
	{
		std::vector<flecs::entity> stale;
		world.query_builder<TilledSoil>().build().each([&stale](flecs::entity e, TilledSoil&) { stale.push_back(e); });
		for (auto e : stale) e.destruct();
	}

	// Resolve the world source: restore a slot (Continue/Load), adopt the world the preview handed
	// over via CurrentWorld (New Game), or generate fresh (dev/test direct launch). See games/farm/docs/adr/0001.
	auto& session = world.get_mut<FarmSession>();
	const bool loadRequested = session.load_requested && FarmSlotOccupied(world, session.slot);
	const int boundSlot = session.slot;
	std::string savedJson;

	if (loadRequested) {
		savedJson = ReadFarmSlotRaw(world, boundSlot);
		int loadSeed = session.seed;
		WorldGenParams params = CanonicalFarmParams();
		const auto doc = nlohmann::json::parse(savedJson, nullptr, /*exceptions=*/false);
		if (!doc.is_discarded() && doc.contains("farm") && doc.at("farm").is_object()) {
			const auto& farm = doc.at("farm");
			loadSeed = farm.value("seed", loadSeed);
			if (farm.contains("params")) params = WorldGenParamsFromJson(farm.at("params"));
		}
		seed_ = loadSeed != 0 ? loadSeed : static_cast<int>(std::random_device{}());
		worldData_ = GenerateFarmWorld(world, seed_, params);
	}
	else if (const auto* cw = world.try_get<CurrentWorld>(); cw && cw->world.grid.width > 0) {
		// New game: gameplay is simply another lens on the world the preview already generated.
		worldData_ = cw->world;
		seed_ = worldData_.seed;
	}
	else {
		// Dev/test direct launch with no preview and no save: generate a fresh world.
		if (seed_ == 0) seed_ = session.seed != 0 ? session.seed : static_cast<int>(std::random_device{}());
		worldData_ = GenerateFarmWorld(world, seed_, CanonicalFarmParams());
	}

	seed_ = worldData_.seed != 0 ? worldData_.seed : seed_;
	session.seed = seed_;
	// Publish the authoritative world state so saves (seed + params) and later lenses read it. This
	// may add the singleton (moving the world entity), so do not touch `session` afterwards.
	world.set<CurrentWorld>({worldData_});

	// Creature catalog for sprite attachment below (generation loads its own copy internally).
	const CreatureCatalog creatureCatalog = LoadFarmCreatureCatalog(world);

	engine::ecs::Log(world).info(
		"[FarmWorldScene] {} world seed={} towns={}",
		loadRequested ? "Loaded" : "Generated",
		seed_,
		worldData_.towns.size()
	);

	// Serialized generation log (ticket 11): emitted here at the scene boundary — beside the other
	// per-run exports — so WorldGen::Generate stays a pure value→value function. Keyed by seed and
	// overwritten on each regeneration with that seed; distinct from the shareable map export.
	{
		const std::string logPath = "generation_log_seed" + std::to_string(seed_) + ".json";
		if (WriteGenerationLog(worldData_, logPath)) {
			engine::ecs::Log(world).info("[FarmWorldScene] Wrote generation log to {}", logPath);
		}
		else {
			engine::ecs::Log(world).warn("[FarmWorldScene] Failed to write generation log to {}", logPath);
		}
	}

	// Synchronous load: level loads + tilemap generation + player placement must all complete
	// before RegisterGridMovementSystems can find the tilemap and player entities.
	// Load registries BEFORE SetupTilemap so the OnSet<WorldObjectLayer> observer (fired during
	// SetupTilemap) can resolve kind→sprite correctly from ObjectKindRegistry. Bridge decks use
	// kind="bridge" with sprite="path"; without the registry present the observer falls back to the
	// full-atlas UV quad (visual garbage).
	LoadCropRegistry(world);
	LoadObjectKindRegistry(world);
	SetupTilemap(world);
	SetupPlayer(world);
	BuildTownPins(world);

	// Spawn villagers across every living town.
	{
		engine::profiling::Zone _zone{"FarmWorldScene/EntitySpawn"};
		{
			const auto village = SelectStartingVillage(worldData_);
			const int startVillageTownId = village ? village->id : -1;
			SpawnVillagers(world, sceneRoot_, worldData_, startVillageTownId);
		}
		AttachVillagerSprites(world);

		// Ambient wandering creatures (ticket 10): pre-register component types BEFORE SpawnCreatures
		// so the explicit world.component<>() calls genuinely run before any entity accesses them
		// (matching the pre-registration invariant and the UprightObjectMeshBuilder::Register pattern).
		RegisterCreatureSystems(world, tilemapEntity_);
		SpawnCreatures(world, sceneRoot_, worldData_.creatures);
		AttachCreatureSprites(world, sceneRoot_, creatureCatalog);
	}

	BakeMapTextures(world);
	RegisterGridMovementSystems(world, sceneRoot_, tilemapEntity_);
	RegisterUprightMoverSystems(world, sceneRoot_);
	RegisterHudSystems(world);
	RegisterObjectStreamingSystem(world);
	RegisterCropGrowthSystem(world, sceneRoot_);
	RegisterWorldInteractionSystems(world, sceneRoot_, tilemapEntity_);
	RegisterVillagerSystems(world, sceneRoot_);
	BuildUI(world);

	if (loadRequested && !savedJson.empty()) {
		// Restore saved state: PlayerInventory, GameTime, FarmSession, farm grid tiles.
		save::LoadFromString(world, savedJson);

		// Sync the crop growth tracker so it doesn't skip-advance on the first frame.
		if (world.has<GameTime>() && world.has<CropGrowthTracker>()) {
			world.get_mut<CropGrowthTracker>().lastProcessedAbsoluteDay = world.get<GameTime>().absoluteDay;
		}

		// Reposition the player from the saved FarmSession.
		if (world.has<FarmSession>() && playerEntity_.is_valid()) {
			const auto& session = world.get<FarmSession>();
			if (session.playerX >= 0 && session.playerZ >= 0 && playerEntity_.has<tilemap::GridPosition>()) {
				auto& gp = playerEntity_.get_mut<tilemap::GridPosition>();
				gp.x = session.playerX;
				gp.z = session.playerZ;
				auto& tr = playerEntity_.get_mut<spatial::Transform>();
				tr.position.x = (static_cast<float>(session.playerX) + 0.5F) * kStep;
				tr.position.z = (static_cast<float>(session.playerZ) + 0.5F) * kStep;
				if (auto* mover = playerEntity_.try_get_mut<GridMover>()) {
					mover->origin = tr.position;
					mover->target = tr.position;
					mover->moving = false;
				}
			}
		}
		world.get_mut<FarmSession>().load_requested = false; // consumed
	}
	else if (boundSlot >= 0) {
		// New game: commit the freshly generated world to its bound slot so Continue/Load can see it.
		SaveFarmSlot(world, boundSlot);
	}
}

void FarmWorldScene::Unload(flecs::world& world) {
	// Update player position in FarmSession before persisting.
	if (playerEntity_.is_valid() && playerEntity_.has<tilemap::GridPosition>() && world.has<FarmSession>()) {
		const auto& gp = playerEntity_.get<tilemap::GridPosition>();
		auto& session = world.get_mut<FarmSession>();
		session.playerX = gp.x;
		session.playerZ = gp.z;
	}

	// Persist to the bound slot (no-op when Platform is unavailable or no slot is bound).
	if (world.has<FarmSession>() && world.get<FarmSession>().slot >= 0) {
		SaveFarmSlot(world, world.get<FarmSession>().slot);
	}

	if (sceneRoot_) {
		sceneRoot_.destruct();
		sceneRoot_ = {};
	}
	if (uiRoot_) {
		uiRoot_.destruct();
		uiRoot_ = {};
	}
	tilemapEntity_ = {};
	objectLayerEntity_ = {};
	occluderEntity_ = {};
	cameraEntity_ = {};
	playerEntity_ = {};
	visibleObjects_.clear();
	occluderIndices_.clear();
	lastObjectOriginX_ = std::numeric_limits<int>::min(); // rebuild the object meshes on the next Load
	lastObjectOriginZ_ = std::numeric_limits<int>::min();
	mapModal_ = {};
	minimapScroll_ = {};
	timeLabelEntity_ = {};
	trackedQuery_ = {};
	hotbarSlotPanels_ = {};
	hotbarItemLabels_ = {};
	hotbarCountLabels_ = {};
	indicators_.clear();
	seed_ = 0; // reset so the next new-game generates a fresh seed

	// Release GPU textures.
	if (const auto* ref = world.try_get<platform::PlatformRef>()) {
		if (auto* p = ref->ptr) {
			if (mapTexture_ >= 0) p->UnloadTexture(mapTexture_);
			if (hudTexture_ >= 0) p->UnloadTexture(hudTexture_);
		}
	}
	mapTexture_ = -1;
	hudTexture_ = -1;

	// Destroy orphaned farm tile entities (not children of sceneRoot_, so not auto-destroyed).
	{
		std::vector<flecs::entity> tiles;
		world.query_builder<TilledSoil>().build().each([&tiles](flecs::entity e, TilledSoil&) { tiles.push_back(e); });
		for (auto e : tiles) e.destruct();
	}

	world.remove<scene::Paused>();
}

void FarmWorldScene::Tick(const flecs::world& world, float) {
	// Pause while the world map modal is open.
	const bool mapOpen = mapModal_.is_valid() && mapModal_.has<ui::Modal>() && mapModal_.get<ui::Modal>().open;
	if (mapOpen && !world.has<scene::Paused>()) world.add<scene::Paused>();
	else if (!mapOpen && world.has<scene::Paused>()) world.remove<scene::Paused>();
}

void FarmWorldScene::HandleInput(const flecs::world& world, const input::InputState& input) {
	const bool mapOpen = mapModal_.is_valid() && mapModal_.has<ui::Modal>() && mapModal_.get<ui::Modal>().open;

	if (input.keys[input::KeyCode::M].pressed && !mapOpen) {
		// Open the map and centre on player.
		if (mapModal_.is_valid() && mapModal_.has<ui::Modal>()) {
			mapModal_.get_mut<ui::Modal>().open = true;
			CenterMapOnPlayer();
		}
		return;
	}

	if (input.keys[input::KeyCode::Escape].pressed) {
		if (mapOpen) {
			// Close map first — ESC does not also pause.
			mapModal_.get_mut<ui::Modal>().open = false;
		}
	}
}

// ── Private helpers ─────────────────────────────────────────────────────────────

void FarmWorldScene::SetupTilemap(flecs::world& world) {
	// Load registry + sprite atlas entities from the level JSON.
	level::LoadLevel(world, assets::ResolveAsset(world, "levels/farm_world_map.level.json"), sceneRoot_);

	const flecs::entity registry = sceneRoot_.lookup("BasicRegistry");
	const flecs::entity sheet = sceneRoot_.lookup("BasicSheet");
	if (!registry.is_valid() || !sheet.is_valid()) {
		engine::ecs::Log(world).error("[FarmWorldScene] farm_world_map level is missing BasicRegistry or BasicSheet");
		return;
	}
	const auto* tileset = registry.try_get<tilemap::TileSet>();
	if (!tileset) {
		engine::ecs::Log(world).error("[FarmWorldScene] BasicRegistry has no TileSet");
		return;
	}

	// Build the full tilemap from the generated WorldData and attach it to the tilemap entity.
	{
		engine::profiling::Zone _zone{"FarmWorldScene/TilemapBuild"};
		const TownStyleTable townStyles = LoadFarmTownStyles(world);
		tilemap::Tilemap map = BuildTilemapFromWorldData(worldData_, *tileset, townStyles);
		const float tileWorldSize = tilemap::TileWorldSize(map);

		tilemapEntity_ = world.entity("FarmWorldTilemap").child_of(sceneRoot_);
		tilemapEntity_.add<level::UseRegistry>(registry);
		tilemapEntity_.add<render::UseSpriteAtlas>(sheet);
		tilemapEntity_.set<tilemap::TilemapViewport>({}); // must be set BEFORE Tilemap for ring-buffer streaming
		tilemapEntity_.set<tilemap::Tilemap>(std::move(map));

		engine::ecs::Log(world).info(
			"[FarmWorldScene] Tilemap built: {}x{}",
			worldData_.grid.width,
			worldData_.grid.height
		);
	}

	// Upright world-object layer entity — geometry is rebuilt by FarmUprightObjectStreaming
	// each time the tilemap viewport scrolls (only visible tiles are on the GPU at any time).
	// The static bake of all worldData_.objects caused chunk 0 to contain distant objects that
	// are frustum-clipped, so player-nearby objects in later chunks never rendered.
	const flecs::entity objectRegistry = sceneRoot_.lookup("FarmObjectRegistry");
	const flecs::entity objectSheet = sceneRoot_.lookup("ObjectSheet");
	if (objectSheet.is_valid() && objectRegistry.is_valid()) {
		objectLayerEntity_ = world.entity("FarmWorldObjects").child_of(sceneRoot_);
		objectLayerEntity_.add<level::UseRegistry>(objectRegistry);
		objectLayerEntity_.add<render::UseSpriteAtlas>(objectSheet);
		// Transform at origin; mesh vertices use absolute world coordinates.
		objectLayerEntity_.set<spatial::Transform>(
			{.position = {0.0F, 0.0F, 0.0F}, .rotation = {0.0F, 0.0F, 0.0F}, .scale = {1.0F, 1.0F, 1.0F}}
		);

		// The objects standing between the camera and the player (occluder_fade.hpp), drawn
		// see-through by the engine's translucent pass. The streaming system moves them here from
		// the opaque layer above and drives this entity's mesh handle too.
		occluderEntity_ = world.entity("FarmWorldOccluders").child_of(sceneRoot_);
		occluderEntity_.add<render::UseSpriteAtlas>(objectSheet);
		occluderEntity_.add<render::Translucent>();
		occluderEntity_.set<spatial::Transform>(
			{.position = {0.0F, 0.0F, 0.0F}, .rotation = {0.0F, 0.0F, 0.0F}, .scale = {1.0F, 1.0F, 1.0F}}
		);
		engine::ecs::Log(world).info(
			"[FarmWorldScene] Upright object layer ready ({} world objects)",
			worldData_.objects.size()
		);
	}
	else {
		engine::ecs::Log(world).error(
			"[FarmWorldScene] farm_world_map level is missing {} {}; upright objects disabled",
			objectSheet.is_valid() ? "" : "ObjectSheet",
			objectRegistry.is_valid() ? "" : "FarmObjectRegistry"
		);
	}
}

void FarmWorldScene::SetupPlayer(flecs::world& world) {
	level::LoadLevel(world, assets::ResolveAsset(world, "levels/farm_world_player.level.json"), sceneRoot_);

	playerEntity_ = sceneRoot_.lookup("Player");
	if (!playerEntity_.is_valid()) {
		engine::ecs::Log(world).error("[FarmWorldScene] 'Player' entity not found after level load");
		return;
	}
	// The follow camera is the player's Camera child (farm_world_player.level.json).
	playerEntity_.children([this](const flecs::entity child) {
		if (child.has<render::Camera>()) cameraEntity_ = child;
	});

	// Determine spawn position from the starting village, with fallback to map centre.
	int spawnX = worldData_.grid.width / 2;
	int spawnZ = worldData_.grid.height / 2;
	if (const auto village = SelectStartingVillage(worldData_)) {
		spawnX = village->location.x;
		spawnZ = village->location.y;
		engine::ecs::Log(world)
			.info("[FarmWorldScene] Spawning at village '{}' ({},{})", village->name, spawnX, spawnZ);
	}
	else {
		engine::ecs::Log(world).warn("[FarmWorldScene] No starting village found; spawning at map centre");
	}

	auto& gp = playerEntity_.get_mut<tilemap::GridPosition>();
	gp.x = spawnX;
	gp.z = spawnZ;

	auto& tr = playerEntity_.get_mut<spatial::Transform>();
	tr.position.x = (static_cast<float>(spawnX) + 0.5F) * kStep;
	tr.position.z = (static_cast<float>(spawnZ) + 0.5F) * kStep;

	if (auto* mover = playerEntity_.try_get_mut<GridMover>()) {
		mover->origin = tr.position;
		mover->target = tr.position;
		mover->moving = false;
	}

	// Add a starting toolkit and FacingDirection so all interactions can be tested from first launch.
	playerEntity_.set<FacingDirection>({});
	playerEntity_.set<PlayerInventory>({});
	auto& inv = playerEntity_.get_mut<PlayerInventory>();
	AddItem(inv, "hoe", 1);
	AddItem(inv, "watering_can", 1);
	AddItem(inv, "scythe", 1);
	AddItem(inv, "seed_carrot", 5); // starting seeds for immediate planting

	// Mark the player as a tracked pin (updated in real time by the indicator system).
	playerEntity_.set<MinimapTracked>({.color = kPinPlayer});
}

void FarmWorldScene::AttachVillagerSprites(flecs::world& world) {
	// Give every villager a dynamic upright quad that reuses the GridMover/Transform pipeline so it
	// glides with the villager each frame. The quad shares the ObjectSheet upright material.
	const flecs::entity objectSheet = sceneRoot_.lookup("ObjectSheet");
	const flecs::entity objectRegistry = sceneRoot_.lookup("FarmObjectRegistry");
	if (!objectSheet.is_valid() || !objectRegistry.is_valid()) {
		return;
	}

	const auto* atlas = objectSheet.try_get<render::SpriteAtlas>();
	const float atlasW = atlas ? static_cast<float>(atlas->image_width_px) : 1.0F;
	const float atlasH = atlas ? static_cast<float>(atlas->image_height_px) : 1.0F;

	std::optional<core::Rect> villagerRect;
	if (const auto* tileset = objectRegistry.try_get<tilemap::TileSet>()) {
		if (const auto it = tileset->name_to_id.find("villager"); it != tileset->name_to_id.end()) {
			if (const auto* desc = tileset->GetTile(it->second)) villagerRect = desc->tex_coords;
		}
	}

	constexpr float kVillagerWidth = 0.4F;  // world units (~0.8 tile wide)
	constexpr float kVillagerHeight = 0.9F; // world units (~1.8 tiles tall)

	// Collect first, then mutate — adding components to the iterated entities would move their table.
	std::vector<flecs::entity> villagers;
	world.query_builder<const VillagerTag>().build().each([&villagers](flecs::entity e, const VillagerTag&) {
		villagers.push_back(e);
	});

	for (const flecs::entity e : villagers) {
		const auto* gp = e.try_get<tilemap::GridPosition>();
		const glm::vec3 pos{
			(static_cast<float>(gp ? gp->x : 0) + 0.5F) * kStep,
			0.0F,
			(static_cast<float>(gp ? gp->z : 0) + 0.5F) * kStep,
		};
		e.set<spatial::Transform>({.position = pos, .rotation = {0.0F, 0.0F, 0.0F}, .scale = {1.0F, 1.0F, 1.0F}});
		e.set<tilemap::GridMover>({.origin = pos, .target = pos, .moving = false});
		e.add<UprightMover>();
		e.add<render::UseSpriteAtlas>(objectSheet);
		e.set<render::FrustumCullRadius>({}); // cull when off-screen

		const int handle = UploadUprightQuadMesh(world, kVillagerWidth, kVillagerHeight, villagerRect, atlasW, atlasH);
		if (handle >= 0) {
			e.set<render::DynamicMesh>({handle});
		}
	}
}

void FarmWorldScene::RegisterObjectStreamingSystem(flecs::world& world) {
	// Keep the upright-object meshes in step with the view. When the tilemap viewport scrolls, the
	// visible object set is refreshed, so only objects near the window are on the GPU (a static
	// bake of every object frustum-clipped whole chunks). Every frame, the objects standing between
	// the camera and the player are found; when that set changes, they move from the opaque layer
	// into the translucent occluder mesh (occluder_fade.hpp) and both meshes are rebuilt.
	const auto sys = world.system("FarmUprightObjectStreaming").kind(flecs::OnUpdate).run([this](flecs::iter& it) {
		while (it.next()) {
			if (!tilemapEntity_.is_valid() || !objectLayerEntity_.is_valid() || !occluderEntity_.is_valid()) continue;

			const auto* vp = tilemapEntity_.try_get<tilemap::TilemapViewport>();
			const auto* tm = tilemapEntity_.try_get<tilemap::Tilemap>();
			if (!vp || vp->buffer_width <= 0 || !tm) continue;
			const float tileSize = tilemap::TileWorldSize(*tm);

			bool visibleChanged = false;
			if (vp->origin_tile_x != lastObjectOriginX_ || vp->origin_tile_z != lastObjectOriginZ_) {
				lastObjectOriginX_ = vp->origin_tile_x;
				lastObjectOriginZ_ = vp->origin_tile_z;
				RefreshVisibleObjects(*vp);
				visibleChanged = true;
			}

			std::vector<size_t> occluders = FindPlayerOccluders(tileSize);
			if (!visibleChanged && occluders == occluderIndices_) continue;
			occluderIndices_ = std::move(occluders);

			RebuildObjectMeshes(it.world(), tileSize);
		}
	});
	sys.child_of(sceneRoot_).add<ecs::Pausable>();
}

void FarmWorldScene::RefreshVisibleObjects(const tilemap::TilemapViewport& vp) {
	// Visible tile range with margin beyond the ring-buffer edges.
	constexpr int kMargin = 8;
	const int x0 = std::max(0, vp.origin_tile_x - kMargin);
	const int z0 = std::max(0, vp.origin_tile_z - kMargin);
	const int x1 = std::min(worldData_.grid.width, vp.origin_tile_x + vp.buffer_width + kMargin);
	const int z1 = std::min(worldData_.grid.height, vp.origin_tile_z + vp.buffer_height + kMargin);

	visibleObjects_.clear();
	for (const WorldObject& obj : worldData_.objects) {
		if (obj.height <= 0.0F || obj.footprint.empty()) continue;
		const glm::ivec2& t = obj.footprint[0];
		if (t.x >= x0 && t.x < x1 && t.y >= z0 && t.y < z1) {
			visibleObjects_.push_back(obj);
		}
	}
}

std::vector<size_t> FarmWorldScene::FindPlayerOccluders(const float tileSize) const {
	if (!playerEntity_.is_valid() || !cameraEntity_.is_valid()) return {};
	const auto* player = playerEntity_.try_get<spatial::WorldTransform>();
	const auto* camera = cameraEntity_.try_get<spatial::WorldTransform>();
	if (!player || !camera) return {};

	const glm::vec3 focus{player->matrix[3]};
	const auto* body = playerEntity_.try_get<render::CubePrimitive>();
	return FindOccluders(
		visibleObjects_,
		{
			.viewDir = focus - glm::vec3{camera->matrix[3]},
			.focusCenter = focus,
			.focusHalfExtents = body ? body->size * 0.5F : glm::vec3{0.25F},
			.tileSize = tileSize,
			.quadWidthTiles = kObjectQuadWidthTiles,
		}
	);
}

void FarmWorldScene::RebuildObjectMeshes(const flecs::world& world, const float tileSize) {
	// Split the visible objects: the occluders go to the translucent mesh, the rest stay opaque.
	std::vector<WorldObject> opaque;
	std::vector<WorldObject> occluding;
	opaque.reserve(visibleObjects_.size());
	size_t next = 0; // occluderIndices_ is ascending
	for (size_t i = 0; i < visibleObjects_.size(); ++i) {
		if (next < occluderIndices_.size() && occluderIndices_[next] == i) {
			occluding.push_back(visibleObjects_[i]);
			++next;
		}
		else {
			opaque.push_back(visibleObjects_[i]);
		}
	}
	// The translucent pass draws the occluder mesh without depth writes, so its quads must blend
	// back to front. The camera looks along +Z: the farthest row has the largest tile z.
	std::ranges::stable_sort(occluding, [](const WorldObject& a, const WorldObject& b) {
		return a.footprint[0].y > b.footprint[0].y;
	});

	const flecs::entity atlasEnt = objectLayerEntity_.target<render::UseSpriteAtlas>();
	const auto* atlas = atlasEnt.try_get<render::SpriteAtlas>();
	const flecs::entity regEnt = objectLayerEntity_.target<level::UseRegistry>();
	const auto* tileset = regEnt.try_get<tilemap::TileSet>();
	const auto* kindsPtr = world.try_get<ObjectKindRegistry>();
	const ObjectKindRegistry emptyKinds;
	const ObjectKindRegistry& kinds = kindsPtr ? *kindsPtr : emptyKinds;

	UprightMeshConfig cfg{
		.tileSize = tileSize,
		.quadWidthTiles = kObjectQuadWidthTiles,
		.atlasWidthPx = atlas ? static_cast<float>(atlas->image_width_px) : 1.0F,
		.atlasHeightPx = atlas ? static_cast<float>(atlas->image_height_px) : 1.0F,
	};
	const SpriteRectResolver resolver = [tileset](std::string_view sprite) {
		if (!tileset) return std::optional<engine::core::Rect>{};
		const auto found = tileset->name_to_id.find(std::string{sprite});
		if (found == tileset->name_to_id.end()) return std::optional<engine::core::Rect>{};
		const auto* desc = tileset->GetTile(found->second);
		return desc ? std::optional<engine::core::Rect>{desc->tex_coords} : std::optional<engine::core::Rect>{};
	};

	const auto& platform_ref = world.get<platform::PlatformRef>();
	if (!platform_ref.ptr) return;
	ReplaceObjectMesh(*platform_ref.ptr, objectLayerEntity_, BuildUprightObjectMesh(opaque, kinds, cfg, resolver));
	cfg.alpha = kOccluderAlpha;
	ReplaceObjectMesh(*platform_ref.ptr, occluderEntity_, BuildUprightObjectMesh(occluding, kinds, cfg, resolver));
}

void FarmWorldScene::ReplaceObjectMesh(
	platform::Platform& platform, const flecs::entity target, const tilemap::TilemapMeshData& mesh
) {
	// Free the old GPU mesh directly (immediate, bypasses defer so the handle is recycled before we
	// potentially hit the 65536-slot limit on long sessions).
	auto* dm = target.try_get_mut<render::DynamicMesh>();
	if (dm && dm->handle >= 0) {
		platform.UnloadDynamicMesh(dm->handle);
		dm->handle = -1;
	}
	if (mesh.vertices.empty() || mesh.indices.empty()) return;

	const int handle = platform.UploadDynamicMesh(mesh.vertices, mesh.indices, mesh.colors, mesh.uvs);
	if (handle < 0) return;
	if (dm) {
		dm->handle = handle; // value mutation, safe during OnUpdate
	}
	else {
		target.set<render::DynamicMesh>({handle});
	}
}

void FarmWorldScene::RegisterHudSystems(flecs::world& world) {
	// Hotbar slot selection: number keys 1–8 switch the equipped slot; scroll wheel cycles.
	// Not Pausable — inventory management should remain accessible while paused.
	auto hotbarInput = world.system<PlayerInventory>("FarmHotbarInput")
						   .with<PlayerControlled>()
						   .kind(flecs::PreUpdate)
						   .each([](const flecs::iter& it, size_t, PlayerInventory& inv) {
							   const auto& in = it.world().get<input::InputState>();
							   for (int i = 0; i < 8; ++i) {
								   if (in.keys[static_cast<std::size_t>(input::KeyCode::Key1 + i)].pressed) {
									   inv.equippedSlot = i;
									   break;
								   }
							   }
							   if (in.mouse.scroll.y > 0.5F) inv.equippedSlot = std::max(0, inv.equippedSlot - 1);
							   if (in.mouse.scroll.y < -0.5F)
								   inv.equippedSlot =
									   std::min(static_cast<int>(inv.slots.size()) - 1, inv.equippedSlot + 1);
						   });
	hotbarInput.child_of(sceneRoot_);

	// Time HUD: update a ui::Label entity each UI frame.
	// Not pausable so it stays visible when the world is paused.
	auto timeHud = world.system("FarmWorldTimeHud").kind<ecs::OnUI>().run([this](flecs::iter& it) {
		while (it.next()) {
			if (!timeLabelEntity_.is_valid() || !it.world().has<GameTime>()) continue;
			const auto& gt = it.world().get<GameTime>();
			const std::string text = "Day "
									 + std::to_string(gt.day)
									 + "  "
									 + std::string(SeasonName(gt.season))
									 + "  "
									 + std::string(PhaseName(gt.phase));
			if (timeLabelEntity_.has<ui::Label>()) timeLabelEntity_.get_mut<ui::Label>().text = text;
		}
	});
	timeHud.child_of(sceneRoot_);

	// Hotbar: update slot panel border colors and item label text each UI frame.
	// Not pausable so the hotbar stays visible while the game is paused.
	auto hotbar = world.system<const PlayerInventory>("FarmWorldHotbar")
					  .kind<ecs::OnUI>()
					  .with<PlayerControlled>()
					  .each([this](const flecs::iter&, size_t, const PlayerInventory& inv) {
						  for (int i = 0; i < kHotbarSlots; ++i) {
							  if (!hotbarSlotPanels_[i].is_valid()) continue;

							  const bool equipped = (i == inv.equippedSlot);
							  if (hotbarSlotPanels_[i].has<ui::Panel>())
								  hotbarSlotPanels_[i].get_mut<ui::Panel>().border_color =
									  equipped ? kEquipBorder : kSlotBorder;

							  const auto& item = inv.slots[i];
							  if (hotbarItemLabels_[i].is_valid() && hotbarItemLabels_[i].has<ui::Label>())
								  hotbarItemLabels_[i].get_mut<ui::Label>().text =
									  item.typeId.empty() ? "" : item.typeId.substr(0, 6);
							  if (hotbarCountLabels_[i].is_valid() && hotbarCountLabels_[i].has<ui::Label>())
								  hotbarCountLabels_[i].get_mut<ui::Label>().text =
									  (item.count > 1) ? std::to_string(item.count) : "";
						  }
					  });
	hotbar.child_of(sceneRoot_);

	// Minimap update: refresh the HUD local-area texture only when the player moves,
	// and reposition all indicator dots.
	// Not pausable so the minimap stays live while the map modal is open.
	auto minimapUpdate = world.system("FarmWorldMinimapUpdate").kind<ecs::OnUI>().run([this](flecs::iter& it) {
		while (it.next()) {
			auto* platform = it.world().get<platform::PlatformRef>().ptr;
			if (!platform) return;

			if (hudTexture_ >= 0 && playerEntity_.is_valid() && playerEntity_.has<tilemap::GridPosition>()) {
				const auto& gp = playerEntity_.get<tilemap::GridPosition>();
				const glm::ivec2 currentPos{gp.x, gp.z};
				if (currentPos != lastPlayerPos_) {
					lastPlayerPos_ = currentPos;
					const auto* tilemapComp = tilemapEntity_.try_get<tilemap::Tilemap>();
					if (tilemapComp) {
						RasterizeLocalAreaFromTilemap(
							*tilemapComp,
							gp.x,
							gp.z,
							kHudRadiusTiles,
							kHudPx,
							kHudPx,
							hudPixels_
						);
					}
					else {
						RasterizeLocalArea(worldData_, gp.x, gp.z, kHudRadiusTiles, kHudPx, kHudPx, hudPixels_);
					}
					platform->UpdateTexture(hudTexture_, hudPixels_);
				}
			}

			UpdateIndicators(it.world());
		}
	});
	minimapUpdate.child_of(sceneRoot_);
}

void FarmWorldScene::BuildUI(flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const float w = static_cast<float>(platform->Width());
	const float h = static_cast<float>(platform->Height());

	uiRoot_ = world.entity("FarmWorldUI");
	uiRoot_.set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = w, .h = h}});

	// ── Time label (top-left) ───────────────────────────────────────────────
	timeLabelEntity_ =
		ui::CreateLabel(world, {.x = 14.0F, .y = 14.0F, .w = 500.0F, .h = 28.0F}, "Day 1  Spring  Night", 20.0F);
	timeLabelEntity_.get_mut<ui::Label>().color = kTimeColor;
	timeLabelEntity_.get_mut<ui::Label>().align = ui::TextAlign::Left;
	timeLabelEntity_.child_of(uiRoot_);

	// ── Hotbar slots ────────────────────────────────────────────────────────
	const float totalW = static_cast<float>(kHotbarSlots) * (kSlotSize + kSlotGap) - kSlotGap;
	const float startX = (w - totalW) * 0.5F;
	const float slotY = h - kSlotSize - kHotbarPadBot;

	for (int i = 0; i < kHotbarSlots; ++i) {
		const float slotX = startX + static_cast<float>(i) * (kSlotSize + kSlotGap);
		const core::Rect slotRect{.x = slotX, .y = slotY, .w = kSlotSize, .h = kSlotSize};

		hotbarSlotPanels_[i] =
			ui::CreatePanel(world, slotRect, {.color = kSlotBg, .border_color = kSlotBorder, .border_thickness = 2.0F});
		hotbarSlotPanels_[i].child_of(uiRoot_);

		hotbarItemLabels_[i] = ui::CreateLabel(
			world,
			{.x = slotX + 4.0F, .y = slotY + 4.0F, .w = kSlotSize - 8.0F, .h = 16.0F},
			"",
			12.0F
		);
		hotbarItemLabels_[i].get_mut<ui::Label>().align = ui::TextAlign::Left;
		hotbarItemLabels_[i].get_mut<ui::Label>().color = kTextColor;
		hotbarItemLabels_[i].child_of(hotbarSlotPanels_[i]);

		hotbarCountLabels_[i] = ui::CreateLabel(
			world,
			{.x = slotX + kSlotSize - 22.0F, .y = slotY + kSlotSize - 18.0F, .w = 18.0F, .h = 16.0F},
			"",
			12.0F
		);
		hotbarCountLabels_[i].get_mut<ui::Label>().align = ui::TextAlign::Right;
		hotbarCountLabels_[i].get_mut<ui::Label>().color = kTextColor;
		hotbarCountLabels_[i].child_of(hotbarSlotPanels_[i]);
	}

	// "Back to Title" button in the top-right corner.
	std::ignore = ui::CreateButton(
					  world,
					  {.x = w - 130.0F, .y = 10.0F, .w = 120.0F, .h = 36.0F},
					  "Title",
					  [onBack = onBackPressed](const flecs::entity) {
						  if (onBack) onBack();
					  }
	)
					  .set_name("FarmWorldBackButton")
					  .child_of(uiRoot_);

	// "Map (M)" button to open the world map overlay.
	std::ignore = ui::CreateButton(
					  world,
					  {.x = w - 260.0F, .y = 10.0F, .w = 120.0F, .h = 36.0F},
					  "Map (M)",
					  [this](const flecs::entity) {
						  if (mapModal_.is_valid() && mapModal_.has<ui::Modal>()) {
							  mapModal_.get_mut<ui::Modal>().open = true;
							  CenterMapOnPlayer();
						  }
					  }
	)
					  .set_name("FarmWorldMapButton")
					  .child_of(uiRoot_);

	BuildMapUI(world);
}

void FarmWorldScene::LoadHeadless(flecs::world& world, const int seed) {
	sceneRoot_ = world.entity("FarmWorldSceneRoot");

	seed_ = (seed != 0) ? seed : 42;
	worldData_ = WorldGen::Generate(WorldGenParams{}, seed_);

	// Build a minimal TileSet (no file I/O) — name resolution is all that BuildTilemapFromWorldData
	// needs; no tex_coords or atlas assets are required headlessly.
	tilemap::TileSet tileset;
	constexpr std::pair<uint32_t, const char*> kTiles[] = {
		{0, "grass"},
		{1, "forest"},
		{2, "path"},
		{3, "water"},
		{4, "stone_floor"},
		{5, "stone_wall"},
		{6, "sand"},
		{7, "deep_forest"},
		{8, "ruins"},
		{9, "tilled_soil"},
		{10, "dirt"},
		{11, "cobblestone"},
		{12, "brick"},
		{13, "moss"},
	};
	for (const auto& [id, name] : kTiles) tileset.RegisterTile({.id = id, .name = name});

	// Tilemap entity — no UseRegistry/UseSpriteAtlas; those are only needed for GPU rendering.
	tilemapEntity_ = world.entity("FarmWorldTilemap").child_of(sceneRoot_);
	tilemapEntity_.set<tilemap::Tilemap>(BuildTilemapFromWorldData(worldData_, tileset));

	// Player entity at the starting village spawn.
	int spawnX = worldData_.grid.width / 2;
	int spawnZ = worldData_.grid.height / 2;
	if (const auto village = SelectStartingVillage(worldData_)) {
		spawnX = village->location.x;
		spawnZ = village->location.y;
	}

	playerEntity_ = world.entity("Player").child_of(sceneRoot_);
	playerEntity_.add<PlayerControlled>();
	playerEntity_.set<tilemap::GridPosition>({.x = spawnX, .z = spawnZ});
	playerEntity_.set<GridMover>({});
	playerEntity_.set<FacingDirection>({});
	playerEntity_.set<PlayerInventory>({});
	auto& inv = playerEntity_.get_mut<PlayerInventory>();
	AddItem(inv, "hoe", 1);
	AddItem(inv, "watering_can", 1);
	AddItem(inv, "scythe", 1);

	// Spawn villagers headlessly (no file I/O) so integration tests can assert on them. Every living
	// town gets inhabitants; the starting village keeps its authored villagers.
	const auto village = SelectStartingVillage(worldData_);
	const int startVillageTownId = village ? village->id : -1;
	RegisterVillagerSystems(world, sceneRoot_);
	SpawnVillagersHeadless(world, sceneRoot_, worldData_, startVillageTownId);

	// Ambient creatures headlessly (ticket 10): no sprites, but spawn the records so integration
	// tests can assert creatures exist and are non-interactive.
	RegisterCreatureSystems(world, tilemapEntity_);
	SpawnCreatures(world, sceneRoot_, worldData_.creatures);
}

// ── New methods ──────────────────────────────────────────────────────────────────

void FarmWorldScene::BuildTownPins(flecs::world& world) {
	for (const auto& town : worldData_.towns) {
		const auto pin = world.entity().child_of(sceneRoot_);
		pin.set<tilemap::GridPosition>({.x = town.location.x, .z = town.location.y});
		const core::Rgba color = (town.status == TownStatus::Living) ? kPinLiving : kPinRuined;
		pin.set<MinimapTracked>({.color = color});
	}
}

void FarmWorldScene::BakeMapTextures(flecs::world& world) {
	const auto* ref = world.try_get<platform::PlatformRef>();
	if (!ref || !ref->ptr) return;
	auto& platform = *ref->ptr;

	mapPxW_ = worldData_.grid.width * kBigMapPxPerTile;
	mapPxH_ = worldData_.grid.height * kBigMapPxPerTile;

	// Full-world map — baked once from the live tilemap (shows roads, towns, ruins accurately).
	const auto* tilemapComp = tilemapEntity_.try_get<tilemap::Tilemap>();
	if (tilemapComp) {
		const auto mapPixels = RasterizeWorldMapFromTilemap(*tilemapComp, mapPxW_, mapPxH_);
		mapTexture_ = platform.UploadTexture(mapPxW_, mapPxH_, mapPixels);
	}
	else {
		// Fallback to biome rasterizer if tilemap entity isn't ready.
		const auto mapPixels = RasterizeWorldMap(worldData_, mapPxW_, mapPxH_);
		mapTexture_ = platform.UploadTexture(mapPxW_, mapPxH_, mapPixels);
	}
	if (mapTexture_ >= 0) {
		platform.SetTextureFilter(mapTexture_, false); // NEAREST — crisp tiles
		platform.SetTextureWrap(mapTexture_, true);
	}

	// Pre-size the HUD pixel buffer once; FarmWorldMinimapUpdate fills it in-place from the tilemap.
	hudPixels_.assign(static_cast<std::size_t>(kHudPx) * kHudPx * 4, 255);

	// HUD local-area minimap — initial pixels centred on the world midpoint.
	const int startX = worldData_.grid.width / 2;
	const int startZ = worldData_.grid.height / 2;
	if (tilemapComp) {
		RasterizeLocalAreaFromTilemap(*tilemapComp, startX, startZ, kHudRadiusTiles, kHudPx, kHudPx, hudPixels_);
	}
	else {
		RasterizeLocalArea(worldData_, startX, startZ, kHudRadiusTiles, kHudPx, kHudPx, hudPixels_);
	}
	hudTexture_ = platform.UploadTexture(kHudPx, kHudPx, hudPixels_);
	if (hudTexture_ >= 0) {
		platform.SetTextureFilter(hudTexture_, false);
		platform.SetTextureWrap(hudTexture_, true);
	}

	// Build the tracked-entities query once; reused every frame by UpdateIndicators.
	trackedQuery_ = world.query_builder<const MinimapTracked>().build();
}

void FarmWorldScene::BuildMapUI(flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const float w = static_cast<float>(platform->Width());
	const float h = static_cast<float>(platform->Height());

	// ── Always-on corner HUD minimap (upper-right) ──────────────────────────────
	hudRect_ = {
		.x = w - kHudPx - 16.0F,
		.y = 58.0F, // below the top buttons
		.w = static_cast<float>(kHudPx),
		.h = static_cast<float>(kHudPx),
	};
	if (hudTexture_ >= 0) {
		std::ignore = ui::CreatePanel(
						  world,
						  hudRect_,
						  {.color = {.r = 255, .g = 255, .b = 255, .a = 255},
						   .border_color = core::colors::Border,
						   .border_thickness = 2.0F,
						   .texture = hudTexture_}
		)
						  .child_of(uiRoot_);
	}

	// ── Full-screen map modal (toggled with M or the "Map" button) ───────────────
	const float vw = std::min(static_cast<float>(mapPxW_), w - 160.0F);
	const float vh = std::min(static_cast<float>(mapPxH_), h - 200.0F);
	const core::Rect mmRect{
		.x = (w - vw) * 0.5F,
		.y = (h - vh) * 0.5F,
		.w = vw,
		.h = vh,
	};

	// Content rect origin = viewport origin; content size = full map texture.
	bigMapRect_ = {
		.x = mmRect.x,
		.y = mmRect.y,
		.w = static_cast<float>(mapPxW_),
		.h = static_cast<float>(mapPxH_),
	};
	bigMapView_ = mmRect;

	mapModal_ = ui::CreateModal(world, mmRect, {.open = false}).child_of(uiRoot_);

	// Title label above the viewport.
	std::ignore = ui::CreateLabel(
					  world,
					  {.x = mmRect.x, .y = mmRect.y - 42.0F, .w = vw, .h = 30.0F},
					  "World Map  —  M / Esc to close",
					  20.0F
	)
					  .child_of(mapModal_);

	// Close button.
	std::ignore = ui::CreateButton(
					  world,
					  {.x = mmRect.x + vw - 100.0F, .y = mmRect.y - 42.0F, .w = 96.0F, .h = 30.0F},
					  "Close",
					  [this](const flecs::entity) {
						  if (mapModal_.is_valid() && mapModal_.has<ui::Modal>())
							  mapModal_.get_mut<ui::Modal>().open = false;
					  }
	).child_of(mapModal_);

	// Scroll rect (provides clipping + optional panning when map > viewport).
	const auto mmScroll =
		ui::CreateScrollRect(world, mmRect, {.vertical = true, .horizontal = true}).child_of(mapModal_);
	minimapScroll_ = mmScroll;

	// The map texture panel inside the scroll rect.
	if (mapTexture_ >= 0) {
		std::ignore =
			ui::CreatePanel(
				world,
				{.x = mmRect.x, .y = mmRect.y, .w = static_cast<float>(mapPxW_), .h = static_cast<float>(mapPxH_)},
				{.color = {.r = 255, .g = 255, .b = 255, .a = 255}, .border_thickness = 0.0F, .texture = mapTexture_}
			)
				.child_of(mmScroll);
	}

	// One dot per MinimapTracked entity (trackedQuery_ built in BakeMapTextures).
	if (trackedQuery_) {
		trackedQuery_.each([&](const flecs::entity tracked, const MinimapTracked& mt) {
			const auto hudDot = ui::CreatePanel(
									world,
									{.x = hudRect_.x, .y = hudRect_.y, .w = kDotSizeHud, .h = kDotSizeHud},
									{.color = mt.color, .border_thickness = 0.0F}
			)
									.child_of(uiRoot_);
			const auto bigDot = ui::CreatePanel(
									world,
									{.x = bigMapRect_.x, .y = bigMapRect_.y, .w = kDotSizeBig, .h = kDotSizeBig},
									{.color = mt.color, .border_thickness = 0.0F}
			)
									.child_of(minimapScroll_);
			indicators_.push_back({.tracked = tracked, .hudDot = hudDot, .bigDot = bigDot});
		});
	}
}

void FarmWorldScene::UpdateIndicators(const flecs::world& world) {
	// Purge indicators whose tracked entity has been removed.
	std::vector<MinimapIndicator> alive;
	alive.reserve(indicators_.size());
	for (auto& ind : indicators_) {
		if (!ind.tracked.is_valid() || !ind.tracked.has<MinimapTracked>()) {
			if (ind.hudDot.is_valid()) ind.hudDot.destruct();
			if (ind.bigDot.is_valid()) ind.bigDot.destruct();
			continue;
		}
		alive.push_back(ind);
	}
	indicators_.swap(alive);

	// Add indicators for newly added MinimapTracked entities (reuse pre-built query).
	if (trackedQuery_) {
		trackedQuery_.each([&](const flecs::entity tracked, const MinimapTracked& mt) {
			const bool exists =
				std::ranges::any_of(indicators_, [&](const MinimapIndicator& ind) { return ind.tracked == tracked; });
			if (exists) return;
			const auto hudDot = ui::CreatePanel(
									world,
									{.x = hudRect_.x, .y = hudRect_.y, .w = kDotSizeHud, .h = kDotSizeHud},
									{.color = mt.color, .border_thickness = 0.0F}
			)
									.child_of(uiRoot_);
			const auto bigDot = ui::CreatePanel(
									world,
									{.x = bigMapRect_.x, .y = bigMapRect_.y, .w = kDotSizeBig, .h = kDotSizeBig},
									{.color = mt.color, .border_thickness = 0.0F}
			)
									.child_of(minimapScroll_);
			indicators_.push_back({.tracked = tracked, .hudDot = hudDot, .bigDot = bigDot});
		});
	}

	// Compute the player's grid position for the HUD local-area fraction.
	glm::ivec2 playerPos{worldData_.grid.width / 2, worldData_.grid.height / 2};
	if (playerEntity_.is_valid() && playerEntity_.has<tilemap::GridPosition>()) {
		const auto& gp = playerEntity_.get<tilemap::GridPosition>();
		playerPos = {gp.x, gp.z};
	}

	const int gridW = worldData_.grid.width;
	const int gridH = worldData_.grid.height;

	for (auto& ind : indicators_) {
		if (!ind.tracked.is_valid()) continue;

		// Resolve world-grid position from GridPosition or (fallback) Transform.
		glm::ivec2 pos{};
		bool hasPos = false;
		if (ind.tracked.has<tilemap::GridPosition>()) {
			const auto& gp = ind.tracked.get<tilemap::GridPosition>();
			pos = {gp.x, gp.z};
			hasPos = true;
		}
		else if (ind.tracked.has<spatial::Transform>()) {
			const auto& tr = ind.tracked.get<spatial::Transform>();
			pos = {
				static_cast<int>(tr.position.x / kStep - 0.5F),
				static_cast<int>(tr.position.z / kStep - 0.5F),
			};
			hasPos = true;
		}
		if (!hasPos) continue;

		const auto color = ind.tracked.get<MinimapTracked>().color;

		// ── Big map: fractional position within full world texture ────────────
		if (ind.bigDot.is_valid() && ind.bigDot.has<ui::UIRect>() && ind.bigDot.has<ui::Panel>()) {
			const auto frac = ComputePinFraction(pos, gridW, gridH);
			auto& r = ind.bigDot.get_mut<ui::UIRect>().rect;
			r.x = bigMapRect_.x + frac.x * bigMapRect_.w - r.w * 0.5F;
			r.y = bigMapRect_.y + frac.y * bigMapRect_.h - r.h * 0.5F;
			ind.bigDot.get_mut<ui::Panel>().color = color;
		}

		// ── HUD minimap: fractional position within local area view ───────────
		if (ind.hudDot.is_valid() && ind.hudDot.has<ui::UIRect>() && ind.hudDot.has<ui::Panel>()) {
			const auto localFrac = ComputeLocalPinFraction(pos, playerPos, kHudRadiusTiles);
			const bool inView =
				localFrac.x >= 0.0F && localFrac.x <= 1.0F && localFrac.y >= 0.0F && localFrac.y <= 1.0F;
			auto& r = ind.hudDot.get_mut<ui::UIRect>().rect;
			if (inView) {
				r.x = hudRect_.x + localFrac.x * hudRect_.w - r.w * 0.5F;
				r.y = hudRect_.y + localFrac.y * hudRect_.h - r.h * 0.5F;
			}
			else {
				// Move off-screen so the dot isn't visible outside the HUD panel.
				r.x = -100.0F;
				r.y = -100.0F;
			}
			ind.hudDot.get_mut<ui::Panel>().color = color;
		}
	}
}

void FarmWorldScene::CenterMapOnPlayer() const {
	if (!minimapScroll_.is_valid()
		|| !minimapScroll_.has<ui::ScrollRect>()
		|| !playerEntity_.is_valid()
		|| !playerEntity_.has<tilemap::GridPosition>()) {
		return;
	}
	const auto& gp = playerEntity_.get<tilemap::GridPosition>();
	const float fx = ComputePinFraction({gp.x, gp.z}, worldData_.grid.width, worldData_.grid.height).x;
	const float fz = ComputePinFraction({gp.x, gp.z}, worldData_.grid.width, worldData_.grid.height).y;
	const float px = fx * bigMapRect_.w;
	const float pz = fz * bigMapRect_.h;
	const float maxX = std::max(0.0F, bigMapRect_.w - bigMapView_.w);
	const float maxZ = std::max(0.0F, bigMapRect_.h - bigMapView_.h);
	auto& sr = minimapScroll_.get_mut<ui::ScrollRect>();
	sr.scroll.x = std::clamp(px - bigMapView_.w * 0.5F, 0.0F, maxX);
	sr.scroll.y = std::clamp(pz - bigMapView_.h * 0.5F, 0.0F, maxZ);
}

} // namespace game::farm
