#include "room_panel.hpp"

#include <algorithm>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include "engine/engine.hpp"
#include "movement_tuning.hpp"
#include "room_components.hpp"
#include "room_constants.hpp"
#include "room_overlays.hpp"
#include "room_scene.hpp"

using namespace engine;

namespace signal_room {

namespace {

constexpr core::Rgba kErrorColour{.r = 240, .g = 90, .b = 80, .a = 255};

// Asset-relative paths of the Shift files, sorted by name.
std::vector<std::string> ListShiftFiles(const flecs::world& world) {
	std::vector<std::string> paths;
	std::error_code ec;
	for (const auto& entry : std::filesystem::directory_iterator{assets::ResolveAsset(world, kShiftDirectory), ec}) {
		if (entry.is_regular_file() && entry.path().extension() == ".json") {
			paths.push_back(std::format("{}/{}", kShiftDirectory, entry.path().filename().string()));
		}
	}
	std::ranges::sort(paths);
	return paths;
}

} // namespace

void RegisterRoomPanel(const flecs::world& world, const flecs::entity roomRoot) {
	// Scanned once per Room load; Reload rebuilds the panel, so new files show up after a Reload.
	world.system("RoomPanel")
		.kind(flecs::OnUpdate)
		.run([shiftFiles = ListShiftFiles(world), roomRoot](const flecs::iter& it) {
			const auto world = it.world();
			auto& ui = world.get<platform::PlatformRef>().ptr->GetDebugUi();
			if (ui.BeginPanel("Room")) {
				const auto& state = world.get<RoomState>();
				ui.Text(std::format("Shift: {}", state.shiftPath));
				if (!state.loadError.empty()) {
					ui.TextColored(kErrorColour, state.loadError);
				}
				if (world.has<FloorGrid>()) {
					const auto& grid = world.get<FloorGrid>();
					ui.Text(std::format("Floor: {} x {} cells of {} m", grid.width, grid.height, grid.cellSize));
				}

				if (ui.CollapsingHeader("Overlays")) {
					auto& overlays = world.get_mut<RoomOverlays>();
					// Draw every checkbox, then rebuild once if any changed.
					bool changed = ui.Checkbox("Floor grid", overlays.floorGrid);
					changed = ui.Checkbox("Solid cells", overlays.solidCells) || changed;
					changed = ui.Checkbox("Worked sides", overlays.workedSides) || changed;
					changed = ui.Checkbox("Operator circle", overlays.operatorCircle) || changed;
					if (changed) {
						BuildRoomOverlays(world, roomRoot);
					}
				}

				if (ui.CollapsingHeader("Movement")) {
					// Live and unsaved: Reload re-reads the tuning file.
					auto& tuning = world.get_mut<MovementTuning>();
					ui.SliderFloat("Walk speed (m/s)", tuning.walkSpeed, 1.0F, 10.0F);
					ui.SliderFloat("Ramp up (s)", tuning.accelTime, 0.01F, 0.5F);
					ui.SliderFloat("Ramp down (s)", tuning.decelTime, 0.01F, 0.5F);
					ui.SliderFloat("Turn time (s)", tuning.turnTime, 0.01F, 0.5F);
					ui.SliderFloat("Dash multiplier", tuning.dashMultiplier, 1.0F, 4.0F);
					ui.SliderFloat("Dash duration (s)", tuning.dashDuration, 0.05F, 0.6F);
					ui.SliderFloat("Dash cooldown (s)", tuning.dashCooldown, 0.0F, 2.0F);
					if (ui.SliderFloat("Operator radius (m)", tuning.radius, 0.1F, 0.49F)) {
						BuildRoomOverlays(world, roomRoot);
					}
				}

				ui.Separator();
				for (const auto& path : shiftFiles) {
					if (ui.Selectable(path, path == state.shiftPath)) {
						StartRoom(world, path);
					}
				}
				if (ui.Button("Reload")) {
					ReloadRoom(world);
				}
				ui.Tooltip("Re-read the Shift and Layout files from disk");
			}
			ui.EndPanel();
		})
		.child_of(roomRoot);
}

} // namespace signal_room
