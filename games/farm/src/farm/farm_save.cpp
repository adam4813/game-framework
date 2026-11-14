#include "farm_save.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "engine/engine.hpp"
#include "farm_components.hpp"
#include "farm_world_gen.hpp"
#include "game_time.hpp"
#include "player_inventory.hpp"

using namespace engine;

namespace game::farm {
namespace {

// Platform slot holding the index of the most recently played farm slot (for the Continue button).
constexpr const char* kLastPlayedKey = "farm_last";

[[nodiscard]] bool SlotInRange(const int slot) { return slot >= 0 && slot < kFarmSlotCount; }

} // namespace

void RegisterFarmSave(flecs::world& world) {
	if (!world.has<save::SaveRegistry>()) return;

	world.component<FarmSession>().member<int>("seed").member<int>("playerX").member<int>("playerZ");

	if (!world.has<FarmSession>()) world.add<FarmSession>();

	auto& reg = world.get_mut<save::SaveRegistry>();

	// ── FarmSession: world-gen seed + last player grid position ──────────────
	reg.Singleton<FarmSession>("farm")
		.Member("seed", &FarmSession::seed)
		.Member("playerX", &FarmSession::playerX)
		.Member("playerZ", &FarmSession::playerZ);

	// ── GameTime: in-game clock ────────────────────────────────────────────────
	// RegisterGameTime registers reflection only; save bindings live here.
	reg.Singleton<GameTime>("time")
		.Member("tick", &GameTime::tick)
		.Member("day", &GameTime::day)
		.Member("hour", &GameTime::hour)
		.Member("absoluteDay", &GameTime::absoluteDay)
		.Member("season", &GameTime::season)
		.Member("phase", &GameTime::phase);

	// ── Farm grid: TilledSoil / Crop / GrowthStage entities ───────────────────
	// Uses a Field escape hatch so entity lifecycle stays under scene control.
	// TilledSoil entities are NOT children of sceneRoot_; FarmWorldScene::Unload()
	// destroys them before destructing sceneRoot_, and Load() cleans up any
	// leftovers from a previous session at the top.
	reg.Field("farm.tiles")
		.Get([](const flecs::world& w) -> save::Json {
			save::Json arr = save::Json::array();
			w.query_builder<const TilledSoil, const tilemap::GridPosition>().build().each(
				[&arr](flecs::entity e, const TilledSoil& soil, const tilemap::GridPosition& gp) {
					save::Json entry;
					entry["x"] = gp.x;
					entry["z"] = gp.z;
					entry["daysEmpty"] = soil.daysEmpty;
					if (const auto* crop = e.try_get<Crop>()) {
						entry["crop"] = save::Json{
							{"typeId", crop->typeId},
							{"watered", crop->watered},
						};
					}
					if (const auto* gs = e.try_get<GrowthStage>()) {
						entry["growth"] = save::Json{
							{"stage", static_cast<int>(gs->stage)},
							{"daysAtStage", gs->daysAtStage},
						};
					}
					arr.push_back(std::move(entry));
				}
			);
			return arr;
		})
		.Set([](flecs::world& w, const save::Json& j) {
			if (!j.is_array()) return;

			// Collect before destroying to avoid mutating an active iterator.
			std::vector<flecs::entity> toDestroy;
			w.query_builder<TilledSoil>().build().each([&toDestroy](flecs::entity e, TilledSoil&) {
				toDestroy.push_back(e);
			});
			for (auto e : toDestroy) e.destruct();

			for (const auto& entry : j) {
				auto e = w.entity()
							 .set<TilledSoil>({.daysEmpty = entry.value("daysEmpty", 0)})
							 .set<tilemap::GridPosition>({
								 .x = entry.value("x", 0),
								 .z = entry.value("z", 0),
							 })
							 .add<FarmInteractable>();
				if (entry.contains("crop")) {
					const auto& c = entry.at("crop");
					e.set<Crop>({
						.typeId = c.value("typeId", ""),
						.watered = c.value("watered", false),
					});
				}
				if (entry.contains("growth")) {
					const auto& g = entry.at("growth");
					e.set<GrowthStage>({
						.stage = static_cast<Stage>(g.value("stage", 0)),
						.daysAtStage = g.value("daysAtStage", 0),
					});
				}
			}
		});

	engine::ecs::Log(world).info("[FarmSave] Registered farm world save bindings");
}

std::string FarmSlotName(const int slot) { return "farm_slot_" + std::to_string(slot + 1); }

bool FarmSlotOccupied(const flecs::world& world, const int slot) {
	if (!SlotInRange(slot)) return false;
	const auto* ref = world.try_get<platform::PlatformRef>();
	if (!ref || !ref->ptr) return false;
	return ref->ptr->HasSave(FarmSlotName(slot));
}

std::string ReadFarmSlotRaw(const flecs::world& world, const int slot) {
	if (!SlotInRange(slot)) return {};
	const auto* ref = world.try_get<platform::PlatformRef>();
	if (!ref || !ref->ptr || !ref->ptr->HasSave(FarmSlotName(slot))) return {};
	return ref->ptr->ReadSave(FarmSlotName(slot));
}

std::string SaveFarmSlot(const flecs::world& world, const int slot) {
	if (!SlotInRange(slot)) return {};
	const auto* ref = world.try_get<platform::PlatformRef>();
	if (!ref || !ref->ptr) return {};

	// Start from the registry's serialization (FarmSession seed/player pos, GameTime, farm.tiles),
	// then inject the world identity (seed + full params) from the authoritative CurrentWorld so a
	// cold load can reconstruct the exact world deterministically (see games/farm/docs/adr/0001).
	nlohmann::json doc = nlohmann::json::parse(save::SaveToString(world), nullptr, /*allow_exceptions=*/false);
	if (doc.is_discarded() || !doc.is_object()) doc = nlohmann::json::object();
	if (const auto* cw = world.try_get<CurrentWorld>()) {
		doc["farm"]["seed"] = cw->world.seed;
		doc["farm"]["params"] = WorldGenParamsToJson(cw->world.params);
	}

	const std::string name = FarmSlotName(slot);
	ref->ptr->WriteSave(name, doc.dump());
	WriteLastPlayedSlot(world, slot);
	engine::ecs::Log(world).info("[FarmSave] Saved farm world to slot '{}'", name);
	return name;
}

void DeleteFarmSlot(const flecs::world& world, const int slot) {
	if (!SlotInRange(slot)) return;
	const auto* ref = world.try_get<platform::PlatformRef>();
	if (!ref || !ref->ptr) return;
	const bool wasLastPlayed = ReadLastPlayedSlot(world) == slot; // check while the slot is still occupied
	ref->ptr->DeleteSave(FarmSlotName(slot));
	if (wasLastPlayed) ref->ptr->DeleteSave(kLastPlayedKey);
	engine::ecs::Log(world).info("[FarmSave] Deleted farm world save slot '{}'", FarmSlotName(slot));
}

void WriteLastPlayedSlot(const flecs::world& world, const int slot) {
	if (!SlotInRange(slot)) return;
	const auto* ref = world.try_get<platform::PlatformRef>();
	if (!ref || !ref->ptr) return;
	ref->ptr->WriteSave(kLastPlayedKey, nlohmann::json{{"slot", slot}}.dump());
}

int ReadLastPlayedSlot(const flecs::world& world) {
	const auto* ref = world.try_get<platform::PlatformRef>();
	if (!ref || !ref->ptr || !ref->ptr->HasSave(kLastPlayedKey)) return -1;
	const nlohmann::json doc =
		nlohmann::json::parse(ref->ptr->ReadSave(kLastPlayedKey), nullptr, /*allow_exceptions=*/false);
	if (doc.is_discarded() || !doc.is_object()) return -1;
	const int slot = doc.value("slot", -1);
	return (SlotInRange(slot) && FarmSlotOccupied(world, slot)) ? slot : -1;
}

} // namespace game::farm
