#include "room_scene.hpp"

#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "bindings.hpp"
#include "engine/engine.hpp"
#include "greybox_catalogue.hpp"
#include "hud.hpp"
#include "hud_layout.hpp"
#include "item_catalogue.hpp"
#include "item_interaction.hpp"
#include "item_placement.hpp"
#include "item_spawn.hpp"
#include "layout.hpp"
#include "layout_greybox.hpp"
#include "modifiers.hpp"
#include "operator_input.hpp"
#include "operator_movement.hpp"
#include "operator_spawn.hpp"
#include "operator_targeting.hpp"
#include "palette.hpp"
#include "radio_systems.hpp"
#include "room_components.hpp"
#include "room_constants.hpp"
#include "room_overlays.hpp"
#include "room_panel.hpp"
#include "room_tuning.hpp"
#include "score_systems.hpp"
#include "shift.hpp"
#include "station_catalogue.hpp"
#include "station_job.hpp"
#include "station_reads.hpp"
#include "station_systems.hpp"
#include "target_brighten.hpp"
#include "ticket_rules.hpp"
#include "ticket_simulation.hpp"

using namespace engine;

namespace signal_room {

namespace {

// Shift and Layout paths are asset-relative; an absolute path (test fixtures) is used as-is.
std::string ResolveGamePath(const flecs::world& world, const std::string& path) {
	return std::filesystem::path{path}.is_absolute() ? path : assets::ResolveAsset(world, path);
}

std::string JoinIds(const std::vector<std::string>& ids) {
	std::string joined;
	for (const auto& id : ids) {
		joined += (joined.empty() ? "" : ", ") + id;
	}
	return joined;
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
	RegisterStationHolding(world, roomRoot_);
	RegisterRadioSystems(world, roomRoot_);
	RegisterOperatorTargeting(world, roomRoot_);
	RegisterItemInteraction(world, roomRoot_);
	RegisterStationSystems(world, roomRoot_);
	RegisterCarriedItemPlacement(world, roomRoot_);
	RegisterStationReads(world, roomRoot_);
	RegisterTargetBrighten(world, roomRoot_);
	RegisterTicketSimulation(world, roomRoot_);
	RegisterScoreSystems(world, roomRoot_);
	if (world.get<RoomState>().loadError.empty()) {
		RegisterHud(world, roomRoot_);
	}
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
	// Which Modifiers are active comes from the unpatched Shift file and the dev toggle; every game
	// document below is read through them.
	const auto shiftFile = ResolveGamePath(world, shiftPath);
	const auto& state = world.get<RoomState>();
	const auto modifierCatalogue = LoadModifierCatalogueFile(
		state.modifierCataloguePath.empty() ? assets::ResolveAsset(world, kModifierCatalogueFile)
											: state.modifierCataloguePath
	);
	if (!modifierCatalogue) {
		return std::unexpected(modifierCatalogue.error());
	}
	const auto shiftModifiers = ReadShiftModifierIds(shiftFile);
	if (!shiftModifiers) {
		return std::unexpected(shiftModifiers.error());
	}
	const auto active = ResolveActiveModifiers(*modifierCatalogue, *shiftModifiers, world.get<ModifierToggle>());
	if (!active) {
		return std::unexpected(std::format("Shift file {}: {}", shiftFile, active.error()));
	}
	world.set<ModifierCatalogue>(*modifierCatalogue);
	world.set<AppliedModifiers>({.ids = JoinIds(active->ids)});

	// Registries first: the Layout checks its kinds against the catalogue.
	auto palette = LoadPaletteFile(assets::ResolveAsset(world, kPaletteFile));
	if (!palette) {
		return std::unexpected(palette.error());
	}
	auto catalogue = LoadGreyboxCatalogueFile(assets::ResolveAsset(world, kGreyboxCatalogueFile), *palette);
	if (!catalogue) {
		return std::unexpected(catalogue.error());
	}
	auto ticketRules = LoadTicketRulesFile(assets::ResolveAsset(world, kTicketRulesFile), *active);
	if (!ticketRules) {
		return std::unexpected(ticketRules.error());
	}
	auto frequencies = LoadFrequencyTableFile(assets::ResolveAsset(world, kFrequencyTableFile), *palette);
	if (!frequencies) {
		return std::unexpected(frequencies.error());
	}
	auto items = LoadItemCatalogueFile(assets::ResolveAsset(world, kItemCatalogueFile), *catalogue, *active);
	if (!items) {
		return std::unexpected(items.error());
	}
	auto stations = LoadStationCatalogueFile(assets::ResolveAsset(world, kStationCatalogueFile), *items, *active);
	if (!stations) {
		return std::unexpected(stations.error());
	}
	auto tuning = LoadTuningFile(assets::ResolveAsset(world, kTuningFile), *active);
	if (!tuning) {
		return std::unexpected(tuning.error());
	}
	auto hud = LoadHudLayoutFile(assets::ResolveAsset(world, kHudFile), *palette);
	if (!hud) {
		return std::unexpected(hud.error());
	}
	auto bindings = LoadBindingsFile(assets::ResolveAsset(world, kBindingsFile));
	if (!bindings) {
		return std::unexpected(bindings.error());
	}
	world.set<Palette>(std::move(*palette));
	world.set<GreyboxCatalogue>(std::move(*catalogue));
	world.set<ItemCatalogue>(std::move(*items));
	world.set<StationCatalogue>(std::move(*stations));
	world.set<TicketRules>(std::move(*ticketRules));
	world.set<FrequencyTable>(std::move(*frequencies));
	world.set<MovementTuning>(tuning->movement);
	world.set<TargetingTuning>(tuning->targeting);
	world.set<DustPuffTuning>(tuning->dustPuff);
	world.set<CouplerTuning>(tuning->coupler);
	world.set<ConfettiTuning>(tuning->confetti);
	world.set<ReadTuning>(tuning->reads);
	world.set<Bindings>(std::move(*bindings));
	world.set<HudLayout>(std::move(*hud));

	const auto shift = LoadShiftFile(shiftFile, *active);
	if (!shift) {
		return std::unexpected(shift.error());
	}
	if (auto tickets = ValidateShiftTickets(*shift, world.get<TicketRules>(), world.get<FrequencyTable>(), shiftFile);
		!tickets) {
		return tickets;
	}
	if (auto layout = LoadLayout(world, ResolveGamePath(world, shift->layout), roomRoot_); !layout) {
		return layout;
	}
	if (auto inService = ApplyInService(world, roomRoot_, shift->inService, shiftFile); !inService) {
		return inService;
	}

	AddStationJobs(world, roomRoot_);
	if (auto radios = AssignRadios(world, roomRoot_, *shift, shiftFile); !radios) {
		return radios;
	}
	BuildLayoutGreybox(world, roomRoot_);
	if (auto spawned = SpawnRoster(world, roomRoot_, shift->roster, shiftFile); !spawned) {
		return spawned;
	}
	if (auto placed = SpawnCounterItems(world, roomRoot_, shift->counterItems, shiftFile); !placed) {
		return placed;
	}
	BuildRoomOverlays(world, roomRoot_);
	StartShiftWindow(world, *shift);
	StartScore(world, *shift);
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
