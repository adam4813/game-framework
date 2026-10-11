#include "system_stats_panel.hpp"

#include <algorithm>
#include <cstdio>
#include <deque>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <flecs.h>

#include "engine/core/core.hpp"
#include "engine/input/input.hpp"
#include "engine/platform/platform.hpp"
#include "engine/profiling/profiling.hpp"
#include "engine/render/render.hpp"
#include "ui_components.hpp"
#include "ui_events.hpp"
#include "ui_factories.hpp"

// ─────────────────────────────────────────────────────────────────────────────────────────────
// FUTURE WORK — a cleaner, more ECS-idiomatic architecture for this panel.
//
// Today the panel is driven by a single task system (UISystemStatsUpdate, OnLoad) that both
// (a) rebuilds the entity tree on a structural change and (b) refreshes values in place. Collapsing a
// module rebuilds its subtree because layout is manual (explicit Y) and a full rebuild is the simplest
// reflow. It works, but it fuses layout, data, and entity lifecycle into one imperative pass.
//
// A better split (per @adam4813) that lets the stock UI systems do the heavy lifting:
//   • OnLoad    — (re)build the tree structure ONLY when the system set changes (scene load, etc.).
//   • PreUpdate — mutate per-element components (collapse -> UIElement.visible, values -> Label.text);
//                 then the existing UI layout / scroll / render systems pick the changes up for free.
//
// Concretely, TODO:
//   1. Retain every row once and collapse by toggling UIElement.visible instead of destroying
//      entities, so a collapse never churns entities — this removes the dropped-click and
//      stale-handle bug classes at the source. It needs a layout pass that reflows Y while skipping
//      hidden rows (a Stack variant or a small custom system) AND an explicit per-element
//      layout/draw-order key, so we can drop the fragile "create Buttons before Labels so archetype
//      iteration draws them under the text" trick.
//   2. Move per-system timing (rolling window + severity) off the panel and onto the system entities
//      (or a world singleton) so module rollups and multiple panels share one source of truth, and a
//      real query — world.query<Label>().with<StatBoundTo>(flecs::Wildcard) — can refresh every bound
//      label without walking children by hand.
//   3. Generalise StatBoundTo + the value refresh into a reusable "data-bound label" for the UI module
//      (bind a Label to a source entity + a formatter), so other overlays get the wiring for free.
//   4. Cost: UISystemStatsUpdate calls ecs_system_stats_get() for every system each sample. For large
//      worlds, sample incrementally (a slice of systems per frame) or widen refresh_frames.
//   5. Wall-clock frame contribution: the panel currently shows cumulative CPU time (sum across all
//      worker threads) per system, not wall-clock frame impact. A per-system wall_ms = max(end) -
//      min(start) across all worker invocations would enable a real frame-budget pie chart (e.g.
//      "PathRequestSystem takes X ms of the 16 ms frame"). Requires converting multi-threaded systems
//      from .each() to .run() and recording per-worker start/end with atomic min/max, then exposing
//      the result as a world singleton or per-system-entity component the panel can query.
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace engine::ui {

namespace {

// Relationship: a value label displays the live timing of its target. Exclusive — a label shows one
// source. The target is the flecs System entity for a per-system row, or the module entity (the panel
// entity itself standing in for the module-less "(root)" bucket) for a module rollup header. Mirrors
// the engine's other first-class links (RenderWith, LookAt): the label is authored with
// `label.add<StatBoundTo>(source)` and the refresh reads `label.target<StatBoundTo>()`.
struct StatBoundTo {};

// The render-stats summary lines have no source entity — they display an external line by index.
struct SummaryBinding {
	int index{0};
};

// Persistent (world-singleton) telemetry recorder. `enabled` is flipped by the panel's checkbox and
// lives on the world, so it survives scene changes. While enabled, the sampler appends one JSON-lines
// row per system per sample to `file` — a format any web page can stream and plot. The stream is held
// by shared_ptr so the singleton stays trivially movable/copyable for flecs.
struct StatsTelemetry {
	bool enabled{false};
	std::string path{"stats-telemetry.jsonl"};
	std::shared_ptr<std::ofstream> file;
};

// One expanded line: a system or observer plus its measured per-frame time (ms).
struct StatRow {
	flecs::entity entity;
	std::string name;
	double ms{0.0};
	bool observer{false};
};

// A module rollup: the module entity it aggregates (invalid for the "(root)" bucket) and its rows.
struct StatGroup {
	flecs::entity module_entity;
	std::string display; // leaf module name shown in the header
	int system_count{0};
	int observer_count{0};
	std::vector<StatRow> rows;
};

// Module names longer than this are ellipsis-truncated in the header (e.g. SceneManagementModule).
constexpr size_t kMaxModuleNameChars = 15;

// Nearest ancestor tagged flecs::Module. Returns {module entity, full-path key, leaf display name};
// systems that live outside any module (e.g. scene-local systems created at the root scope) fall back
// to an invalid entity with the "(root)" key.
struct ModuleInfo {
	flecs::entity entity;
	std::string key;
	std::string display;
};

ModuleInfo ModuleOf(const flecs::entity entity) {
	for (flecs::entity parent = entity.parent(); parent; parent = parent.parent()) {
		if (parent.has(flecs::Module)) {
			return {parent, std::string(parent.path().c_str()), std::string(parent.name().c_str())};
		}
	}
	return {flecs::entity{}, "(root)", "(root)"};
}

// Clip `text` to `max_chars` glyphs, ending in "..." when truncated, so wide module names don't blow
// out the panel width.
std::string Truncate(const std::string& text, const size_t max_chars) {
	if (text.size() <= max_chars) {
		return text;
	}
	if (max_chars <= 3) {
		return text.substr(0, max_chars);
	}
	return text.substr(0, max_chars - 3) + "...";
}

// flecs' own modules (pipeline, timer, stats, rest, ...) all live under the "flecs" scope. They have
// no reflection and report ~0ms, so they are hidden by default.
bool ModuleHidden(const std::string& key, const SystemStatsPanel& panel) {
	return panel.hide_flecs_internals && key.find("flecs") != std::string::npos;
}

// Displayed collapse state, defaulting to collapsed for never-seen modules.
bool IsCollapsed(const SystemStatsPanel& panel, const std::string& key) {
	const auto found = panel.collapsed.find(key);
	return found == panel.collapsed.end() ? true : found->second;
}

double GroupTotalMs(const StatGroup& group) {
	double total = 0.0;
	for (const StatRow& row : group.rows) {
		total += row.ms;
	}
	return total;
}

void FormatTime(char* buffer, const size_t size, const double ms, const bool observer) {
	if (observer) {
		std::snprintf(buffer, size, "-");
	}
	else {
		std::snprintf(buffer, size, "%.3f ms", ms);
	}
}

// Compact entity/work count: "1.2k" / "3.4M" for readability in the narrow value column.
std::string CompactCount(const double count) {
	char buffer[16];
	if (count >= 1'000'000.0) {
		std::snprintf(buffer, sizeof(buffer), "%.1fM", count / 1'000'000.0);
	}
	else if (count >= 1'000.0) {
		std::snprintf(buffer, sizeof(buffer), "%.1fk", count / 1'000.0);
	}
	else {
		std::snprintf(buffer, sizeof(buffer), "%.0f", count);
	}
	return buffer;
}

// Classify a rolling window: 2 = spiking (a sample far above the average), 1 = variable (noisy but
// not spiking), 0 = steady. Tiny timings are always steady so sub-millisecond jitter is not flagged.
int ComputeSeverity(const double avg, const double max_val, const double stddev, const size_t count) {
	constexpr double floor_ms = 0.05;    // ignore everything below this — noise
	constexpr double spike_factor = 3.0; // peak this many times the average => spike
	constexpr double var_coeff = 0.5;    // stddev/avg above this => variable
	if (count < 2 || avg <= 0.0 || max_val < floor_ms) {
		return 0;
	}
	if (max_val > avg * spike_factor) {
		return 2;
	}
	if (stddev / avg > var_coeff) {
		return 1;
	}
	return 0;
}

// Map a severity (and observer flag) to the displayed value colour.
core::Rgba SeverityColor(const int severity, const bool observer) {
	if (observer) {
		return core::colors::Subtle;
	}
	switch (severity) {
	case 2: return core::colors::Hazard; // red — spiking
	case 1: return core::colors::Boost;  // orange — variable
	default: return core::colors::Text;
	}
}

// Worst severity among a module's timed systems, so a collapsed header still flags trouble inside.
int GroupSeverity(const SystemStatsPanel& panel, const StatGroup& group) {
	int worst = 0;
	for (const StatRow& row : group.rows) {
		if (row.observer) {
			continue;
		}
		if (const auto found = panel.severity.find(row.entity.id()); found != panel.severity.end()) {
			worst = std::max(worst, found->second);
		}
	}
	return worst;
}

// Sample the cumulative per-system time from the stats addon, convert it to a per-frame millisecond
// value, push it through a rolling window, and record the window's average + spike/variability class.
// Also captures the matched entity count (for hotspot correlation) and, when telemetry is enabled,
// appends a JSON-lines row per system.
void SampleTimings(
	const flecs::world& world,
	const flecs::entity panel_entity,
	SystemStatsPanel& panel,
	const flecs::query<>& systems
) {
	ecs_world_t* raw = world.c_ptr();
	const ecs_world_info_t* info = ecs_get_world_info(raw);
	const int64_t cur_frame = info->frame_count_total;
	const double world_time = info->world_time_total;
	const auto frames = static_cast<double>(std::max<int64_t>(1, cur_frame - panel.last_frame_count));
	panel.last_frame_count = cur_frame;
	const auto window_size = static_cast<size_t>(std::max(1, panel.average_window));

	// Use get_mut so flecs tracks the write and the shared_ptr stream is properly accessible.
	const auto* telemetry = world.try_get_mut<StatsTelemetry>();
	const bool emit = telemetry != nullptr && telemetry->enabled && telemetry->file && telemetry->file->is_open();

	systems.each([&](const flecs::entity system) {
		ecs_system_stats_t stats = {};
		if (!ecs_system_stats_get(raw, system.id(), &stats)) {
			return;
		}
		const int32_t t = stats.query.t;
		const double cumulative = stats.time_spent.counter.value[t];
		const uint64_t id = system.id();
		// First time we see a system, seed prev == cumulative so its first delta is zero (avoids a
		// spurious "time since program start" spike on the first sample).
		const auto found = panel.prev_time.find(id);
		const double prev = found == panel.prev_time.end() ? cumulative : found->second;
		const double delta = std::max(0.0, cumulative - prev);
		panel.prev_time[id] = cumulative;
		const double sample_ms = (delta / frames) * 1000.0;

		std::deque<double>& window = panel.history[id];
		window.push_back(sample_ms);
		while (window.size() > window_size) {
			window.pop_front();
		}

		double sum = 0.0;
		double max_val = 0.0;
		for (const double value : window) {
			sum += value;
			max_val = std::max(max_val, value);
		}
		const double avg = sum / static_cast<double>(window.size());
		double variance = 0.0;
		for (const double value : window) {
			const double diff = value - avg;
			variance += diff * diff;
		}
		variance /= static_cast<double>(window.size());

		const double entity_count = stats.query.matched_entity_count.gauge.avg[t];
		panel.last_ms[id] = avg;
		panel.severity[id] = ComputeSeverity(avg, max_val, std::sqrt(variance), window.size());
		panel.entities[id] = entity_count;

		if (emit) {
			const double table_count = stats.query.matched_table_count.gauge.avg[t];
			*telemetry->file
				<< "{\"frame\":"
				<< cur_frame
				<< ",\"t\":"
				<< world_time
				<< ",\"system\":\""
				<< system.path().c_str()
				<< "\",\"ms\":"
				<< avg
				<< ",\"entities\":"
				<< entity_count
				<< ",\"tables\":"
				<< table_count
				<< "}\n";
		}

		// Per-system timing pushed into the panel entity's ProfileStats for the ProfilerModule to emit.
		// Module display name (e.g. "RenderModule") is used as module_key so tracy.json can gate it.
		const bool flecs_internal = panel.hide_flecs_internals
									&& std::string_view(system.path().c_str()).find("flecs") != std::string_view::npos;
		if (!flecs_internal) {
			const ModuleInfo module_info = ModuleOf(system);
			if (auto* profile = panel_entity.try_get_mut<profiling::ProfileStats>()) {
				const std::string sys_name(system.name().c_str());
				profile->Plot(module_info.display, sys_name + ".ms", avg);
				profile->Plot(module_info.display, sys_name + ".ent", entity_count);
			}
		}
	});

	if (emit) {
		telemetry->file->flush();
	}
}

// Collect the (filtered) modules and their rows from the current system/observer set and the most
// recent timing sample. Rows are name-sorted so both the tree order and the signature are stable.
std::map<std::string, StatGroup>
GatherGroups(const SystemStatsPanel& panel, const flecs::query<>& systems, const flecs::query<>& observers) {
	std::map<std::string, StatGroup> groups;
	const auto accumulate = [&](const flecs::entity entity, const bool observer) {
		ModuleInfo module = ModuleOf(entity);
		if (ModuleHidden(module.key, panel)) {
			return;
		}
		StatGroup& group = groups[module.key];
		group.module_entity = module.entity;
		group.display = std::move(module.display);
		double ms = 0.0;
		if (const auto found = panel.last_ms.find(entity.id()); !observer && found != panel.last_ms.end()) {
			ms = found->second;
		}
		if (observer) {
			++group.observer_count;
		}
		else {
			++group.system_count;
		}
		group.rows.push_back(
			{.entity = entity, .name = std::string(entity.name().c_str()), .ms = ms, .observer = observer}
		);
	};

	systems.each([&](const flecs::entity system) { accumulate(system, false); });
	if (panel.include_observers) {
		observers.each([&](const flecs::entity observer) { accumulate(observer, true); });
	}

	for (auto& group : groups | std::views::values) {
		std::ranges::sort(group.rows, [](const StatRow& a, const StatRow& b) { return a.name < b.name; });
	}
	return groups;
}

// The render-stats lines shown at the top of the panel, read straight from the render module's
// RenderStats singleton plus the platform frame rate. Empty when the render module isn't present.
std::vector<std::string> BuildRenderSummary(const flecs::world& world) {
	std::vector<std::string> lines;
	const auto* stats = world.try_get<render::RenderStats>();
	if (stats == nullptr) {
		return lines;
	}
	int fps = 0;
	if (const auto* platform_ref = world.try_get<platform::PlatformRef>();
		platform_ref != nullptr && platform_ref->ptr != nullptr) {
		fps = platform_ref->ptr->Fps();
	}
	char counts[160];
	std::snprintf(
		counts,
		sizeof(counts),
		"FPS %d   enqueued %d  culled %d   draws %d  inst %d",
		fps,
		stats->queued,
		stats->culled,
		stats->draw_calls,
		stats->instances
	);
	lines.emplace_back(counts);
	char timings[160];
	std::snprintf(
		timings,
		sizeof(timings),
		"render CPU: enq %.2f  grp %.2f  draw %.2f ms",
		stats->enqueue_ms,
		stats->group_ms,
		stats->draw_ms
	);
	lines.emplace_back(timings);
	return lines;
}

// A string that changes whenever the *structure* of the tree changes (summary-line count, module
// set, collapse state, or the systems within an expanded module) — but not when only values change.
std::string BuildSignature(
	const SystemStatsPanel& panel,
	const std::map<std::string, StatGroup>& groups,
	const size_t summary_count
) {
	std::string signature = std::to_string(summary_count) + ";;";
	for (const auto& [key, group] : groups) {
		const bool collapsed = IsCollapsed(panel, key);
		signature += key;
		signature += collapsed ? "|c|" : "|e|";
		signature += std::to_string(group.system_count);
		signature += '/';
		signature += std::to_string(group.observer_count);
		if (!collapsed) {
			for (const StatRow& row : group.rows) {
				signature += ',';
				signature += std::to_string(row.entity.id());
			}
		}
		signature += ';';
	}
	return signature;
}

// Column geometry shared by build + layout.
struct Columns {
	float x_left{0.0F};
	float full_w{0.0F};
	float name_w{0.0F};
	float row_name_w{0.0F};
	float time_x{0.0F};
	float time_w{0.0F};
};

Columns ComputeColumns(const flecs::entity panel_entity) {
	const core::Rect view = panel_entity.get<UIRect>().rect;
	const float scrollbar = panel_entity.get<ScrollRect>().scrollbar_thickness;
	constexpr float pad = 4.0F;
	constexpr float gap = 6.0F;
	constexpr float indent = 14.0F;
	Columns c;
	c.time_w = 120.0F; // wide enough for "0.123 ms  1.2k"
	c.x_left = view.x + pad;
	const float right = view.x + view.w - scrollbar - pad;
	c.full_w = std::max(0.0F, right - c.x_left);
	c.time_x = right - c.time_w;
	c.name_w = std::max(0.0F, c.time_x - gap - c.x_left);
	c.row_name_w = std::max(0.0F, c.time_x - gap - (c.x_left + indent));
	return c;
}

// Destroy the retained tree and recreate it from `groups`, repopulating the in-place number-label
// handles. Header = a full-width Button (click target) with a left name label and a right total-ms
// label composited on top; each expanded row is a left name label + a right time label.
void BuildStructure(
	const flecs::world& world,
	const flecs::entity panel_entity,
	SystemStatsPanel& panel,
	const std::map<std::string, StatGroup>& groups,
	const std::vector<std::string>& summary
) {
	panel.content.children([](const flecs::entity child) { child.destruct(); });

	const Columns col = ComputeColumns(panel_entity);
	const core::Rect view = panel_entity.get<UIRect>().rect;
	constexpr float pad = 4.0F;
	constexpr float spacing = 2.0F;
	const float row_step = panel.row_height + spacing;

	// The telemetry checkbox is the very first child (a Button), which keeps the Button archetype table
	// ahead of the Label table so buttons draw *under* the text labels composited on them (see below).
	// It uses its own centred label (no overlay), so it needs no special handling. Toggling it flips
	// the persistent StatsTelemetry singleton and requests a rebuild so the label reflects the state.
	const bool recording = [&] {
		const auto* telemetry = world.try_get<StatsTelemetry>();
		return telemetry != nullptr && telemetry->enabled;
	}();
	std::ignore =
		world.entity()
			.set<UIRect>({{.x = col.x_left, .y = view.y + pad, .w = col.full_w, .h = panel.row_height}})
			.set<Button>({
				.label = recording ? "[x] Recording telemetry" : "[ ] Record telemetry",
				.font_size = panel.font_size,
				.normal = recording ? core::colors::Accent : core::colors::Panel,
				.hover = core::colors::PanelHi,
				.pressed_color = core::colors::PanelBg,
			})
			.set<OnClick>({[panel_entity](const flecs::entity clicked) {
				auto& telemetry = clicked.world().get_mut<StatsTelemetry>();
				telemetry.enabled = !telemetry.enabled;
				if (telemetry.enabled && (!telemetry.file || !telemetry.file->is_open())) {
					telemetry.file = std::make_shared<std::ofstream>(telemetry.path, std::ios::out | std::ios::trunc);
				}
				panel_entity.get_mut<SystemStatsPanel>().dirty = true; // rebuild so the label reflects the state
			}})
			.child_of(panel.content);

	// Modules are built next so a header Button precedes any Label — Flecs children() (which drives
	// layout + draw order) iterates grouped by archetype in table-creation order, so buttons stay ahead
	// of (draw under) the text labels composited on them. The checkbox row and the full-width summary
	// lines are positioned at the top via explicit Y (layout is manual, not creation order).
	float cursor = view.y + pad + row_step + static_cast<float>(summary.size()) * row_step;

	for (const auto& [key, group] : groups) {
		const bool collapsed = panel.collapsed.try_emplace(key, true).first->second;
		const double total = GroupTotalMs(group);
		const std::string display = Truncate(group.display, kMaxModuleNameChars);

		char name[160];
		if (group.observer_count > 0) {
			std::snprintf(
				name,
				sizeof(name),
				"%s %s  %d sys, %d obs",
				collapsed ? "[+]" : "[-]",
				display.c_str(),
				group.system_count,
				group.observer_count
			);
		}
		else {
			std::snprintf(
				name,
				sizeof(name),
				"%s %s  %d sys",
				collapsed ? "[+]" : "[-]",
				display.c_str(),
				group.system_count
			);
		}

		// Full-width click target. Built directly (not via CreateButton) so it carries no click
		// sound — rebuilds would otherwise churn the shared sound asset.
		const std::string group_key = key;
		std::ignore = world.entity()
						  .set<UIRect>({{.x = col.x_left, .y = cursor, .w = col.full_w, .h = panel.row_height}})
						  .set<Button>({
							  .label = "",
							  .font_size = panel.font_size,
							  .normal = core::colors::Panel,
							  .hover = core::colors::PanelHi,
							  .pressed_color = core::colors::PanelBg,
						  })
						  .set<OnClick>({[panel_entity, group_key](flecs::entity) {
							  auto& state = panel_entity.get_mut<SystemStatsPanel>();
							  state.collapsed[group_key] = !IsCollapsed(state, group_key);
							  state.dirty = true;
						  }})
						  .child_of(panel.content);

		std::ignore = world.entity()
						  .set<UIRect>({{.x = col.x_left + 4.0F, .y = cursor, .w = col.name_w, .h = panel.row_height}})
						  .set<Label>({
							  .text = name,
							  .font_size = panel.font_size,
							  .color = core::colors::Title,
							  .align = TextAlign::Left,
						  })
						  .child_of(panel.content);

		char total_text[32];
		FormatTime(total_text, sizeof(total_text), total, false);
		// Roll-up header binds to the module entity — or the panel entity, standing in for the
		// module-less "(root)" bucket — so the refresh reads its total via target<StatBoundTo>().
		std::ignore = world.entity()
						  .set<UIRect>({{.x = col.time_x, .y = cursor, .w = col.time_w, .h = panel.row_height}})
						  .set<Label>({
							  .text = total_text,
							  .font_size = panel.font_size,
							  .color = SeverityColor(GroupSeverity(panel, group), false),
							  .align = TextAlign::Right,
						  })
						  .add<StatBoundTo>(group.module_entity ? group.module_entity : panel_entity)
						  .child_of(panel.content);
		cursor += panel.row_height + spacing;

		if (collapsed) {
			continue;
		}

		for (const StatRow& row : group.rows) {
			constexpr float indent = 14.0F;
			std::ignore =
				world.entity()
					.set<UIRect>({{.x = col.x_left + indent, .y = cursor, .w = col.row_name_w, .h = panel.row_height}})
					.set<Label>({
						.text = row.name,
						.font_size = panel.font_size,
						.color = row.observer ? core::colors::Subtle : core::colors::Text,
						.align = TextAlign::Left,
					})
					.child_of(panel.content);

			char time_text[32];
			FormatTime(time_text, sizeof(time_text), row.ms, row.observer);
			const int row_severity =
				row.observer ? 0 : (panel.severity.contains(row.entity.id()) ? panel.severity.at(row.entity.id()) : 0);
			flecs::entity time_label =
				world.entity()
					.set<UIRect>({{.x = col.time_x, .y = cursor, .w = col.time_w, .h = panel.row_height}})
					.set<Label>({
						.text = time_text,
						.font_size = panel.font_size,
						.color = SeverityColor(row_severity, row.observer),
						.align = TextAlign::Right,
					})
					.child_of(panel.content);
			// Observers are untimed (static "-"); only system rows bind to their system entity.
			if (!row.observer) {
				time_label.add<StatBoundTo>(row.entity);
			}
			cursor += panel.row_height + spacing;
		}
	}

	// Summary lines (e.g. render stats) span the full width, positioned just below the checkbox row.
	// Created last (see above) but refreshed each frame via their SummaryBinding index.
	float summary_cursor = view.y + pad + row_step;
	for (size_t i = 0; i < summary.size(); ++i) {
		std::ignore = world.entity()
						  .set<UIRect>({{.x = col.x_left, .y = summary_cursor, .w = col.full_w, .h = panel.row_height}})
						  .set<Label>({
							  .text = summary[i],
							  .font_size = panel.font_size,
							  .color = core::colors::Title,
							  .align = TextAlign::Left,
						  })
						  .set<SummaryBinding>({.index = static_cast<int>(i)})
						  .child_of(panel.content);
		summary_cursor += row_step;
	}
}

// Refresh the value labels in place by walking the panel's children and reading each label's source
// through its StatBoundTo pair (or SummaryBinding index) — no stored entity handles that can go stale
// across a rebuild. Setting a component field doesn't change an entity's archetype, so mutating
// during children() iteration is safe.
void RefreshValues(
	const flecs::entity panel_entity,
	const SystemStatsPanel& panel,
	const std::map<std::string, StatGroup>& groups,
	const std::vector<std::string>& summary
) {
	// Module rollups are keyed by their source entity (the panel entity stands in for "(root)").
	std::unordered_map<uint64_t, std::pair<double, int>> module_agg;
	for (const auto& group : groups | std::views::values) {
		const uint64_t source = group.module_entity ? group.module_entity.id() : panel_entity.id();
		module_agg[source] = {GroupTotalMs(group), GroupSeverity(panel, group)};
	}

	panel.content.children([&](const flecs::entity child) {
		auto* label = child.try_get_mut<Label>();
		if (label == nullptr) {
			return;
		}
		if (const flecs::entity source = child.target<StatBoundTo>(); source) {
			char text[48];
			if (source.has(flecs::System)) {
				const double ms = panel.last_ms.contains(source.id()) ? panel.last_ms.at(source.id()) : 0.0;
				const int severity = panel.severity.contains(source.id()) ? panel.severity.at(source.id()) : 0;
				const double entities = panel.entities.contains(source.id()) ? panel.entities.at(source.id()) : 0.0;
				// Show ms alongside the matched entity count so hotspots (high ms, few entities) stand out.
				std::snprintf(text, sizeof(text), "%.3f ms  %s", ms, CompactCount(entities).c_str());
				label->text = text;
				label->color = SeverityColor(severity, false);
			}
			else if (const auto found = module_agg.find(source.id()); found != module_agg.end()) {
				FormatTime(text, sizeof(text), found->second.first, false);
				label->text = text;
				label->color = SeverityColor(found->second.second, false);
			}
		}
		else if (const auto* summary_binding = child.try_get<SummaryBinding>()) {
			if (summary_binding->index >= 0 && static_cast<size_t>(summary_binding->index) < summary.size()) {
				label->text = summary[static_cast<size_t>(summary_binding->index)];
			}
		}
	});
}

// Gather the current state and either rebuild the tree (structure changed) or update numbers in place.
void RefreshTree(
	const flecs::world& world,
	const flecs::entity panel_entity,
	SystemStatsPanel& panel,
	const flecs::query<>& systems,
	const flecs::query<>& observers
) {
	const std::vector<std::string> summary = BuildRenderSummary(world);
	const std::map<std::string, StatGroup> groups = GatherGroups(panel, systems, observers);
	std::string signature = BuildSignature(panel, groups, summary.size());
	if (panel.dirty || signature != panel.signature) {
		BuildStructure(world, panel_entity, panel, groups, summary);
		panel.signature = std::move(signature);
		panel.dirty = false;
	}
	else {
		RefreshValues(panel_entity, panel, groups, summary);
	}
}

} // namespace

flecs::entity
CreateSystemStatsPanel(const flecs::world& world, const core::Rect rect, const SystemStatsPanelOptions& options) {
	// The ScrollRect is both the viewport and the parent the header/row entities are created under;
	// they are positioned in the viewport's coordinate space and clipped/offset by the scroll. The
	// UIElement raises the whole panel above regular scene UI (z propagates to the subtree) and its
	// `visible` flag is what the F4 toggle flips.
	constexpr int kPanelZ = 500;
	const flecs::entity scroll = CreateScrollRect(world, rect, {.vertical = true});
	scroll.set<UIElement>({.z_index = kPanelZ, .visible = true});
	scroll.set<SystemStatsPanel>({
		.refresh_frames = options.refresh_frames,
		.average_window = options.average_window,
		.include_observers = options.include_observers,
		.hide_flecs_internals = options.hide_flecs_internals,
		.row_height = options.row_height,
		.font_size = options.font_size,
		.content = scroll,
	});
	// Attach ProfileStats so SampleTimings can route per-system timing through the profiler pipeline.
	scroll.set<profiling::ProfileStats>({});

	return scroll;
}

void RegisterSystemStatsPanelSystems(const flecs::world& world) {
	// Per-system timing is only accumulated while system-time measurement is enabled.
	ecs_measure_system_time(world.c_ptr(), true);

	world.component<SystemStatsPanel>();
	// Exclusive: one source per label. PairIsTag: keep (StatBoundTo, *) pairs pure tags even when the
	// target is itself a component (module entities are), so flecs never tries to construct the target
	// type as pair data.
	world.component<StatBoundTo>().add(flecs::Exclusive).add(flecs::PairIsTag);
	world.component<SummaryBinding>();

	// Persistent telemetry recorder — a world singleton so the "recording" state survives scene changes.
#ifdef PROFILE_LOGGING_ENABLED
	// Profiling preset: start recording immediately so data is captured from frame 1.
	world.set<StatsTelemetry>({
		.enabled = true,
		.file = std::make_shared<std::ofstream>("stats-telemetry.jsonl", std::ios::out | std::ios::trunc),
	});
#else
	world.set<StatsTelemetry>({});
#endif

	const flecs::query<> systems = world.query_builder().with(flecs::System).build();
	const flecs::query<> observers = world.query_builder().with(flecs::Observer).build();
	const flecs::query<SystemStatsPanel> panels = world.query_builder<SystemStatsPanel>().build();

	// Both driver and toggle are always-active *task* systems (no query terms) that iterate the
	// captured `panels` query — exactly like UILayout / UIBuildDrawList. A term-matched system would
	// flip active<->inactive as panels are created/destroyed during scene switches, which perturbs
	// the pipeline plan that flecs' always-on pipeline-stats monitor reads and trips its internal
	// `pip_count == i_system` assert.
	//
	// Runs in OnLoad (before UILayout in PreUpdate) so a freshly rebuilt tree is laid out the same
	// frame. Timings are sampled every `refresh_frames` frames and pushed into the retained labels;
	// the entity tree is only rebuilt on a structural change (collapse toggle or system-set change).
	// Hidden panels do no work — that is the panel's real "disabled" state, scoped per-panel so
	// hiding one scene's panel never freezes another's.
	world.system("UISystemStatsUpdate").kind(flecs::OnLoad).run([systems, observers, panels](const flecs::iter& it) {
		const flecs::world world_ref = it.world();
		panels.each([&](const flecs::entity self, SystemStatsPanel& panel) {
			if (const auto* element = self.try_get<UIElement>(); element != nullptr && !element->visible) {
				return;
			}
			++panel.frame_counter;

			bool sampled = false;
			if (panel.frame_counter >= panel.refresh_frames) {
				panel.frame_counter = 0;
				SampleTimings(world_ref, self, panel, systems);
				sampled = true;
			}
			if (sampled || panel.dirty) {
				RefreshTree(world_ref, self, panel, systems, observers);
			}
		});
	});

	// F4 shows/hides every stats panel. Runs in OnUpdate, where input polled in PreUpdate is fresh
	// (and not yet reset in PostUpdate).
	world.system("UISystemStatsToggle").kind(flecs::OnUpdate).run([panels](const flecs::iter& it) {
		if (!it.world().get<input::InputState>().WasKeyPressed(input::KeyCode::F4)) {
			return;
		}
		panels.each([](const flecs::entity self, SystemStatsPanel&) {
			if (self.has<UIElement>()) {
				auto& element = self.get_mut<UIElement>();
				element.visible = !element.visible;
			}
		});
	});
}

} // namespace engine::ui
