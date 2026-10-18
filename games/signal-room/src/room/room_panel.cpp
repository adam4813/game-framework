#include "room_panel.hpp"

#include <algorithm>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include "engine/engine.hpp"
#include "room_components.hpp"
#include "room_constants.hpp"
#include "room_overlays.hpp"
#include "room_scene.hpp"
#include "room_tuning.hpp"

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
					changed = ui.Checkbox("Targeting ray and cone", overlays.targetingReach) || changed;
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

				if (ui.CollapsingHeader("Targeting")) {
					auto& tuning = world.get_mut<TargetingTuning>();
					// Draw every reach slider, then rebuild the overlay once if any changed.
					bool reachChanged = ui.SliderFloat("Ray length (m)", tuning.rayLength, 0.3F, 2.0F);
					reachChanged = ui.SliderFloat("Cone range (m)", tuning.coneRange, 0.3F, 2.5F) || reachChanged;
					reachChanged =
						ui.SliderFloat("Cone half angle (deg)", tuning.coneHalfAngle, 5.0F, 90.0F) || reachChanged;
					if (reachChanged) {
						BuildRoomOverlays(world, roomRoot);
					}
					ui.SliderFloat("Brighten rate (/s)", tuning.brightenRate, 0.1F, 5.0F);
					ui.SliderFloat("Brighten strength", tuning.brightenStrength, 0.05F, 1.0F);
					ui.SliderFloat("Brighten lift", tuning.brightenLift, 0.0F, 1.0F);
				}

				if (ui.CollapsingHeader("Dust puff")) {
					auto& tuning = world.get_mut<DustPuffTuning>();
					ui.SliderFloat("Particles", tuning.count, 1.0F, 40.0F);
					ui.SliderFloat("Speed (m/s)", tuning.speed, 0.1F, 4.0F);
					ui.SliderFloat("Lifetime (s)", tuning.lifetime, 0.05F, 1.5F);
					ui.SliderFloat("Size (m)", tuning.size, 0.01F, 0.3F);
					ui.SliderFloat("Spread", tuning.spread, 0.0F, 2.0F);
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
