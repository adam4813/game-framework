#include <gtest/gtest.h>

#include <string>

#include "farm/world_gen.hpp"
#include "farm/world_report.hpp"

using namespace game::farm;

namespace {
bool Contains(const std::string& haystack, const std::string& needle) {
	return haystack.find(needle) != std::string::npos;
}
} // namespace

TEST(WorldReportTest, includes_all_sections) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 1337);
	const std::string report = FormatWorldReport(world);

	EXPECT_TRUE(Contains(report, "seed 1337"));
	EXPECT_TRUE(Contains(report, "## Parameters"));
	EXPECT_TRUE(Contains(report, "## Notable bodies of water"));
	EXPECT_TRUE(Contains(report, "## Cities"));
	EXPECT_TRUE(Contains(report, "## History"));
	EXPECT_TRUE(Contains(report, "Connections:"));
}

TEST(WorldReportTest, lists_every_city_and_the_history_steps) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 42);
	const std::string report = FormatWorldReport(world);

	for (const TownSite& t : world.towns) {
		EXPECT_TRUE(Contains(report, t.name)) << "missing city " << t.name;
	}
	for (const WorldSnapshot& step : world.timeline.steps) {
		EXPECT_TRUE(Contains(report, step.label)) << "missing step " << step.label;
	}
}

TEST(WorldReportTest, is_deterministic) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 7);
	EXPECT_EQ(FormatWorldReport(world), FormatWorldReport(world));
}
