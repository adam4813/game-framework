#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>

#include "farm/map_export.hpp"
#include "farm/world_gen.hpp"

using namespace game::farm;

TEST(MapExportTest, writes_a_nonempty_png) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 1337);
	const std::filesystem::path path = std::filesystem::temp_directory_path() / "farm_map_export_test.png";
	std::filesystem::remove(path);

	ASSERT_TRUE(WriteWorldImagePng(world, path.string(), 4));
	ASSERT_TRUE(std::filesystem::exists(path));
	EXPECT_GT(std::filesystem::file_size(path), 0u);

	// PNG magic number.
	std::array<char, 8> header{};
	{
		std::ifstream file(path, std::ios::binary);
		ASSERT_TRUE(file.read(header.data(), header.size()));
	}
	EXPECT_EQ(static_cast<unsigned char>(header[0]), 0x89);
	EXPECT_EQ(header[1], 'P');
	EXPECT_EQ(header[2], 'N');
	EXPECT_EQ(header[3], 'G');

	std::filesystem::remove(path);
}
