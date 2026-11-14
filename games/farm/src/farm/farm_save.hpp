#pragma once

#include <string>

#include <flecs.h>

namespace game::farm {

/// Number of farm save slots exposed in the title menu's slot picker.
inline constexpr int kFarmSlotCount = 3;

/// Farm world identity + runtime scene binding.
///
/// The reflected/persisted members (seed, playerX, playerZ) lead the struct because the Flecs meta
/// registration auto-computes their offsets sequentially from 0; the runtime-only members (slot,
/// load_requested) trail and are neither reflected nor serialized. They tell FarmWorldScene which
/// slot it is bound to and whether to restore it from disk (load_requested = true) or build the
/// world handed to it via the CurrentWorld singleton (false). See games/farm/docs/adr/0001.
struct FarmSession {
	int seed = 0;                ///< world-gen seed; 0 = not yet generated
	int playerX = -1;            ///< last known player grid X; -1 = derive from starting village
	int playerZ = -1;            ///< last known player grid Z
	int slot = -1;               ///< bound save-slot index (0-based), or -1 when unbound (runtime only)
	bool load_requested = false; ///< true => restore `slot` from disk; false => new game (runtime only)
};

/// Register Flecs reflection and save-registry bindings for FarmSession, GameTime,
/// and farm grid state (TilledSoil / Crop / GrowthStage).
/// Requires save::SaveRegistry (imported via engine's SaveModule). Call once, after
/// RegisterGameTime and RegisterPlayerInventory.
void RegisterFarmSave(flecs::world& world);

/// Platform save-slot key for a 0-based slot index (e.g. 0 -> "farm_slot_1").
[[nodiscard]] std::string FarmSlotName(int slot);

/// Whether a save exists on disk for the given 0-based slot (false for out-of-range).
[[nodiscard]] bool FarmSlotOccupied(const flecs::world& world, int slot);

/// Serialize the current farm world to `slot`: seed + params from the CurrentWorld singleton plus
/// the registry state (GameTime, player position, tilled soil). Updates the last-played pointer.
/// Returns the slot name, or "" on failure.
std::string SaveFarmSlot(const flecs::world& world, int slot);

/// Read the raw save JSON stored for `slot`, or "" when the slot is empty/out-of-range.
[[nodiscard]] std::string ReadFarmSlotRaw(const flecs::world& world, int slot);

/// Delete the save on disk for `slot` (no-op if empty/out-of-range). Clears the last-played pointer
/// when it referenced the deleted slot.
void DeleteFarmSlot(const flecs::world& world, int slot);

/// Record `slot` as the most recently played slot (used by the title's Continue button).
void WriteLastPlayedSlot(const flecs::world& world, int slot);

/// The most recently played, still-occupied slot index, or -1 when there is none.
[[nodiscard]] int ReadLastPlayedSlot(const flecs::world& world);

} // namespace game::farm
