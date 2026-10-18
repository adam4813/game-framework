#include "signal_room_module.hpp"

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

#include "engine/engine.hpp"
#include "room/layout.hpp"
#include "room/room_components.hpp"
#include "room/room_scene.hpp"

using namespace engine;

namespace signal_room {

namespace {

// Every game component is registered here, before any system or load can reach it lazily (a lazy
// registration inside a readonly multi-stage world crashes; see AGENTS.md).
void RegisterComponents(const flecs::world& world) {
	world.component<RoomSceneTag>();
	world.component<FloorTile>();
	world.component<FloorGrid>()
		.member<int>("width")
		.member<int>("height")
		.member<float>("cell_size")
		.member<glm::vec3>("origin")
		.add(EcsSingleton);
	world.component<RoomState>()
		.member<std::string>("shift_path")
		.member<std::string>("load_error")
		.add(EcsSingleton);
}

} // namespace

SignalRoomModule::SignalRoomModule(flecs::world& world) {
	// Level, render, input and the debug UI are always imported by the EngineContext.
	world.get<EngineContextRef>().ptr->GetModuleRegistry().Import(world, {"particles", "pathfinding"});

	RegisterComponents(world);
	world.set<RoomState>({});
	RegisterLayoutLoaders(world);

	scene::RegisterScene<RoomSceneTag>(world, std::make_shared<RoomScene>());

	spdlog::info("[SignalRoom] Module imported");
}

} // namespace signal_room
