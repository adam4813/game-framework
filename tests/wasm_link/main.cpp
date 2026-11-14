// Link check for the engine's WASM build: CI's engine workflow builds this instead of every Game Target.
// It is never run. It boots the engine the way a game's main.cpp does, with no scenes, so the link
// exercises the real entry points; the whole engine_lib archive is linked too (see CMakeLists.txt).

#include <emscripten.h>

#include <memory>

#include "engine/app/app.hpp"
#include "engine/engine.hpp"
#include "engine/platform/raylib/raylib_backend.hpp"

namespace {

engine::App* g_app = nullptr;

void TickWrapper() {
	g_app->Tick();
	g_app->PostTick();
}

} // namespace

int main() {
	auto platform = std::make_unique<engine::platform::RaylibBackend>();
	platform->Init(640, 360, "engine_wasm_link");

	const auto context = std::make_unique<engine::EngineContext>(std::move(platform));
	engine::App app(context.get());

	g_app = &app;
	emscripten_set_main_loop(TickWrapper, 0, 1);
	return 0;
}
