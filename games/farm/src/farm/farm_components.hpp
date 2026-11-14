#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include "game_time.hpp"

namespace game::farm {

// ── Growth stage ─────────────────────────────────────────────────────────────

// Ordered progression for a planted crop. Closed set — per-crop timing is data.
enum class Stage : int32_t {
	Planted     = 0,
	Sprouting   = 1,
	Mature      = 2,
	Harvestable = 3,
};

// Driven by CropGrowthSystem; paired with Crop on TilledSoil entities.
struct GrowthStage {
	Stage stage       = Stage::Planted;
	int   daysAtStage = 0; // days spent in the current stage (watered days only when requiresWatering)
};

// Direction the player is currently facing. Updated each frame from GridMover delta
// by FarmFacingUpdater. Tests may set this directly.
struct FacingDirection {
	int dx = 0;
	int dz = 1; // default: "up" direction (W/up-arrow in tilemap coords)
};

// Marks a tile entity as tilled soil — prepared for planting.
// Placed by WorldInteractionSystem when the player uses a hoe on bare ground.
// Removed by CropGrowthSystem when daysEmpty reaches daysPerSeason (bare-ground reclaim).
struct TilledSoil {
	int daysEmpty = 0; // consecutive days without a Crop; resets to 0 when a Crop is planted
};

// Represents a planted crop on a TilledSoil entity.
// typeId references a CropDef in CropRegistry. Ticket 07 adds GrowthStage on top of this.
struct Crop {
	std::string typeId;
	bool watered   = false;
};

// A world entity the player can pick up; confirm adds typeId×count to PlayerInventory
// and removes the entity from the world.
struct Pickupable {
	std::string typeId;
	int count = 1;
};

/// Zero-size tag that narrows the facing-tile query to entities the player can
/// actually interact with (TilledSoil, Pickupable items, Interactable objects/NPCs).
/// Must be present on every such entity for FarmWorldInteraction to discover it.
struct FarmInteractable {};

// Marks an entity as generically interactable (door, NPC trigger, etc.).
// Confirm fires an InteractWith event; the entity's own observers decide what happens next.
struct Interactable {};

// Flecs event emitted on an Interactable entity when the player confirms interaction.
struct InteractWith {};

// Toggle state for a door — used as a smoke test for the Interactable path.
struct DoorOpen {
	bool open = false;
};

// Singleton: current interaction hint shown to the player (updated every frame).
struct InteractionHint {
	std::string label;
	bool visible = false;
};

// Per-crop type data loaded from games/farm/assets/data/crops.json.
struct CropDef {
	std::string typeId;
	std::string name;
	std::string harvestTool; // empty = bare-hand harvest (no tool required)
	std::string seedItemId;  // inventory typeId used to plant this crop (e.g. "seed_wheat")

	// Days required at each stage before advancing:
	//   index 0 = Planted→Sprouting, 1 = Sprouting→Mature, 2 = Mature→Harvestable
	std::array<int, 3> stageDays = {2, 3, 2};

	// Seasons this crop may be planted in; empty = valid in all seasons.
	std::vector<game::Season> seasonsValid;

	// When true, crop only advances stages while the watered flag is set the previous day.
	// When false, crop advances each day regardless of watering.
	bool requiresWatering = true;

	int sellPrice = 1;
};

// Singleton holding all loaded crop type definitions, keyed by typeId.
struct CropRegistry {
	std::unordered_map<std::string, CropDef> defs;
	std::unordered_map<std::string, std::string> seedToCropId; // seedItemId → typeId
};

/// Return the CropDef for `typeId`, or nullptr if not found.
[[nodiscard]] inline const CropDef* FindCropDef(const CropRegistry& reg, std::string_view typeId) {
	const auto it = reg.defs.find(std::string{typeId});
	return (it != reg.defs.end()) ? &it->second : nullptr;
}

/// Return the CropDef whose seedItemId matches `seedItemId`, or nullptr if not found.
[[nodiscard]] inline const CropDef* FindCropDefBySeed(const CropRegistry& reg, std::string_view seedItemId) {
	const auto it = reg.seedToCropId.find(std::string{seedItemId});
	if (it == reg.seedToCropId.end()) return nullptr;
	return FindCropDef(reg, it->second);
}

} // namespace game::farm
