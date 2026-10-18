#include <memory>

#include <flecs.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "engine/app/app.hpp"
#include "engine/ecs/flecs_remote.hpp"
#include "engine/engine.hpp"
#include "engine/platform/raylib/raylib_backend.hpp"
#include "room/room_constants.hpp"
#include "room/room_scene.hpp"
#include "signal_room_module.hpp"

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

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr const char* kWindowTitle = "Signal Room";

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

int main() {
	// Tee logs to both console (colour) and a persistent file so crashes are captured.
	auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("signal_room.log", true /*truncate*/);
	const auto logger = std::make_shared<spdlog::logger>("default", spdlog::sinks_init_list{consoleSink, fileSink});
	logger->set_level(spdlog::level::trace);
	spdlog::set_default_logger(logger);

	ecs_os_set_api_defaults();
	ecs_os_api_t osApi = ecs_os_get_api();
	osApi.log_ = FlecsLogHandler;
	ecs_os_set_api(&osApi);

	auto platform = std::make_unique<engine::platform::RaylibBackend>();
	platform->Init(kWindowWidth, kWindowHeight, kWindowTitle);

	const auto context = std::make_unique<engine::EngineContext>(std::move(platform));

#if !defined(__EMSCRIPTEN__)
	// REST API requires TCP sockets — unavailable in WASM browsers. Signal Room serves on its own port
	// so it can run beside the sandbox or another game (FLECS_REST_PORT still overrides it).
	engine::ecs::InitializeRemoteAPI(*context->GetWorld(), signal_room::kRestPort);
#endif

	engine::App app(context.get());

	auto& world = *context->GetWorld();
	world.import<signal_room::SignalRoomModule>();
	// No menus until the meta spec: boot straight into the Room.
	signal_room::StartRoom(world, signal_room::kBootShift);

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
