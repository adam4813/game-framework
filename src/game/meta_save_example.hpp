#pragma once

#include <string>
#include <vector>

#include <flecs.h>

// Worked example of the JSON save system (engine::save) wired into game state, persisted through the
// Platform save API (desktop files / browser localStorage). Shows both binding styles end-to-end and
// exposes the flow to AngelScript (see assets/scripts/save_demo.as) and a small UI (save_ui.*).
namespace game {

// Persistent meta-progression state (single instance). Saved under the "meta" key via singleton
// member bindings.
struct MetaProgress {
	int parlorTokens = 0;
	int scorePoints = 0;
	int runsCompleted = 0;
};

// A permanent unlock. Entities carrying PersistTag are saved as one entry in the "upgrades" JSON
// array. The upgrade's identity is a plain data member (`id`), NOT the entity handle — entities are
// ephemeral and fully reconstructed on load, so nothing depends on runtime entity ids.
struct UnlockedUpgrade {
	std::string id; // e.g. "magnet", "phase"
	int level = 1;
};

// Save tag: marks the entities the save system should persist. Any zero-size struct works.
struct PersistTag {};

// Registers the example: singletons, component reflection (+ scripting exposure), the save schema,
// and the script-facing global verbs (SaveGame/LoadGame/DeleteSave/AddParlorTokens/…). Call once
// during game initialisation, after the engine modules (SaveModule + ScriptingModule) are up.
void RegisterMetaSaveExample(const flecs::world& world);

// Slot-based save API shared by the UI and the script verbs. Storage goes through the Platform save
// API, so a slot is a file on desktop and a localStorage entry on the web.
std::string SaveGameToSlot(const flecs::world& world, const std::string& name); // serialize -> slot; returns name
bool LoadGameFromSlot(const flecs::world& world, const std::string& name);      // false if the slot is missing
void DeleteSaveSlot(const flecs::world& world, const std::string& name);
[[nodiscard]] std::vector<std::string> ListSaveSlots(const flecs::world& world);

} // namespace game
