#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include <flecs.h>
#include <glm/glm.hpp>

#include "engine/engine.hpp"
#include "farm_components.hpp"
#include "farm_tilemap_builder.hpp"
#include "world_gen.hpp"
#include "world_interaction_system.hpp"

namespace game::farm {

/// Identity tag for the farm world scene.
struct FarmWorldSceneTag {};

/// Add this component to any entity to make it appear as a pin on the world map and HUD minimap.
/// The player, living towns, and ruined towns all carry this; distinct colours per pin type.
struct MinimapTracked {
	engine::core::Rgba color{.r = 255, .g = 60, .b = 60, .a = 255};
};

/// The main playable farm prototype scene. Generates a world from WorldGen::Generate,
/// builds a tilemap entity from the biome grid, places the player at the starting village,
/// and registers a time HUD + hotbar overlay, plus a world map (M) and corner minimap.
class FarmWorldScene : public engine::scene::SceneBase {
public:
	FarmWorldScene() = default;
	~FarmWorldScene() override = default;

	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void Unload(flecs::world& world) override;
	void Tick(const flecs::world& world, float deltaTime) override;
	void HandleInput(const flecs::world& world, const engine::input::InputState& input) override;

	/// Callback invoked when the player presses the back / return-to-title button.
	std::function<void()> onBackPressed;

	/// Headless path: builds WorldData + tilemap + player entities without file I/O.
	/// Suitable for unit-test harnesses that have no asset directory (e.g. NullPlatform).
	/// Registers GameTime and PlayerInventory before calling if they are not already set.
	void LoadHeadless(flecs::world& world, int seed = 42);

private:
	flecs::entity sceneRoot_;
	flecs::entity tilemapEntity_;
	flecs::entity objectLayerEntity_; // opaque upright objects near the view
	flecs::entity occluderEntity_;    // the objects in front of the player, drawn translucent
	flecs::entity playerEntity_;
	flecs::entity cameraEntity_; // the player's follow camera, for the occluder test
	flecs::entity uiRoot_;

	int seed_{0}; // 0 = generate randomly in Load()
	WorldData worldData_{};

	// ── Map / minimap ──────────────────────────────────────────────────────────────
	int mapTexture_{-1}; // full-world map texture handle (baked once)
	int hudTexture_{-1}; // HUD local-area texture handle (updated on player movement)
	int mapPxW_{0};      // full-world map texture width in pixels
	int mapPxH_{0};      // full-world map texture height in pixels

	std::vector<uint8_t> hudPixels_{}; // pre-sized RGBA buffer for HUD minimap
	glm::ivec2 lastPlayerPos_{-1, -1}; // position at last hudTexture upload

	flecs::entity mapModal_{};        // full-screen map Modal entity (toggled with M)
	flecs::entity minimapScroll_{};   // ScrollRect inside the map modal
	engine::core::Rect hudRect_{};    // screen rect of the corner HUD minimap panel
	engine::core::Rect bigMapRect_{}; // content rect (viewport origin, full map size)
	engine::core::Rect bigMapView_{}; // viewport rect (clamped to screen)

	flecs::query<const MinimapTracked> trackedQuery_{}; // built once in BakeMapTextures

	/// Last tilemap viewport origin used to build the upright-object mesh. Sentinel INT_MIN
	/// forces a rebuild on the first frame (origin is always ≥ 0).
	int lastObjectOriginX_{std::numeric_limits<int>::min()};
	int lastObjectOriginZ_{std::numeric_limits<int>::min()};
	std::vector<WorldObject> visibleObjects_; // objects within the viewport window (plus margin)
	std::vector<size_t> occluderIndices_;     // ascending indices into visibleObjects_ drawn translucent

	// ── HUD widgets (updated from systems rather than drawn directly) ──────────────
	static constexpr int kHotbarSlots = 8;
	flecs::entity timeLabelEntity_{};
	std::array<flecs::entity, kHotbarSlots> hotbarSlotPanels_{};
	std::array<flecs::entity, kHotbarSlots> hotbarItemLabels_{};
	std::array<flecs::entity, kHotbarSlots> hotbarCountLabels_{};

	/// One dot per MinimapTracked entity on each map surface.
	struct MinimapIndicator {
		flecs::entity tracked;
		flecs::entity hudDot;
		flecs::entity bigDot;
	};
	std::vector<MinimapIndicator> indicators_;

	void SetupTilemap(flecs::world& world);
	void SetupPlayer(flecs::world& world);
	void AttachVillagerSprites(flecs::world& world);
	void BuildTownPins(flecs::world& world);
	void BakeMapTextures(flecs::world& world);
	void RegisterHudSystems(flecs::world& world);
	void RegisterObjectStreamingSystem(flecs::world& world);
	void RefreshVisibleObjects(const engine::tilemap::TilemapViewport& vp);
	[[nodiscard]] std::vector<size_t> FindPlayerOccluders(float tileSize) const;
	void RebuildObjectMeshes(const flecs::world& world, float tileSize);
	static void ReplaceObjectMesh(
		engine::platform::Platform& platform, flecs::entity target, const engine::tilemap::TilemapMeshData& mesh
	);
	void BuildUI(flecs::world& world);
	void BuildMapUI(flecs::world& world);
	void UpdateIndicators(const flecs::world& world);
	void CenterMapOnPlayer() const;
};

} // namespace game::farm
