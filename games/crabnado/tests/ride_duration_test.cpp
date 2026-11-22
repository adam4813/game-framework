#include <gtest/gtest.h>

#include "enemy_types.hpp"

namespace {

crabnado::EnemyType Type() {
	crabnado::EnemyType type;
	type.tumble_min = 0.5F;
	type.ride_base = 0.5F;
	type.ride_per_height = 0.6F;
	type.ride_max = 2.0F;
	return type;
}

TEST(RideDuration, IsBasePlusPerHeightAboveThreshold) {
	EXPECT_FLOAT_EQ(crabnado::RideDuration(Type(), 1.5F), 0.5F + 0.6F * 1.0F);
}

TEST(RideDuration, IsBaseAtTheThreshold) {
	EXPECT_FLOAT_EQ(crabnado::RideDuration(Type(), 0.5F), 0.5F);
	EXPECT_FLOAT_EQ(crabnado::RideDuration(Type(), 0.1F), 0.5F);
}

TEST(RideDuration, IsCappedAtRideMax) {
	EXPECT_FLOAT_EQ(crabnado::RideDuration(Type(), 20.0F), 2.0F);
}

TEST(RideDuration, ATallerPeakExtendsTheRide) {
	// The horde system keeps the tallest peak and measures the ride's age against RideDuration(peak), so a taller
	// crest mid-ride lengthens the total without resetting the age.
	const crabnado::EnemyType type = Type();
	EXPECT_GT(crabnado::RideDuration(type, 2.0F), crabnado::RideDuration(type, 1.0F));
}

} // namespace
