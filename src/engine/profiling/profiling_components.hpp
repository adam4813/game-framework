#pragma once

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace engine::profiling {

// A single named numeric measurement queued for emission by the ProfilerModule.
// `module_key` is checked against the profiling module filter (assets/config/profiling.json) to
// gate plot output; it is also the module label shown by profiling tooling.
struct PlotEntry {
	std::string module_key; // e.g. "RenderModule", "PathfindingModule", "World"
	std::string name;       // plot/file key (e.g. "render.fps", "path_cache.hits")
	std::variant<int64_t, double> value;
};

// Attach this component to any entity to route its stats through the profiling pipeline.
// Owning systems push entries each frame; ProfilerModule flushes them to Tracy and/or a file
// in the ProfilerFlushPhase (after PostFrame), then clears the list.
struct ProfileStats {
	std::vector<PlotEntry> entries;

	void Plot(const std::string_view module_key, const std::string_view name, int64_t value) {
		entries.push_back({std::string(module_key), std::string(name), value});
	}
	void Plot(const std::string_view module_key, const std::string_view name, double value) {
		entries.push_back({std::string(module_key), std::string(name), value});
	}
	void Plot(const std::string_view module_key, const std::string_view name, const float value) {
		entries.push_back({std::string(module_key), std::string(name), static_cast<double>(value)});
	}
};

// World singleton — optional file output configuration. Set via world.set<ProfilerConfig>({...}).
// The ProfilerModule's OnSet observer opens `file` when this is set with `file_enabled = true`, so
// the stream is ready before the (readonly) flush runs. Absent by default — file output is opt-in.
struct ProfilerConfig {
	bool file_enabled{false};
	std::string file_path{"profiler.jsonl"};
	std::shared_ptr<std::ofstream> file; // opened by the OnSet observer; shared_ptr keeps it movable
};

} // namespace engine::profiling
