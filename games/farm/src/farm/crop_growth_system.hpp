#pragma once

#include <flecs.h>

namespace game::farm {

// Prevents CropGrowthSystem from processing the same absoluteDay twice.
struct CropGrowthTracker {
	int lastProcessedAbsoluteDay = 0;
};

// Register CropGrowthSystem (OnUpdate, Pausable) as a child of sceneRoot.
// Each new game day the system:
//   - Advances GrowthStage for crops that are watered (or don't require watering).
//   - Resets Crop.watered to false for the next day.
//   - Increments TilledSoil.daysEmpty for bare plots; removes TilledSoil after a full season.
// Prerequisites: CropRegistry and GameTime singletons must be set before the first progress().
void RegisterCropGrowthSystem(flecs::world& world, flecs::entity sceneRoot);

} // namespace game::farm
