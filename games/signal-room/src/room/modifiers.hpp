#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "engine/core/core.hpp"

namespace signal_room {

// One data override: set the value at JSON Pointer `path` in the named game document.
struct ModifierOverride {
	std::string doc; // shift | stations | items | tickets | tuning
	std::string path;
	nlohmann::json set;
};

// A named set of overrides. A Shift lists the ones it wants; the Room panel can force each on or off.
struct Modifier {
	std::string name;
	std::string description;
	std::vector<ModifierOverride> overrides;
};

// World singleton loaded from the Modifier catalogue file. Ids iterate sorted: that is catalogue order.
struct ModifierCatalogue : engine::core::Registry<Modifier> {};

// An override of an active Modifier, with where it came from for error messages.
struct ActiveOverride {
	std::string modifier;
	size_t index{0};
	ModifierOverride override;
};

// The overrides to apply to the game documents this load, in order.
struct ActiveModifiers {
	std::vector<std::string> ids; // the applied Modifiers, catalogue order
	std::vector<ActiveOverride> overrides;
};

// How the dev toggle treats one Modifier. Absent from the toggle means "Shift default".
struct ModifierForce {
	std::string id;
	bool on{false};
};

// World singleton: forced Modifiers, kept across Reloads and edited in the Room panel.
struct ModifierToggle {
	std::vector<ModifierForce> forced;
};

// World singleton: the Modifiers applied by the last Room load (comma separated, for the inspector).
struct AppliedModifiers {
	std::string ids;
};

// Parse the Modifier catalogue: {"modifiers": {"<id>": {"name", "description", "overrides":
// [{"doc", "path", "set"}]}}}. The error names the Modifier, the override index and the path.
[[nodiscard]] std::expected<ModifierCatalogue, std::string> LoadModifierCatalogueFile(const std::string& path);

// The `modifiers` list of the unpatched Shift file at `shiftPath`.
[[nodiscard]] std::expected<std::vector<std::string>, std::string> ReadShiftModifierIds(const std::string& shiftPath);

// Shift's Modifiers, then the toggle, then the overrides of each active Modifier in catalogue order. An unknown
// id in either names it.
[[nodiscard]] std::expected<ActiveModifiers, std::string> ResolveActiveModifiers(
	const ModifierCatalogue& catalogue,
	const std::vector<std::string>& shiftIds,
	const ModifierToggle& toggle
);

// The one "load a game document" call: read the JSON file, then apply the active overrides aimed at
// `doc` ("shift", "stations", ...) before the caller parses it. `fileKind` is as for ReadJsonFile.
[[nodiscard]] std::expected<nlohmann::json, std::string> ReadGameDocument(
	const std::string& path,
	std::string_view fileKind,
	std::string_view doc,
	const ActiveModifiers& active
);

} // namespace signal_room
