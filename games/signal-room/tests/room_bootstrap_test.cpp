#include <filesystem>
#include <format>
#include <fstream>
#include <string_view>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "engine/input/input.hpp"
#include "room/room_components.hpp"
#include "room_harness.hpp"

namespace KeyCode = engine::input::KeyCode;
using engine::input::InputState;
using signal_room::FloorGrid;
using signal_room::test::RoomHarness;

namespace {

TEST(RoomBootstrap, Shift1LoadsA13By9FloorOf1mCells) {
	RoomHarness room;

	const auto loaded = room.LoadShift("shifts/shift_01.shift.json");
	ASSERT_TRUE(loaded.has_value()) << loaded.error();
	room.Step();

	ASSERT_TRUE(room.World().has<FloorGrid>());
	const auto& grid = room.World().get<FloorGrid>();
	EXPECT_EQ(grid.width, 13);
	EXPECT_EQ(grid.height, 9);
	EXPECT_FLOAT_EQ(grid.cellSize, 1.0F);
}

TEST(RoomBootstrap, ShiftNamingAMissingLayoutFailsWithThatPath) {
	RoomHarness room;

	const auto loaded = room.LoadShift(SIGNAL_ROOM_FIXTURE_DIR "/missing_layout.shift.json");

	ASSERT_FALSE(loaded.has_value());
	EXPECT_THAT(loaded.error(), testing::HasSubstr("layouts/does_not_exist.layout.json"));
	EXPECT_FALSE(room.World().has<FloorGrid>());
}

// Scratch directory under the system temp dir, removed when the test ends (pass or fail).
struct ScratchDir {
	std::filesystem::path path;
	explicit ScratchDir(const std::string_view name) : path(std::filesystem::temp_directory_path() / name) {
		std::filesystem::remove_all(path);
		std::filesystem::create_directories(path);
	}
	~ScratchDir() {
		std::error_code ec;
		std::filesystem::remove_all(path, ec);
	}
};

TEST(RoomBootstrap, ReloadRereadsTheLayoutFromDisk) {
	const ScratchDir dir{"signal_room_reload_test"};
	const auto layoutPath = (dir.path / "reload.layout.json").generic_string();
	const auto shiftPath = (dir.path / "reload.shift.json").generic_string();
	const auto writeLayout = [&](const int width) {
		std::ofstream{layoutPath} << std::format(
			R"({{"singletons": {{"floor_grid": {{"width": {}, "height": 4, "cell_size": 1.0}}}}}})",
			width
		);
	};
	std::ofstream{shiftPath} << std::format(R"({{"id": "reload", "layout": "{}"}})", layoutPath);
	writeLayout(5);

	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(shiftPath).has_value());
	ASSERT_EQ(room.World().get<FloorGrid>().width, 5);

	writeLayout(7);
	const auto reloaded = room.Reload();

	ASSERT_TRUE(reloaded.has_value()) << reloaded.error();
	EXPECT_EQ(room.World().get<FloorGrid>().width, 7);
}

TEST(RoomHarness, TappedKeyIsPressedForOneFrameAndHeldKeyStaysDown) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift("shifts/shift_01.shift.json").has_value());
	int spacePresses = 0;
	int wDownFrames = 0;
	room.World().system("KeyProbe").kind(flecs::OnUpdate).run([&](const flecs::iter& it) {
		const auto& input = it.world().get<InputState>();
		spacePresses += input.WasKeyPressed(KeyCode::Space) ? 1 : 0;
		wDownFrames += input.IsKeyDown(KeyCode::W) ? 1 : 0;
	});

	room.Tap(KeyCode::Space);
	room.Hold(KeyCode::W);
	room.Step(3);
	room.Release(KeyCode::W);
	room.Step(2);

	EXPECT_EQ(spacePresses, 1);
	EXPECT_EQ(wDownFrames, 3);
}

} // namespace
