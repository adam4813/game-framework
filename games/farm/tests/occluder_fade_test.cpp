#include <gtest/gtest.h>

#include "farm/occluder_fade.hpp"

using game::farm::FindOccluders;
using game::farm::OccluderView;
using game::farm::WorldObject;

namespace {

// The farm's follow camera: offset (0, 12, -10) from the player, looking at it (farm_world_player.level.json).
// The player stands on tile (10, 10) as a 0.4 x 0.8 x 0.4 cube whose center is 0.5 above the ground.
OccluderView PlayerOnTile10x10() {
	return {
		.viewDir = {0.0F, -12.0F, 10.0F},
		.focusCenter = {5.25F, 0.5F, 5.25F},
		.focusHalfExtents = {0.2F, 0.4F, 0.2F},
		.tileSize = 0.5F,
		.quadWidthTiles = 1.0F,
	};
}

WorldObject Tree(const int x, const int z, const float heightTiles = 4.0F) {
	return {.footprint = {{x, z}}, .kind = "tree_oak", .height = heightTiles, .blocksMovement = true};
}

} // namespace

TEST(OccluderFadeTest, TallTreeDirectlyInFrontOfThePlayerOccludes) {
	EXPECT_EQ(FindOccluders({Tree(10, 9)}, PlayerOnTile10x10()), (std::vector<size_t>{0}));
}

TEST(OccluderFadeTest, TreeBehindThePlayerNeverOccludes) {
	// Its canopy rises above the player on screen, but the player stands in front of it.
	EXPECT_TRUE(FindOccluders({Tree(10, 11, 8.0F)}, PlayerOnTile10x10()).empty());
}

TEST(OccluderFadeTest, TreeOnTheSameRowDoesNotOcclude) {
	EXPECT_TRUE(FindOccluders({Tree(10, 10)}, PlayerOnTile10x10()).empty());
}

TEST(OccluderFadeTest, TreeInTheNeighboringColumnDoesNotOcclude) {
	EXPECT_TRUE(FindOccluders({Tree(9, 9), Tree(11, 9)}, PlayerOnTile10x10()).empty());
}

TEST(OccluderFadeTest, ShortObjectInFrontNeverOccludes) {
	EXPECT_TRUE(FindOccluders({Tree(10, 9, 1.0F)}, PlayerOnTile10x10()).empty());
}

TEST(OccluderFadeTest, TallTreeTwoRowsInFrontStillReachesThePlayerOnScreen) {
	// A 2-unit tree at z = 4.25 tops out at screen height 0.64 * 2 + 0.768 * 4.25 = 4.54, above the
	// player's feet at 0.64 * 0.1 + 0.768 * 5.05 = 3.94.
	EXPECT_EQ(FindOccluders({Tree(10, 8)}, PlayerOnTile10x10()), (std::vector<size_t>{0}));
}

TEST(OccluderFadeTest, TreeFarInFrontIsBelowThePlayerOnScreen) {
	// Top at 0.64 * 2 + 0.768 * 1.25 = 2.24, below the player's feet at 3.94.
	EXPECT_TRUE(FindOccluders({Tree(10, 2)}, PlayerOnTile10x10()).empty());
}

TEST(OccluderFadeTest, MultiTileBuildingOccludesOnceWhenAnyWallTileCovers) {
	const WorldObject house{.footprint = {{8, 9}, {9, 9}, {10, 9}, {11, 9}}, .kind = "house", .height = 3.0F};
	EXPECT_EQ(FindOccluders({Tree(2, 2), house}, PlayerOnTile10x10()), (std::vector<size_t>{1}));
}
