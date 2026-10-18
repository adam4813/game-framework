#include "debug_ui_module.hpp"

#include <format>

#include <flecs.h>

#include "engine/ecs/ecs_types.hpp"
#include "engine/platform/platform.hpp"

namespace engine::debug {

DebugUiModule::DebugUiModule(const flecs::world& world) {
	world.component<ecs::OnDebugUI>().add(flecs::Phase).add(flecs::DependsOn, world.component<ecs::OnUI>());

	// Single-threaded (the default): ImGui and the GL calls must stay on the main thread.
	world.system("DebugUiDraw").kind<ecs::OnDebugUI>().run([](const flecs::iter& it) {
		it.world().get<platform::PlatformRef>().ptr->DrawDebugUi();
	});

	world.system("EngineDebugPanel").kind(flecs::OnUpdate).run([](const flecs::iter& it) {
		auto* platform = it.world().get<platform::PlatformRef>().ptr;
		auto& ui = platform->GetDebugUi();
		if (ui.BeginPanel("Engine")) {
			ui.Text(std::format("FPS: {}  ({:.2f} ms)", platform->Fps(), it.delta_time() * 1000.0f));
			// it.world() is a stage inside a system; ecs_get_entities needs the real world.
			const ecs_world_t* world = ecs_get_world(it.world().c_ptr());
			ui.Text(std::format("Entities: {}", ecs_get_entities(world).alive_count));
		}
		ui.EndPanel();
	});
}

} // namespace engine::debug
