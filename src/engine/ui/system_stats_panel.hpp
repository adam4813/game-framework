#pragma once

#include <deque>
#include <string>
#include <unordered_map>

#include <flecs.h>

#include "engine/core/core.hpp"

// A reusable, retained-mode debug overlay that renders per-system Flecs timings (from the built-in
// stats addon) as a scrollable, collapsible tree. Systems and observers are grouped under the
// module that owns them; each module is a collapsible header button (all start collapsed) that
// rolls up the total CPU time and the system/observer counts of its children, and expands to a
// right-aligned, per-system/observer timing line.
//
// Usage from any scene:
//   const auto stats = engine::ui::CreateSystemStatsPanel(world, {.x=20, .y=20, .w=420, .h=420})
//                          .child_of(sceneUiRoot);
//
// The panel measures per-system time via ecs_system_stats_get (which requires system-time
// measurement, enabled once by RegisterSystemStatsPanelSystems). Timings are sampled every
// `refresh_frames` frames and the number labels updated in place; the entity tree is only rebuilt
// when its structure changes (a module collapses/expands, or the system set changes), so clicks are
// never dropped by churn.
namespace engine::ui {

// Construction-time configuration for CreateSystemStatsPanel.
struct SystemStatsPanelOptions {
	int refresh_frames{5};           // sample timings every N frames (drawing happens every frame)
	int average_window{10};          // rolling-average window (samples) applied to each timing
	bool include_observers{true};    // list observers (untimed) alongside systems
	bool hide_flecs_internals{true}; // skip flecs' own modules (no reflection, ~0ms, just noise)
	float row_height{18.0F};         // pixel height of each header/row
	float font_size{15.0F};          // label/button font size
};

// Retained state singleton-per-panel. Attached to the panel's ScrollRect entity by the factory and
// driven by UISystemStatsUpdate — do not author by hand. Holds refresh bookkeeping, the per-module
// collapse state, the running timing samples, and handles to the number labels updated in place.
struct SystemStatsPanel {
	// === Config (copied from SystemStatsPanelOptions) ===
	int refresh_frames{5};
	int average_window{10};
	bool include_observers{true};
	bool hide_flecs_internals{true};
	float row_height{18.0F};
	float font_size{15.0F};

	// === Runtime state (written by UISystemStatsUpdate) ===
	int frame_counter{0};        // frames since last sample
	int64_t last_frame_count{0}; // world frame count at last sample (for per-frame averaging)
	bool dirty{true};            // force a refresh next update (e.g. after a collapse toggle)
	flecs::entity content;       // parent the header/row entities are created under (the panel itself)
	std::string signature;       // structure hash of the last build; a change triggers a full rebuild

	// Module path -> collapsed. Keyed by full path so identically-named nested modules don't clash.
	std::unordered_map<std::string, bool> collapsed;
	// System entity id -> cumulative time_spent (seconds) at last sample, for delta computation.
	std::unordered_map<uint64_t, double> prev_time;
	// System entity id -> the last `average_window` per-frame samples (ms), for the rolling average.
	std::unordered_map<uint64_t, std::deque<double>> history;
	// System entity id -> rolling-average per-frame time (milliseconds), the value that is displayed.
	std::unordered_map<uint64_t, double> last_ms;
	// System entity id -> severity of the window: 0 = steady, 1 = variable (orange), 2 = spiking (red).
	std::unordered_map<uint64_t, int> severity;
	// System entity id -> matched entity count at last sample (from the query stats), for correlating
	// timing with the amount of work processed (hotspot spotting: high ms + low entities = expensive
	// per entity).
	std::unordered_map<uint64_t, double> entities;
};

/// Create a scrollable, collapsible per-system timing panel occupying `rect`. Returns the panel's
/// ScrollRect entity (parent it into a scene UI root). Registers the driving systems on first use.
flecs::entity
CreateSystemStatsPanel(const flecs::world& world, core::Rect rect, const SystemStatsPanelOptions& options = {});

/// Register the systems that drive every SystemStatsPanel and enable Flecs system-time measurement.
/// Call exactly once during engine startup (EngineContext does this after importing modules).
void RegisterSystemStatsPanelSystems(const flecs::world& world);

} // namespace engine::ui
