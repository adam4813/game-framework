#include "room_panel.hpp"

#include <algorithm>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include "engine/engine.hpp"
#include "room_components.hpp"
#include "room_constants.hpp"
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
		.run([shiftFiles = ListShiftFiles(world)](const flecs::iter& it) {
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
