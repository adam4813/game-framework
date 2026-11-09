#include <gtest/gtest.h>

#include "engine/animation/animation_playback.hpp"

using engine::animation::AdvanceAnimation;
using engine::animation::AnimationPlayer;

namespace {

AnimationPlayer BoundPlayer(const float duration) {
	AnimationPlayer player;
	player.clip_index = 0;
	player.clip_duration = duration;
	return player;
}

} // namespace

TEST(AdvanceAnimationTest, a_looping_clip_wraps_past_its_end) {
	auto player = BoundPlayer(2.0F);
	player.time = 1.5F;
	AdvanceAnimation(player, 1.0F);
	EXPECT_NEAR(player.time, 0.5F, 1e-5F);
	EXPECT_TRUE(player.playing);
}

TEST(AdvanceAnimationTest, a_non_looping_clip_stops_at_its_end) {
	auto player = BoundPlayer(2.0F);
	player.loop = false;
	player.time = 1.5F;
	AdvanceAnimation(player, 1.0F);
	EXPECT_FLOAT_EQ(player.time, 2.0F);
	EXPECT_FALSE(player.playing);
}

TEST(AdvanceAnimationTest, speed_scales_the_advance) {
	auto player = BoundPlayer(10.0F);
	player.speed = 2.5F;
	AdvanceAnimation(player, 0.5F);
	EXPECT_FLOAT_EQ(player.time, 1.25F);
}

TEST(AdvanceAnimationTest, a_negative_speed_counts_as_zero) {
	auto player = BoundPlayer(10.0F);
	player.speed = -3.0F;
	player.time = 4.0F;
	AdvanceAnimation(player, 1.0F);
	EXPECT_FLOAT_EQ(player.time, 4.0F);
}

TEST(AdvanceAnimationTest, a_paused_player_does_not_advance) {
	auto player = BoundPlayer(10.0F);
	player.playing = false;
	AdvanceAnimation(player, 1.0F);
	EXPECT_FLOAT_EQ(player.time, 0.0F);
}

TEST(AdvanceAnimationTest, an_unbound_player_is_left_untouched) {
	AnimationPlayer player; // clip_index == -1: no Animation Kind has bound it
	player.time = 0.25F;
	EXPECT_FALSE(AdvanceAnimation(player, 1.0F));
	EXPECT_FLOAT_EQ(player.time, 0.25F);
	EXPECT_TRUE(player.playing);
}
