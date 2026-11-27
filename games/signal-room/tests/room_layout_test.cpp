#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "engine/pathfinding/grid/grid_nav_graph.hpp"
#include "engine/pathfinding/pathfinding_components.hpp"
#include "room/room_components.hpp"
#include "room_harness.hpp"

using engine::pathfinding::GridNavGraph;
using engine::pathfinding::NavEnvironment;
using signal_room::FloorGrid;
using signal_room::Station;
using signal_room::Tarped;
using signal_room::test::RoomHarness;

namespace {

constexpr std::string_view kShift1 = "shifts/shift_01.shift.json";

flecs::entity FindStation(const RoomHarness& room, const std::string_view name) {
	flecs::entity found;
	room.World().each([&](const flecs::entity e, const Station&) {
		if (std::string_view{e.name().c_str()} == name) {
			found = e;
		}
	});
	return found;
}

TEST(RoomLayout, Shift1PutsRadio1DecoderAndTelexInServiceAndTarpsTheRest) {
	RoomHarness room;
	const auto loaded = room.LoadShift(kShift1);
	ASSERT_TRUE(loaded.has_value()) << loaded.error();

	for (const auto* name : {"Radio1", "Decoder", "Telex"}) {
		const auto station = FindStation(room, name);
		ASSERT_TRUE(station) << name;
		EXPECT_FALSE(station.has<Tarped>()) << name;
	}
	for (const auto* name : {"Radio2", "Radio3", "Keypunch"}) {
		const auto station = FindStation(room, name);
		ASSERT_TRUE(station) << name;
		EXPECT_TRUE(station.has<Tarped>()) << name;
	}
}

TEST(RoomLayout, StormWindowCellsAndTheGapBetweenRadio2AndRadio3AreOpenFloor) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());
	const auto& grid = room.World().get<FloorGrid>();

	// Radio2 stands on (4-5,0) and Radio3 on (8-9,0); the window cells (6-7,0) between them are open.
	EXPECT_TRUE(grid.IsSolid({5, 0}));
	EXPECT_FALSE(grid.IsSolid({6, 0}));
	EXPECT_FALSE(grid.IsSolid({7, 0}));
	EXPECT_TRUE(grid.IsSolid({8, 0}));
}

TEST(RoomLayout, TarpedStationsAndSolidPropsAreSolidButTheRugIsNot) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());
	const auto& grid = room.World().get<FloorGrid>();

	EXPECT_TRUE(grid.IsSolid({3, 2}));  // Tarped Keypunch
	EXPECT_TRUE(grid.IsSolid({12, 0})); // Supply Rack tarp
	EXPECT_TRUE(grid.IsSolid({0, 8}));  // the Counter that replaced the Vent
	EXPECT_FALSE(grid.IsSolid({5, 3})); // rug beside the Decoder
	EXPECT_FALSE(grid.IsSolid({0, 3})); // floor under the wall-mounted Power Panel tarp
}

TEST(RoomLayout, NavGraphWalksTheOpenFloorAndNotTheSolidCells) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());

	const auto& nav = room.World().get<NavEnvironment>();
	ASSERT_TRUE(nav.IsReady());
	const auto& graph = static_cast<const GridNavGraph&>(*nav.graph);
	EXPECT_EQ(graph.width, 13);
	EXPECT_EQ(graph.height, 9);
	EXPECT_TRUE(graph.IsWalkable(6, 0));
	EXPECT_TRUE(graph.IsWalkable(1, 5));
	EXPECT_FALSE(graph.IsWalkable(6, 3)); // Decoder
	EXPECT_FALSE(graph.IsWalkable(1, 0)); // Radio1
}

TEST(RoomLayout, InServiceStationsShowTheirLampAndRiserBarAndTarpedOnesDoNot) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());

	const auto radio1 = FindStation(room, "Radio1");
	for (const auto* part : {"body", "lamp", "riser_bar", "antenna"}) {
		EXPECT_TRUE(radio1.lookup(part)) << part;
	}
	const auto decoder = FindStation(room, "Decoder");
	for (const auto* part : {"body", "lamp", "riser_bar", "reel_l", "reel_r", "crank"}) {
		EXPECT_TRUE(decoder.lookup(part)) << part;
	}
	const auto telex = FindStation(room, "Telex");
	for (const auto* part : {"body", "lamp", "platen", "paper"}) {
		EXPECT_TRUE(telex.lookup(part)) << part;
	}
	// A tap job has no progress to show.
	EXPECT_FALSE(telex.lookup("riser_bar"));

	const auto radio2 = FindStation(room, "Radio2");
	EXPECT_FALSE(radio2.lookup("lamp"));
	EXPECT_FALSE(radio2.lookup("riser_bar"));
	EXPECT_FALSE(FindStation(room, "Keypunch").lookup("lamp"));
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

// A 4x3 Layout holding a Decoder on (0-1,0-1) plus `extraEntities` (JSON entity objects, comma
// separated), and a Shift putting `inService` in service.
struct LayoutCase {
	ScratchDir dir{"signal_room_layout_test"};
	std::string layoutPath = (dir.path / "case.layout.json").generic_string();
	std::string shiftPath = (dir.path / "case.shift.json").generic_string();
	RoomHarness room;

