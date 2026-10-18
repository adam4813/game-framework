#include "signal_room_module.hpp"

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

#include "engine/engine.hpp"
#include "room/greybox_catalogue.hpp"
#include "room/layout.hpp"
#include "room/palette.hpp"
#include "room/room_components.hpp"
#include "room/room_scene.hpp"

using namespace engine;

namespace signal_room {

namespace {

// Every game component is registered here, before any system or load can reach it lazily (a lazy
// registration inside a readonly multi-stage world crashes; see AGENTS.md).
void RegisterComponents(const flecs::world& world) {
	world.component<glm::ivec2>("ivec2").member<int>("x").member<int>("y");
	world.component<Sides>()
		.bit("north", Sides::North)
		.bit("east", Sides::East)
		.bit("south", Sides::South)
		.bit("west", Sides::West);

	world.component<RoomSceneTag>();
	world.component<FloorTile>();
	world.component<Tarped>();
	world.component<FloorGrid>()
		.member<int>("width")
		.member<int>("height")
		.member<float>("cell_size")
		.member<glm::vec3>("origin")
		.add(EcsSingleton);
	world.component<RoomState>().member<std::string>("shift_path").member<std::string>("load_error").add(EcsSingleton);
	world.component<RoomOverlays>()
		.member<bool>("floor_grid")
		.member<bool>("solid_cells")
		.member<bool>("worked_sides")
		.add(EcsSingleton);
	world.component<LayoutErrors>().add(EcsSingleton);
	world.component<Palette>().add(EcsSingleton);
	world.component<GreyboxCatalogue>().add(EcsSingleton);

	world.component<Station>()
		.member<std::string>("kind")
		.member<glm::ivec2>("cell")
		.member<glm::ivec2>("footprint")
		.member<Sides>("worked_sides");
	world.component<Counter>().member<glm::ivec2>("cell");
	world.component<Wall>()
		.member<glm::ivec2>("cell")
		.member<glm::ivec2>("footprint")
		.member<std::string>("height_class");
	world.component<EdgeBlock>().member<glm::ivec2>("cell").member<glm::ivec2>("footprint");
	world.component<Prop>()
		.member<std::string>("kind")
		.member<glm::ivec2>("cell")
		.member<glm::ivec2>("footprint")
		.member<bool>("solid")
		.member<float>("height");
	world.component<SpawnMarker>().member<glm::ivec2>("cell");
}

} // namespace

SignalRoomModule::SignalRoomModule(flecs::world& world) {
	// Level, render, input and the debug UI are always imported by the EngineContext.
	world.get<EngineContextRef>().ptr->GetModuleRegistry().Import(world, {"particles", "pathfinding"});

	RegisterComponents(world);
	world.set<RoomState>({});
	world.set<RoomOverlays>({});
	RegisterLayoutLoaders(world);

	scene::RegisterScene<RoomSceneTag>(world, std::make_shared<RoomScene>());

	spdlog::info("[SignalRoom] Module imported");
}

} // namespace signal_room
