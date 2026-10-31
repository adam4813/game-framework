#include "profiling_trace.hpp"

#include <cstddef>
#include <fstream>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <flecs.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>
#endif

namespace engine::profiling {

namespace {

#ifdef TRACY_ENABLE
// Modules whose plots are suppressed (from the filter file). Written once by LoadModuleFilter at
// startup, read-only thereafter — so concurrent IsModuleEnabled reads from worker threads are safe.
std::unordered_set<std::string> g_disabled_modules;

// Per-system-name stable source-location entries so Tracy can correlate zones across frames
// without allocating a new entry on every push. Flecs system names are permanent strings
// (owned by the ECS world) so using std::string keys by value is safe and cheap.
// g_tracy_locs_mutex serialises the rare first-write (new system name seen for the first time);
// concurrent reads on the hot path (name already present) share the lock and never block each other.
std::unordered_map<std::string, ___tracy_source_location_data> g_tracy_locs;
std::shared_mutex g_tracy_locs_mutex;
thread_local std::vector<TracyCZoneCtx> g_tracy_zone_stack;

void PerfTracePush(const char* file, const size_t line, const char* name) {
	// Hot path: shared lock — most calls find an existing entry. Multiple threads can read
	// concurrently; a rehash (exclusive write) cannot happen while any shared lock is held,
	// so &it->second is stable for the duration of this scope.
	{
		std::shared_lock read(g_tracy_locs_mutex);
		if (const auto it = g_tracy_locs.find(name); it != g_tracy_locs.end()) {
			g_tracy_zone_stack.push_back(___tracy_emit_zone_begin(&it->second, 1));
			return;
		}
	}
	// First registration for this name — exclusive lock to insert. Double-checked: another thread
	// may have inserted between the shared-lock release and here.
	std::unique_lock write(g_tracy_locs_mutex);
	auto& loc = g_tracy_locs[name];
	if (!loc.name) {
		loc = {name, name, file, static_cast<uint32_t>(line), 0};
	}
	g_tracy_zone_stack.push_back(___tracy_emit_zone_begin(&loc, 1));
}

void PerfTracePop(const char*, size_t, const char*) {
	if (!g_tracy_zone_stack.empty()) {
		___tracy_emit_zone_end(g_tracy_zone_stack.back());
		g_tracy_zone_stack.pop_back();
	}
}

// Tracy identifies a plot by the pointer identity of its name, so the char* must be stable for the
// program's lifetime. This interning pool provides that stability; std::unordered_set node storage
// is address-stable across inserts. Guarded by a mutex because immediate Plot() may be called from
// worker threads.
std::unordered_set<std::string> g_name_pool;
std::mutex g_name_pool_mutex;

const char* InternName(const std::string_view name) {
	std::scoped_lock lock(g_name_pool_mutex);
	return g_name_pool.emplace(name).first->c_str();
}
#endif

} // namespace

Zone::Zone([[maybe_unused]] const std::string_view name, [[maybe_unused]] const std::source_location& loc) {
#ifdef TRACY_ENABLE
	const uint64_t srcloc = ___tracy_alloc_srcloc_name(
		loc.line(),
		loc.file_name(),
		std::strlen(loc.file_name()),
		loc.function_name(),
		std::strlen(loc.function_name()),
		name.data(),
		name.size(),
		0
	);
	const TracyCZoneCtx ctx = ___tracy_emit_zone_begin_alloc(srcloc, 1);
	id_ = ctx.id;
	active_ = ctx.active;
#endif
}

Zone::~Zone() {
#ifdef TRACY_ENABLE
	TracyCZoneCtx ctx{};
	ctx.id = id_;
	ctx.active = active_;
	___tracy_emit_zone_end(ctx);
#endif
}

#ifdef TRACY_ENABLE
// ZoneSite mirrors Tracy's source-location record so a static ZoneSite can be handed to Tracy as-is.
static_assert(sizeof(ZoneSite) == sizeof(___tracy_source_location_data));
static_assert(offsetof(ZoneSite, line) == offsetof(___tracy_source_location_data, line));
static_assert(offsetof(ZoneSite, color) == offsetof(___tracy_source_location_data, color));
#endif

StaticZone::StaticZone([[maybe_unused]] const ZoneSite& site) {
#ifdef TRACY_ENABLE
	const TracyCZoneCtx ctx =
		___tracy_emit_zone_begin(reinterpret_cast<const ___tracy_source_location_data*>(&site), 1);
	id_ = ctx.id;
	active_ = ctx.active;
#endif
}

StaticZone::~StaticZone() {
#ifdef TRACY_ENABLE
	TracyCZoneCtx ctx{};
	ctx.id = id_;
	ctx.active = active_;
	___tracy_emit_zone_end(ctx);
#endif
}

void Plot(
	[[maybe_unused]] const std::string_view module_key,
	[[maybe_unused]] const std::string_view name,
	[[maybe_unused]] const std::int64_t value
) {
#ifdef TRACY_ENABLE
	if (!IsModuleEnabled(module_key)) {
		return;
	}
	TracyPlot(InternName(name), value);
#endif
}

void Plot(
	[[maybe_unused]] const std::string_view module_key,
	[[maybe_unused]] const std::string_view name,
	[[maybe_unused]] const double value
) {
#ifdef TRACY_ENABLE
	if (!IsModuleEnabled(module_key)) {
		return;
	}
	TracyPlot(InternName(name), value);
#endif
}

void InstallFlecsSystemZones() {
#ifdef TRACY_ENABLE
	ecs_os_api_t os_api = ecs_os_get_api();
	os_api.perf_trace_push_ = PerfTracePush;
	os_api.perf_trace_pop_ = PerfTracePop;
	ecs_os_set_api(&os_api);
#endif
}

void MarkFrame() {
#ifdef TRACY_ENABLE
	FrameMark;
#endif
}

bool IsModuleEnabled([[maybe_unused]] const std::string_view module_key) {
#ifdef TRACY_ENABLE
	return !g_disabled_modules.contains(std::string(module_key));
#else
	return true;
#endif
}

void LoadModuleFilter([[maybe_unused]] const std::string_view json_path) {
#ifdef TRACY_ENABLE
	std::ifstream f{std::string(json_path)};
	if (!f.is_open()) {
		spdlog::debug("[Profiling] No filter at '{}'; all module plots enabled", json_path);
		return;
	}
	const auto json = nlohmann::json::parse(f, nullptr, /*allow_exceptions=*/false);
	if (json.is_discarded()) {
		spdlog::warn("[Profiling] Failed to parse filter '{}'", json_path);
		return;
	}
	if (const auto it = json.find("disabled_modules"); it != json.end() && it->is_array()) {
		for (const auto& v : *it) {
			if (v.is_string()) {
				g_disabled_modules.emplace(v.get<std::string>());
			}
		}
	}
	spdlog::info("[Profiling] {} module(s) disabled for plotting", g_disabled_modules.size());
#endif
}

} // namespace engine::profiling
