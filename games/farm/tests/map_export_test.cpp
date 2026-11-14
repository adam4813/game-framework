#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>

#include "farm/map_export.hpp"
#include "farm/world_gen.hpp"

using namespace game::farm;

TEST(MapExportTest, writes_a_nonempty_png) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 1337);
	const std::filesystem::path path =
		std::filesystem::temp_directory_path() / "farm_map_export_test.png";
	std::filesystem::remove(path);

	ASSERT_TRUE(WriteWorldImagePng(world, path.string(), 4));
	ASSERT_TRUE(std::filesystem::exists(path));
	EXPECT_GT(std::filesystem::file_size(path), 0u);

	// PNG magic number.
	FILE* f = std::fopen(path.string().c_str(), "rb");
	ASSERT_NE(f, nullptr);
	unsigned char header[8] = {0};
	ASSERT_EQ(std::fread(header, 1, 8, f), 8u);
	std::fclose(f);
	EXPECT_EQ(header[0], 0x89);
	EXPECT_EQ(header[1], 'P');
	EXPECT_EQ(header[2], 'N');
	EXPECT_EQ(header[3], 'G');

	std::filesystem::remove(path);
}
