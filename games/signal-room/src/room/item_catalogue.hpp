#pragma once

#include <expected>
#include <string>

#include "engine/core/core.hpp"

namespace signal_room {

struct GreyboxCatalogue;

// One Item kind: `shape` keys the greybox catalogue's `items` models, `carrySpeed` scales the
// carrier's walk and dash, and `tiltToCamera` tilts it toward the camera while carried (cards).
struct ItemKind {
	std::string shape;
	float carrySpeed{1.0F};
	bool tiltToCamera{false};
};

// World singleton loaded from the Item catalogue file, keyed by Item id. A new Item is a catalogue
// entry (and a greybox shape) and no C++ change.
struct ItemCatalogue : engine::core::Registry<ItemKind> {};

// Parse the Item catalogue file: {"items": {"<id>": {"shape": key, "carry_speed": 1.0,
// "tilt_to_camera": false}}}. Every shape must be one of `greybox`'s Item models, and carry_speed a
// positive number. The error names the file and the bad Item.
[[nodiscard]] std::expected<ItemCatalogue, std::string>
LoadItemCatalogueFile(const std::string& path, const GreyboxCatalogue& greybox);

} // namespace signal_room
