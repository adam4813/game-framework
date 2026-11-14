#include <memory>

#include <flecs.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>
#include <unordered_map>
#include <vector>
#endif

#include "constants.hpp"
#include "engine/app/app.hpp"
#include "engine/ecs/flecs_remote.hpp"
#include "engine/engine.hpp"
#include "engine/platform/raylib/raylib_backend.hpp"
#include "farm_game_module.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>

namespace {
engine::App* g_app = nullptr;
void tickWrapper() {
	g_app->Tick();
	g_app->PostTick();
}
} // namespace
#endif

namespace {

void FlecsLogHandler(const int32_t level, const char* file, int32_t line, const char* msg) {
	switch (level) {
	case 0: spdlog::trace("[Flecs] {}:{}: {}", file, line, msg); break;
	case -2: spdlog::warn("[Flecs] {}:{}: {}", file, line, msg); break;
	case -3: spdlog::error("[Flecs] {}:{}: {}", file, line, msg); break;
	case -4: spdlog::critical("[Flecs] {}:{}: {}", file, line, msg); break;
	default: spdlog::info("[Flecs] {}:{}: {}", file, line, msg); break;
	}
}

#ifdef TRACY_ENABLE
#include <shared_mutex>

std::unordered_map<std::string, ___tracy_source_location_data> g_tracy_locs;
std::shared_mutex g_tracy_locs_mutex;
thread_local std::vector<TracyCZoneCtx> g_tracy_zone_stack;

void PerfTracePush(const char* file, const size_t line, const char* name) {
	{
		std::shared_lock read(g_tracy_locs_mutex);
		if (const auto it = g_tracy_locs.find(name); it != g_tracy_locs.end()) {
			g_tracy_zone_stack.push_back(___tracy_emit_zone_begin(&it->second, 1));
			return;
		}
	}
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
#endif

} // namespace

int main() {
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("farm.log", true /*truncate*/);
	const auto logger = std::make_shared<spdlog::logger>("default", spdlog::sinks_init_list{console_sink, file_sink});
	logger->set_level(spdlog::level::trace);
	spdlog::set_default_logger(logger);

	ecs_os_set_api_defaults();
	ecs_os_api_t os_api = ecs_os_get_api();
	os_api.log_ = FlecsLogHandler;
#ifdef TRACY_ENABLE
	os_api.perf_trace_push_ = PerfTracePush;
	os_api.perf_trace_pop_ = PerfTracePop;
#endif
	ecs_os_set_api(&os_api);

	auto platform = std::make_unique<engine::platform::RaylibBackend>();
	platform->Init(app::constants::kWindowWidth, app::constants::kWindowHeight, "Farm");

	const auto context = std::make_unique<engine::EngineContext>(std::move(platform));

#if !defined(__EMSCRIPTEN__)
	engine::ecs::InitializeRemoteAPI(*context->GetWorld());
#endif

	engine::App app(context.get());

#if !defined(__EMSCRIPTEN__)
	context->GetWorld()->set_threads(4);
#endif
	context->GetWorld()->import<farm::FarmGameModule>();

#if defined(__EMSCRIPTEN__)
	g_app = &app;
	emscripten_set_main_loop(tickWrapper, 0, 1);
#else
	while (!context->GetPlatform()->ShouldClose()) {
		app.Tick();
		app.PostTick();
		engine::profiling::MarkFrame();
	}
	context->GetPlatform()->Shutdown();
#endif

	return 0;
}
