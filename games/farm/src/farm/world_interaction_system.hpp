#pragma once

#include <string>
#include <utility>

#include <flecs.h>

#include "engine/engine.hpp"
#include "farm_components.hpp"

namespace game::farm {

// Loads crops.json from the platform asset directory and sets the CropRegistry singleton.
// Falls back to an empty registry when the file cannot be found (headless/test builds).
void LoadCropRegistry(flecs::world& world);

// Compute the tile grid coordinate the player is currently facing.
[[nodiscard]] std::pair<int, int>
ComputeFacingTile(const engine::tilemap::GridPosition& gp, const FacingDirection& facing);

// Which category of interaction is available for the player's facing tile.
enum class InteractionKind : uint8_t {
	None,
	TillSoil,
	Water,
	Plant, // seed item on TilledSoil (no Crop present)
	Harvest,
	Pickup,
	Interact,
};

// The resolved interaction offered to the player this frame.
struct AvailableInteraction {
	InteractionKind kind = InteractionKind::None;
	std::string label;      // hint text, e.g. "Till soil" / "Harvest wheat"
	std::string cropTypeId; // populated for Harvest and Plant
	std::string seedItemId; // populated for Plant (removed from inventory on confirm)
};

// Determine what interaction (if any) is available given:
//   equippedTool   — typeId of the equipped item (empty = bare hands)
//   facingEntity   — entity at the facing tile (invalid = bare ground)
//   registry       — crop type definitions
//   currentSeason  — used to gate planting by seasonal validity (default: Spring)
[[nodiscard]] AvailableInteraction DetermineInteraction(
	const std::string& equippedTool,
	flecs::entity facingEntity,
	const CropRegistry& registry,
	game::Season currentSeason = game::Season::Spring
);

// Execute an already-determined interaction, mutating world state.
// `tilemapEntity` is optional: if valid, TillSoil visually updates the tilemap tile.
void ExecuteInteraction(
	flecs::world& world,
	flecs::entity playerEntity,
	flecs::entity facingEntity,
	int facingX,
	int facingZ,
	const AvailableInteraction& action,
	flecs::entity tilemapEntity = {}
);

// Register the three WorldInteraction systems as children of sceneRoot.
// `tilemapEntity` is forwarded to ExecuteInteraction so TillSoil can update the tile visually.
void RegisterWorldInteractionSystems(flecs::world& world, flecs::entity sceneRoot, flecs::entity tilemapEntity = {});

} // namespace game::farm
