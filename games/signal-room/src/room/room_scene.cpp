#include "room_scene.hpp"

#include <filesystem>

#include <spdlog/spdlog.h>

#include "engine/engine.hpp"
#include "layout.hpp"
#include "room_components.hpp"
#include "room_panel.hpp"
#include "shift.hpp"

using namespace engine;

namespace signal_room {

namespace {

// Shift and Layout paths are asset-relative; an absolute path (test fixtures) is used as-is.
std::string ResolveGamePath(const flecs::world& world, const std::string& path) {
	return std::filesystem::path{path}.is_absolute() ? path : assets::ResolveAsset(world, path);
}

} // namespace

void RoomScene::InitializePipeline(const flecs::world& world) {
	// clang-format off
	pipeline_ = world.pipeline()
					.with(flecs::System)
					.without<scene::MenuScene>()
					.with(flecs::Phase).cascade(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::ChildOf)
					.build();
	// clang-format on
}

void RoomScene::Load(flecs::world& world) {
	// Activated from a system (panel buttons), Load runs inside a deferred merge: suspend deferral so
	// the Layout's entities and singletons exist before the floor is built from them.
	const bool deferred = world.is_deferred();
	if (deferred) {
		world.defer_suspend();
	}

	roomRoot_ = world.entity("RoomRoot");
	LoadRoom(world);
	RegisterRoomPanel(world, roomRoot_);

	if (deferred) {
		world.defer_resume();
	}
}

void RoomScene::LoadRoom(const flecs::world& world) const {
	auto& state = world.get_mut<RoomState>();
	state.loadError.clear();
	const auto fail = [&state](const std::string& error) {
		state.loadError = error;
		spdlog::error("[Room] {}", error);
	};

	const auto shift = LoadShiftFile(ResolveGamePath(world, state.shiftPath));
	if (!shift) {
		fail(shift.error());
		return;
	}

	if (const auto layout = LoadLayout(world, ResolveGamePath(world, shift->layout), roomRoot_); !layout) {
		fail(layout.error());
		return;
	}
	spdlog::info("[Room] Loaded Shift '{}' ({}) in Layout {}", shift->name, shift->id, shift->layout);
}

void RoomScene::Unload(flecs::world& world) {
	if (roomRoot_) {
		roomRoot_.destruct();
		roomRoot_ = {};
	}
	UnloadLayout(world);
}

void StartRoom(const flecs::world& world, const std::string& shiftPath) {
	world.get_mut<RoomState>().shiftPath = shiftPath;
	if (const auto room = scene::FindScene<RoomSceneTag>(world); room && room.has<scene::Active>()) {
		scene::ReloadScene<RoomSceneTag>(world);
		return;
	}
	scene::ActivateScene<RoomSceneTag>(world);
}

void ReloadRoom(const flecs::world& world) { scene::ReloadScene<RoomSceneTag>(world); }

} // namespace signal_room
