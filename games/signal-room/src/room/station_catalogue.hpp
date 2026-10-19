#pragma once

#include <expected>
#include <string>
#include <vector>

#include "engine/core/core.hpp"
#include "modifiers.hpp"

namespace signal_room {

struct ItemCatalogue;

// How a Transform's Job advances (see station_job.hpp):
// - Instant:  completes the moment its input set is complete.
// - Tap:      `taps` empty-handed presses complete it.
// - Hold:     advances only while an Operator Holds the action key at the Station, for `duration` s.
// - WalkAway: runs by itself for `duration` s once its input set is complete; an Operator Holding it
//             runs it at duration / crankDuration times that speed (when crankDuration > 0).
enum class Verb { Instant, Tap, Hold, WalkAway };

// One thing a Station kind can do: turn `inputs` (placed one at a time) into `outputs` (all picked up
// before the next Job). Either list may be empty. A `transmit` Transform sends a transmission on
// completion; it consumes its inputs and has no outputs.
struct StationTransform {
	std::string id;
	std::vector<std::string> inputs;  // Item ids
	std::vector<std::string> outputs; // Item ids
	Verb verb{Verb::Instant};
	float duration{0.0F};      // s, for Hold and WalkAway
	int taps{1};               // presses, for Tap
	float crankDuration{0.0F}; // s while cranked, for WalkAway (0 = cannot be cranked)
	bool transmit{false};
};

// One Station kind: its name for Hints and Bubbles, and its Transforms. Each input set (as a
// multiset of Item ids) maps to exactly one Transform, so placing an Item is deterministic.
struct StationKind {
	std::string displayName;
	std::vector<StationTransform> transforms;
};

// World singleton loaded from the Station catalogue file, keyed by Station kind (the same keys as the
// greybox catalogue's `stations`). A new machine or route change is a catalogue entry and no C++. The
// Room panel edits durations live.
struct StationCatalogue : engine::core::Registry<StationKind> {};

// Parse the Station catalogue file: {"stations": {"<kind>": {"display_name": text, "transforms":
// [{"id", "inputs", "outputs", "verb", "duration" | "taps" | "crank_duration", "transmit"}]}}}. The
// error names the file and the kind and Transform: an unknown Item id or verb, a missing or
// non-positive number, or two Transforms of one kind sharing an input set.
[[nodiscard]] std::expected<StationCatalogue, std::string>
LoadStationCatalogueFile(const std::string& path, const ItemCatalogue& items, const ActiveModifiers& modifiers = {});

} // namespace signal_room