	std::expected<void, std::string>
	Load(const std::string_view extraEntities, const std::string_view inService = R"("Decoder")") {
		std::ofstream{layoutPath} << std::format(
			R"({{"singletons": {{"floor_grid": {{"width": 4, "height": 3, "cell_size": 1.0}}}},
				"entities": [
					{{"name": "Decoder", "components": {{"station": {{"kind": "decoder", "cell": [0, 0],
						"footprint": [2, 2], "worked_sides": ["N", "E", "S", "W"]}}}}}}{}{}
				]}})",
			extraEntities.empty() ? "" : ",",
			extraEntities
		);
		std::ofstream{
			shiftPath
		} << std::format(R"({{"id": "case", "layout": "{}", "in_service": [{}]}})", layoutPath, inService);
		return room.LoadShift(shiftPath);
	}
};

TEST(RoomLayoutErrors, TheBaseCaseLoads) {
	LayoutCase data;
	const auto loaded = data.Load("");
	EXPECT_TRUE(loaded.has_value()) << loaded.error();
}

TEST(RoomLayout, ReloadPicksUpAStationNewlyListedInService) {
	LayoutCase data;
	const auto keypunch = R"({"name": "Keypunch", "components": {"station": {"kind": "keypunch", "cell": [3, 0]}}})";
	ASSERT_TRUE(data.Load(keypunch).has_value());
	ASSERT_TRUE(FindStation(data.room, "Keypunch").has<Tarped>());

	std::ofstream{
		data.shiftPath
	} << std::format(R"({{"id": "case", "layout": "{}", "in_service": ["Decoder", "Keypunch"]}})", data.layoutPath);
	const auto reloaded = data.room.Reload();

	ASSERT_TRUE(reloaded.has_value()) << reloaded.error();
	const auto station = FindStation(data.room, "Keypunch");
	EXPECT_FALSE(station.has<Tarped>());
	EXPECT_TRUE(station.lookup("lamp"));
}

TEST(RoomLayoutErrors, AnUnknownInServiceNameFailsNamingTheShiftFileAndTheName) {
	LayoutCase data;

	const auto loaded = data.Load("", R"("Decoder", "Radio9")");

	ASSERT_FALSE(loaded.has_value());
	EXPECT_THAT(loaded.error(), testing::HasSubstr(data.shiftPath));
	EXPECT_THAT(loaded.error(), testing::HasSubstr("'Radio9'"));
}

TEST(RoomLayoutErrors, AFootprintOffTheGridFailsNamingTheLayoutFileAndThePlacement) {
	LayoutCase data;

	const auto loaded = data.Load(
		R"({"name": "Telex", "components": {"station": {"kind": "telex", "cell": [3, 2], "footprint": [2, 1]}}})"
	);

	ASSERT_FALSE(loaded.has_value());
	EXPECT_THAT(loaded.error(), testing::HasSubstr(data.layoutPath));
	EXPECT_THAT(loaded.error(), testing::HasSubstr("'Telex'"));
	EXPECT_THAT(loaded.error(), testing::HasSubstr("off the 4x3 grid"));
}

TEST(RoomLayoutErrors, OverlappingSolidCellsFailNamingBothPlacementsAndTheCell) {
	LayoutCase data;

	const auto loaded = data.Load(R"({"name": "CounterA", "components": {"counter": {"cell": [1, 1]}}})");

	ASSERT_FALSE(loaded.has_value());
	EXPECT_THAT(loaded.error(), testing::HasSubstr(data.layoutPath));
	EXPECT_THAT(loaded.error(), testing::HasSubstr("'CounterA' overlaps 'Decoder' at cell (1,1)"));
}

TEST(RoomLayoutErrors, ANonSolidPropMayOverlapFurniture) {
	LayoutCase data;

	const auto loaded =
		data.Load(R"({"name": "Rug", "components": {"prop": {"kind": "rug", "cell": [0, 0], "footprint": [3, 2]}}})");

	EXPECT_TRUE(loaded.has_value()) << loaded.error();
}

TEST(RoomLayoutErrors, AnUnknownStationKindFailsNamingThePlacementAndTheKind) {
	LayoutCase data;

	const auto loaded =
		data.Load(R"({"name": "Mystery", "components": {"station": {"kind": "teletype", "cell": [3, 0]}}})");

	ASSERT_FALSE(loaded.has_value());
	EXPECT_THAT(loaded.error(), testing::HasSubstr(data.layoutPath));
	EXPECT_THAT(loaded.error(), testing::HasSubstr("'Mystery' has unknown kind 'teletype'"));
}

TEST(RoomLayoutErrors, AnUnknownPropKindFailsNamingThePlacementAndTheKind) {
	LayoutCase data;

	const auto loaded = data.Load(R"({"name": "Crate", "components": {"prop": {"kind": "crate", "cell": [3, 0]}}})");

	ASSERT_FALSE(loaded.has_value());
	EXPECT_THAT(loaded.error(), testing::HasSubstr("'Crate' has unknown kind 'crate'"));
}

TEST(RoomLayoutErrors, AFailedLoadLeavesNoHalfBuiltRoom) {
	LayoutCase data;

	ASSERT_FALSE(data.Load(R"({"name": "CounterA", "components": {"counter": {"cell": [1, 1]}}})").has_value());

	EXPECT_FALSE(data.room.World().has<FloorGrid>());
	EXPECT_FALSE(FindStation(data.room, "Decoder"));
}

} // namespace
