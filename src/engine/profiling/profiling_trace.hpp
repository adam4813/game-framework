#pragma once

#include <cstdint>
#include <source_location>
#include <string_view>

// Consumer-facing profiling interface. This is the ONLY profiling header a subsystem needs to
// instrument code: it exposes scoped zones, immediate plots, a frame marker, and the per-module
// filter — with zero knowledge of the backend. Whether data is sent to Tracy, a file, or nowhere
// is an implementation detail hidden in profiling_trace.cpp. No macros, no #ifdefs, no Tracy
// headers leak to callers; in a non-profiled build every call compiles to a no-op.
namespace engine::profiling {

// RAII scope timer. Construct at the top of a function or block to time it; the zone closes when
// the object goes out of scope. The call site is captured automatically via std::source_location,
// so no macro is needed:
//
//   void Solve() {
//       profiling::Zone _zone{"Pathfinder::Solve"};
//       ...
//   }
//
// `name` must outlive the constructor call (a string literal is ideal). Non-copyable/non-movable —
// it is meant to live on the stack for the duration of the scope.
class Zone {
public:
	explicit Zone(std::string_view name, const std::source_location& loc = std::source_location::current());
	~Zone();

	Zone(const Zone&) = delete;
	Zone& operator=(const Zone&) = delete;
	Zone(Zone&&) = delete;
	Zone& operator=(Zone&&) = delete;

private:
	// Opaque backend zone handle, stored without leaking backend types into this header.
	std::uint32_t id_{0};
	int active_{0};
};

// Emit a scalar plot immediately (thread-safe). Use this from contexts that cannot queue into a
// ProfileStats component — e.g. worker-thread code that runs many times per frame. For per-frame
// aggregate stats owned by a system, prefer ProfileStats::Plot (queued + flushed once per frame).
// `module_key` gates output through the per-module filter (see LoadModuleFilter).
void Plot(std::string_view module_key, std::string_view name, std::int64_t value);
void Plot(std::string_view module_key, std::string_view name, double value);
inline void Plot(const std::string_view module_key, const std::string_view name, const float value) {
	Plot(module_key, name, static_cast<double>(value));
}

// Mark the end of a frame on the profiler timeline. Call once per rendered frame.
void MarkFrame();

// True when `module_key` is not disabled in the profiling filter. Lets a producer skip expensive
// stat gathering entirely when its module is filtered out. Always true in non-profiled builds.
[[nodiscard]] bool IsModuleEnabled(std::string_view module_key);

// Load the per-module filter from `json_path` (schema: { "disabled_modules": ["ModuleName", ...] }).
// Call once at startup. Missing/invalid files leave every module enabled.
void LoadModuleFilter(std::string_view json_path);

} // namespace engine::profiling
