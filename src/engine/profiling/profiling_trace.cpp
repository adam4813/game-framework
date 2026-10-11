#include "profiling_trace.hpp"

#include <fstream>
#include <mutex>
#include <string>
#include <unordered_set>

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
