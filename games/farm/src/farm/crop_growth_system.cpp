#include "crop_growth_system.hpp"


#include "engine/engine.hpp"
#include "farm_components.hpp"
#include "game_time.hpp"

namespace game::farm {

void RegisterCropGrowthSystem(flecs::world& world, flecs::entity sceneRoot) {
	// Align the tracker to the current absoluteDay so the system doesn't fire on the very
	// first progress() call if absoluteDay is still 0 (no day has yet elapsed).
	const int startDay = world.has<game::GameTime>() ? world.get<game::GameTime>().absoluteDay : 0;
	world.set<CropGrowthTracker>({.lastProcessedAbsoluteDay = startDay});

	// Pre-build queries to avoid per-frame allocation.
	auto cropQuery = world.query_builder<TilledSoil, Crop, GrowthStage>().cached().build();
	auto bareQuery = world.query_builder<TilledSoil>().without<Crop>().cached().build();

	auto sys = world.system("CropGrowthSystem")
				   .kind(flecs::OnUpdate)
				   .run([cropQuery, bareQuery](flecs::iter& it) {
					   while (it.next()) {
						   flecs::world w = it.world();
						   if (!w.has<game::GameTime>() || !w.has<CropRegistry>()) return;
						   const auto& gt = w.get<game::GameTime>();
						   auto& tracker = w.get_mut<CropGrowthTracker>();
						   const auto& registry = w.get<CropRegistry>();

						   if (gt.absoluteDay <= tracker.lastProcessedAbsoluteDay) return;

						   // Advance growth for all tiles that have a planted crop.
						   cropQuery.each([&](flecs::entity, TilledSoil& soil, Crop& crop, GrowthStage& gs) {
							   soil.daysEmpty = 0; // has a crop — not neglected

							   const auto* def = FindCropDef(registry, crop.typeId);
							   if (!def) {
								   crop.watered = false;
								   return;
							   }

							   if (crop.watered || !def->requiresWatering) {
								   if (gs.stage != Stage::Harvestable) {
									   const auto idx = static_cast<size_t>(gs.stage);
									   gs.daysAtStage++;
									   if (gs.daysAtStage >= def->stageDays[idx]) {
										   gs.stage = static_cast<Stage>(static_cast<int>(gs.stage) + 1);
										   gs.daysAtStage = 0;
										   engine::ecs::Log(w).debug(
											   "[CropGrowth] '{}' advanced to stage {}",
											   crop.typeId,
											   static_cast<int>(gs.stage)
										   );
									   }
								   }
							   }

							   crop.watered = false; // reset for the next day
						   });

						   // Reclaim neglected tilled soil after a full season without a crop.
						   bareQuery.each([&](flecs::entity e, TilledSoil& soil) {
							   soil.daysEmpty++;
							   if (soil.daysEmpty >= gt.daysPerSeason) {
								   e.remove<TilledSoil>();
								   engine::ecs::Log(w).debug(
									   "[CropGrowth] Reclaimed neglected TilledSoil entity {}",
									   e.id()
								   );
							   }
						   });

						   tracker.lastProcessedAbsoluteDay = gt.absoluteDay;
					   }
				   })
				   .add<engine::ecs::Pausable>();
	sys.child_of(sceneRoot);
}

} // namespace game::farm
