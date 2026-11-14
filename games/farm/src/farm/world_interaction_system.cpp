#include "world_interaction_system.hpp"

#include <cmath>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "engine/engine.hpp"
#include "game_time.hpp"
#include "player_inventory.hpp"

using namespace engine;
using tilemap::GridMover;
using tilemap::PlayerControlled;

namespace game::farm {

// ── CropRegistry loading ──────────────────────────────────────────────────────

void LoadCropRegistry(flecs::world& world) {
	CropRegistry registry;

	const auto* ref = world.try_get<platform::PlatformRef>();
	const std::string path = (ref && ref->ptr) ? (ref->ptr->AssetDirectory() + "/data/crops.json") : "";

	if (!path.empty()) {
		const auto j = core::LoadJsonFile(path, "CropRegistry");
		if (j) {
			for (const auto& crop : j->at("crops")) {
				CropDef def{
					.typeId = crop.value("typeId", ""),
					.name = crop.value("name", ""),
					.harvestTool = crop.value("harvestTool", ""),
					.seedItemId = crop.value("seedItemId", ""),
					.requiresWatering = crop.value("requiresWatering", true),
					.sellPrice = crop.value("sellPrice", 1),
				};

				if (crop.contains("stageDays") && crop["stageDays"].is_array()) {
					const auto& arr = crop["stageDays"];
					for (size_t i = 0; i < 3 && i < arr.size(); ++i) def.stageDays[i] = arr[i].get<int>();
				}

				if (crop.contains("seasonsValid")) {
					for (const auto& sv : crop["seasonsValid"]) {
						const std::string s = sv.get<std::string>();
						if (s == "spring") def.seasonsValid.push_back(Season::Spring);
						else if (s == "summer") def.seasonsValid.push_back(Season::Summer);
						else if (s == "autumn") def.seasonsValid.push_back(Season::Autumn);
						else if (s == "winter") def.seasonsValid.push_back(Season::Winter);
					}
				}

				if (!def.typeId.empty()) {
					if (!def.seedItemId.empty()) registry.seedToCropId[def.seedItemId] = def.typeId;
					registry.defs[def.typeId] = std::move(def);
				}
			}
			engine::ecs::Log(world).info("[CropRegistry] Loaded {} crop type(s)", registry.defs.size());
		}
	}

	world.set<CropRegistry>(std::move(registry));
}

// ── Pure helpers ──────────────────────────────────────────────────────────────

std::pair<int, int> ComputeFacingTile(const tilemap::GridPosition& gp, const FacingDirection& facing) {
	return {gp.x + facing.dx, gp.z + facing.dz};
}

AvailableInteraction DetermineInteraction(
	const std::string& equippedTool,
	flecs::entity facingEntity,
	const CropRegistry& registry,
	Season currentSeason
) {
	if (facingEntity.is_valid()) {
		// Pickupable: no tool requirement
		if (const auto* pick = facingEntity.try_get<Pickupable>()) {
			return {
				.kind = InteractionKind::Pickup,
				.label = "Pick up " + pick->typeId,
			};
		}

		// Interactable: no tool requirement
		if (facingEntity.has<Interactable>()) {
			return {
				.kind = InteractionKind::Interact,
				.label = "Interact",
			};
		}

		// Crop: harvest first (if Harvestable), then water (non-harvestable + unwatered)
		if (const auto* crop = facingEntity.try_get<Crop>()) {
			const auto* gs = facingEntity.try_get<GrowthStage>();

			if (gs && gs->stage == Stage::Harvestable) {
				const CropDef* def = FindCropDef(registry, crop->typeId);
				const std::string& harvestTool = def ? def->harvestTool : "";
				const bool toolMatches = harvestTool.empty() || equippedTool == harvestTool;
				if (toolMatches) {
					return {
						.kind = InteractionKind::Harvest,
						.label = "Harvest " + crop->typeId,
						.cropTypeId = crop->typeId,
					};
				}
			}

			if (equippedTool == "watering_can" && !crop->watered && !(gs && gs->stage == Stage::Harvestable)) {
				return {
					.kind = InteractionKind::Water,
					.label = "Water " + crop->typeId,
				};
			}

			if (gs && gs->stage != Stage::Harvestable) {
				// Not harvestable yet, no other action available for this crop.
			}
		}

		// TilledSoil with no Crop + seed item → Plant (season-gated)
		if (facingEntity.has<TilledSoil>() && !facingEntity.has<Crop>()) {
			if (const auto* cropDef = FindCropDefBySeed(registry, equippedTool)) {
				bool seasonOk = cropDef->seasonsValid.empty();
				if (!seasonOk) {
					for (const auto s : cropDef->seasonsValid) {
						if (s == currentSeason) {
							seasonOk = true;
							break;
						}
					}
				}
				if (seasonOk) {
					return {
						.kind = InteractionKind::Plant,
						.label = "Plant " + cropDef->name,
						.cropTypeId = cropDef->typeId,
						.seedItemId = equippedTool,
					};
				}
				return {.kind = InteractionKind::None, .label = "Wrong season for " + cropDef->name};
			}
			// No seed equipped — nudge the player.
			return {.kind = InteractionKind::None, .label = "Equip seeds to plant"};
		}
	}

	// Bare ground + hoe → till
	if (!facingEntity.is_valid() && equippedTool == "hoe") {
		return {
			.kind = InteractionKind::TillSoil,
			.label = "Till soil",
		};
	}

	return {};
}

void ExecuteInteraction(
	flecs::world& world,
	flecs::entity playerEntity,
	flecs::entity facingEntity,
	int facingX,
	int facingZ,
	const AvailableInteraction& action,
	flecs::entity tilemapEntity
) {
	// Tile ID 9 = "tilled_soil" in assets/data/tilesets/basic.json.
	constexpr uint32_t kTilledSoilTileId = 9u;
	switch (action.kind) {

	case InteractionKind::TillSoil:
		world.entity()
			.set<TilledSoil>({})
			.set<tilemap::GridPosition>({.x = facingX, .z = facingZ})
			.add<FarmInteractable>();
		// Visually update the tilemap tile so the player sees tilled earth immediately.
		// Skip modified<Tilemap>() — it resets the viewport streaming origin, causing a
		// one-frame flicker. The streaming system will pick up the tile change on the next
		// viewport refresh (when the player moves).
		if (tilemapEntity.is_valid()) {
			const auto handle = world.entity(tilemapEntity.id());
			if (auto* tm = handle.try_get_mut<tilemap::Tilemap>()) {
				tilemap::SetTile(*tm, facingX, facingZ, kTilledSoilTileId);
			}
		}
		engine::ecs::Log(world).debug("[WorldInteraction] Tilled soil at ({},{})", facingX, facingZ);
		break;

	case InteractionKind::Water:
		if (auto* crop = facingEntity.try_get_mut<Crop>()) crop->watered = true;
		engine::ecs::Log(world).debug("[WorldInteraction] Watered crop at ({},{})", facingX, facingZ);
		break;

	case InteractionKind::Plant:
	{
		facingEntity.set<Crop>({
			.typeId = action.cropTypeId,
			.watered = false,
		});
		facingEntity.set<GrowthStage>({
			.stage = Stage::Planted,
			.daysAtStage = 0,
		});
		if (auto* inv = playerEntity.try_get_mut<PlayerInventory>()) RemoveItem(*inv, action.seedItemId, 1);
		engine::ecs::Log(world)
			.debug("[WorldInteraction] Planted '{}' at ({},{})", action.cropTypeId, facingX, facingZ);
		break;
	}

	case InteractionKind::Harvest:
		if (auto* inv = playerEntity.try_get_mut<PlayerInventory>()) AddItem(*inv, action.cropTypeId);
		facingEntity.remove<Crop>();
		facingEntity.remove<GrowthStage>();
		engine::ecs::Log(world)
			.debug("[WorldInteraction] Harvested '{}' at ({},{})", action.cropTypeId, facingX, facingZ);
		break;

	case InteractionKind::Pickup:
		if (const auto* pick = facingEntity.try_get<Pickupable>()) {
			if (auto* inv = playerEntity.try_get_mut<PlayerInventory>()) AddItem(*inv, pick->typeId, pick->count);
		}
		facingEntity.destruct();
		engine::ecs::Log(world).debug("[WorldInteraction] Picked up item at ({},{})", facingX, facingZ);
		break;

	case InteractionKind::Interact:
		// Associate the event with Interactable so observers filtered on that component match.
		world.event<InteractWith>().id<Interactable>().entity(facingEntity).emit();
		engine::ecs::Log(world).debug("[WorldInteraction] Fired InteractWith at ({},{})", facingX, facingZ);
		break;

	case InteractionKind::None: break;
	}
}

// ── System registration ───────────────────────────────────────────────────────

void RegisterWorldInteractionSystems(flecs::world& world, flecs::entity sceneRoot, flecs::entity tilemapEntity) {
	// Pre-register every farm-component type that DetermineInteraction/ExecuteInteraction may access
	// via try_get<T>()/has<T>() at runtime.  Without this, Flecs lazily calls ecs_new_low_id() on
	// first use, which asserts when the world is readonly AND has multiple stages (set by the physics
	// module).  world.component<T>() is a no-op if the type is already registered.
	world.component<Pickupable>();
	world.component<Interactable>();
	world.component<InteractWith>();
	world.component<DoorOpen>();
	world.component<Crop>();
	world.component<GrowthStage>();
	world.component<TilledSoil>();
	world.component<FarmInteractable>();

	if (!world.has<CropRegistry>()) world.set<CropRegistry>({});
	if (!world.has<InteractionHint>()) world.set<InteractionHint>({});

	// FarmFacingUpdater: derives FacingDirection from GridMover's last movement delta.
	// Runs PreUpdate so the direction is ready when the interaction system reads it.
	// Skips the update when delta ≈ 0 (player stationary) to preserve the last facing.
	// NOTE: .add<Pausable>() must come AFTER .each() — the builder chain requires this ordering.
	auto facingUpdater = world.system<const GridMover, FacingDirection>("FarmFacingUpdater")
							 .with<PlayerControlled>()
							 .kind(flecs::PreUpdate)
							 .each([](const GridMover& mover, FacingDirection& facing) {
								 const glm::vec3 delta = mover.target - mover.origin;
								 const float len2 = delta.x * delta.x + delta.z * delta.z;
								 if (len2 < 1e-6f) return;
								 if (std::abs(delta.x) >= std::abs(delta.z)) {
									 facing.dx = (delta.x > 0.0f) ? 1 : -1;
									 facing.dz = 0;
								 }
								 else {
									 facing.dx = 0;
									 facing.dz = (delta.z > 0.0f) ? 1 : -1;
								 }
							 })
							 .add<ecs::Pausable>();
	facingUpdater.child_of(sceneRoot);

	// Pre-build the cached queries so the interaction lambda doesn't rebuild them each frame.
	auto playerQuery = world.query_builder<const tilemap::GridPosition, const FacingDirection, const PlayerInventory>()
						   .with<PlayerControlled>()
						   .build();

	auto tileQuery = world.query_builder<const tilemap::GridPosition>().with<FarmInteractable>().build();

	// FarmWorldInteraction: single dispatch seam for all player→world interactions.
	// it.world() returns a value (lightweight flecs::world wrapper around ecs_world_t*),
	// so capture as a value not a reference to avoid binding a non-const ref to a temporary.
	auto interactionSys =
		world.system("FarmWorldInteraction")
			.kind(flecs::OnUpdate)
			.run([playerQuery, tileQuery, tilemapEntity](flecs::iter& it) {
				while (it.next()) {
					flecs::world w = it.world();

					flecs::entity playerEntity;
					playerQuery.each([&](flecs::entity e,
										 const tilemap::GridPosition&,
										 const FacingDirection&,
										 const PlayerInventory&) {
						if (!playerEntity.is_valid()) playerEntity = e;
					});
					if (!playerEntity.is_valid()) return;

					const auto& gp = playerEntity.get<tilemap::GridPosition>();
					const auto& facing = playerEntity.get<FacingDirection>();
					const auto& inv = playerEntity.get<PlayerInventory>();
					const auto [tx, tz] = ComputeFacingTile(gp, facing);

					flecs::entity facingEntity;
					tileQuery.each([&](flecs::entity e, const tilemap::GridPosition& pos) {
						if (pos.x == tx && pos.z == tz && !facingEntity.is_valid()) facingEntity = e;
					});

					const std::string equippedTool = [&]() -> std::string {
						if (const auto* item = GetEquippedItem(inv)) return item->typeId;
						return {};
					}();

					const auto& registry = w.get<CropRegistry>();
					const Season season = w.has<GameTime>() ? w.get<GameTime>().season : Season::Spring;
					const auto available = DetermineInteraction(equippedTool, facingEntity, registry, season);

					auto& hint = w.get_mut<InteractionHint>();
					hint.label = available.label;
					// Show the hint whenever there is a label — this includes informational
					// hints like "Equip seeds to plant" (kind = None, label non-empty).
					hint.visible = !available.label.empty();

					const auto& input = w.get<input::InputState>();
					if (input.keys[input::KeyCode::E].pressed && available.kind != InteractionKind::None)
						ExecuteInteraction(w, playerEntity, facingEntity, tx, tz, available, tilemapEntity);
				}
			})
			.add<ecs::Pausable>();
	interactionSys.child_of(sceneRoot);

	// FarmInteractionHintHud: draws the action prompt near the bottom of the screen.
	// Not Pausable — hint remains visible while the world is paused.
	auto hintHud = world.system("FarmInteractionHintHud").kind<ecs::OnUI>().run([](flecs::iter& it) {
		while (it.next()) {
			const flecs::world w = it.world();
			if (!w.has<InteractionHint>()) return;

			const auto& hint = w.get<InteractionHint>();
			if (!hint.visible) return;

			const auto* ref = w.try_get<platform::PlatformRef>();
			if (!ref || !ref->ptr) return;
			auto* platform = ref->ptr;

			const float screenW = static_cast<float>(platform->Width());
			const float screenH = static_cast<float>(platform->Height());
			constexpr float kFontSize = 22.0F;
			constexpr core::Rgba kHintBg = {.r = 0, .g = 0, .b = 0, .a = 140};
			constexpr core::Rgba kHintColor = {.r = 255, .g = 240, .b = 100, .a = 255};

			const float textW = static_cast<float>(hint.label.size()) * kFontSize * 0.55F;
			const float textX = (screenW - textW) * 0.5F;
			const float textY = screenH - 120.0F;

			platform->DrawRect(
				{.x = textX - 8.0F, .y = textY - 4.0F, .w = textW + 16.0F, .h = kFontSize + 8.0F},
				kHintBg
			);
			platform->DrawText(hint.label, textX, textY, kFontSize, kHintColor);
		}
	});
	hintHud.child_of(sceneRoot);
}

} // namespace game::farm
