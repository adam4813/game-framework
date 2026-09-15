#include <memory>

#include <flecs.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "constants.hpp"
#include "engine/app/app.hpp"
#include "engine/ecs/flecs_remote.hpp"
#include "engine/engine.hpp"
#include "engine/platform/raylib/raylib_backend.hpp"
#include "game/game.hpp"

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

/* The level should be interpreted as:
 * >0: Debug tracing. Only enabled in debug builds.
 *  0: Tracing. Enabled in debug/release builds.
 * -2: Warning. An issue occurred, but operation was successful.
 * -3: Error. An issue occurred, and operation was unsuccessful.
 * -4: Fatal. An issue occurred, and application must quit. */
void FlecsLogHandler(const int32_t level, const char* file, int32_t line, const char* msg) {
	switch (level) {
	case 0: spdlog::trace("[Flecs] {}:{}: {}", file, line, msg); break;
	case -2: spdlog::warn("[Flecs] {}:{}: {}", file, line, msg); break;
	case -3: spdlog::error("[Flecs] {}:{}: {}", file, line, msg); break;
	case -4: spdlog::critical("[Flecs] {}:{}: {}", file, line, msg); break;
	default: spdlog::info("[Flecs] {}:{}: {}", file, line, msg); break;
	}
}

} // namespace

int main(int argc, char** argv) {
	// Tee logs to both console (colour) and a persistent file so crashes are captured.
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("game.log", true /*truncate*/);
	auto logger = std::make_shared<spdlog::logger>(
		"default", spdlog::sinks_init_list{console_sink, file_sink}
	);
	logger->set_level(spdlog::level::trace);
	spdlog::set_default_logger(logger);

	ecs_os_set_api_defaults();
	ecs_os_api_t os_api = ecs_os_get_api();
	os_api.log_ = FlecsLogHandler;
	ecs_os_set_api(&os_api);

	auto platform = std::make_unique<engine::platform::RaylibBackend>();
	platform->Init(app::constants::kWindowWidth, app::constants::kWindowHeight, app::constants::kWindowTitle);

	const auto context = std::make_unique<engine::EngineContext>(std::move(platform));

#if !defined(__EMSCRIPTEN__)
	// REST API requires TCP sockets — unavailable in WASM browsers.
	engine::ecs::InitializeRemoteAPI(*context->GetWorld());
#endif

	engine::App app(context.get());

	// The game layer is a Flecs module: it reads the EngineContext from the world singleton set by
	// the context constructor, then registers its scenes and activates the title scene.
	context->GetWorld()->import<game::GameModule>();

#if defined(__EMSCRIPTEN__)
	g_app = &app;
	emscripten_set_main_loop(tickWrapper, 0, 1);
#else
	while (!context->GetPlatform()->ShouldClose()) {
		app.Tick();
		app.PostTick();
	}
	context->GetPlatform()->Shutdown();
#endif

	return 0;
}
