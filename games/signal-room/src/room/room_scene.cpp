#include "room_scene.hpp"

#include <filesystem>

#include <spdlog/spdlog.h>

#include "bindings.hpp"
#include "engine/engine.hpp"
#include "greybox_catalogue.hpp"
#include "layout.hpp"
#include "layout_greybox.hpp"
#include "operator_input.hpp"
#include "operator_movement.hpp"
#include "operator_spawn.hpp"
#include "operator_targeting.hpp"
#include "palette.hpp"
#include "room_components.hpp"
#include "room_constants.hpp"
#include "room_overlays.hpp"
#include "room_panel.hpp"
#include "room_tuning.hpp"
#include "shift.hpp"
#include "target_brighten.hpp"

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
	RegisterOperatorInput(world, roomRoot_);
	RegisterOperatorMovement(world, roomRoot_);
	RegisterOperatorTargeting(world, roomRoot_);
	RegisterTargetBrighten(world, roomRoot_);
	RegisterRoomPanel(world, roomRoot_);

	if (deferred) {
		world.defer_resume();
	}
}

void RoomScene::LoadRoom(const flecs::world& world) const {
	auto& state = world.get_mut<RoomState>();
	state.loadError.clear();
	if (const auto loaded = BuildRoom(world, state.shiftPath); !loaded) {
		// Leave an empty Room rather than half a Layout.
		world.delete_with(flecs::ChildOf, roomRoot_);
		UnloadLayout(world);
		state.loadError = loaded.error();
		spdlog::error("[Room] {}", loaded.error());
	}
}

std::expected<void, std::string> RoomScene::BuildRoom(const flecs::world& world, const std::string& shiftPath) const {
	// Registries first: the Layout checks its kinds against the catalogue.
	auto palette = LoadPaletteFile(assets::ResolveAsset(world, kPaletteFile));
	if (!palette) {
		return std::unexpected(palette.error());
	}
	auto catalogue = LoadGreyboxCatalogueFile(assets::ResolveAsset(world, kGreyboxCatalogueFile), *palette);
	if (!catalogue) {
		return std::unexpected(catalogue.error());
	}
	auto tuning = LoadTuningFile(assets::ResolveAsset(world, kTuningFile));
	if (!tuning) {
		return std::unexpected(tuning.error());
	}
	auto bindings = LoadBindingsFile(assets::ResolveAsset(world, kBindingsFile));
	if (!bindings) {
		return std::unexpected(bindings.error());
	}
	world.set<Palette>(std::move(*palette));
	world.set<GreyboxCatalogue>(std::move(*catalogue));
	world.set<MovementTuning>(tuning->movement);
	world.set<TargetingTuning>(tuning->targeting);
	world.set<Bindings>(std::move(*bindings));

	const auto shiftFile = ResolveGamePath(world, shiftPath);
	const auto shift = LoadShiftFile(shiftFile);
	if (!shift) {
		return std::unexpected(shift.error());
	}
	if (auto layout = LoadLayout(world, ResolveGamePath(world, shift->layout), roomRoot_); !layout) {
		return layout;
	}
	if (auto inService = ApplyInService(world, roomRoot_, shift->inService, shiftFile); !inService) {
		return inService;
	}

	BuildLayoutGreybox(world, roomRoot_);
	if (auto spawned = SpawnRoster(world, roomRoot_, shift->roster, shiftFile); !spawned) {
		return spawned;
	}
	BuildRoomOverlays(world, roomRoot_);
	spdlog::info("[Room] Loaded Shift '{}' ({}) in Layout {}", shift->name, shift->id, shift->layout);
	return {};
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
