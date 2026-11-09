#include <gtest/gtest.h>

#include <vector>

#include "engine/render/point_light_packing.hpp"

using engine::platform::LightParams;
using engine::render::AddPointLight;
using engine::render::PointLightSource;

namespace {

std::vector<PointLightSource> MakeSources(const int count) {
	std::vector<PointLightSource> sources;
	for (int i = 0; i < count; ++i) {
		sources.push_back(
			{.light = {.intensity = static_cast<float>(i + 1)}, .position = {static_cast<float>(i), 0.0F, 0.0F}}
		);
	}
	return sources;
}

// Packs every source in order and returns how many were dropped.
int PackAll(const std::vector<PointLightSource>& sources, LightParams& params) {
	int dropped = 0;
	for (const auto& source : sources) {
		if (!AddPointLight(params, source)) ++dropped;
	}
	return dropped;
}

} // namespace

TEST(PointLightPackingTest, no_lights_leaves_count_zero) {
	LightParams params;
	EXPECT_EQ(PackAll({}, params), 0);
	EXPECT_EQ(params.point_light_count, 0);
}

TEST(PointLightPackingTest, one_light_fills_slot_zero_with_its_fields) {
	LightParams params;
	const std::vector<PointLightSource> sources{
		{.light =
			 {.color = {.r = 10, .g = 20, .b = 30, .a = 255},
			  .intensity = 2.0F,
			  .constant = 1.0F,
			  .linear = 0.5F,
			  .quadratic = 0.25F},
		 .position = {1.0F, 2.0F, 3.0F}},
	};
	EXPECT_EQ(PackAll(sources, params), 0);
	ASSERT_EQ(params.point_light_count, 1);
	const auto& packed = params.point_lights[0];
	EXPECT_EQ(packed.position, glm::vec3(1.0F, 2.0F, 3.0F));
	EXPECT_EQ(packed.color.g, 20);
	EXPECT_FLOAT_EQ(packed.intensity, 2.0F);
	EXPECT_FLOAT_EQ(packed.linear, 0.5F);
	EXPECT_FLOAT_EQ(packed.quadratic, 0.25F);
}

TEST(PointLightPackingTest, four_lights_pack_in_order) {
	LightParams params;
	const auto sources = MakeSources(4);
	EXPECT_EQ(PackAll(sources, params), 0);
	ASSERT_EQ(params.point_light_count, 4);
	for (int i = 0; i < 4; ++i) {
		EXPECT_FLOAT_EQ(params.point_lights[static_cast<std::size_t>(i)].position.x, static_cast<float>(i));
	}
}

TEST(PointLightPackingTest, fifth_light_is_dropped_and_reported) {
	LightParams params;
	const auto sources = MakeSources(5);
	EXPECT_EQ(PackAll(sources, params), 1);
	ASSERT_EQ(params.point_light_count, 4);
	EXPECT_FLOAT_EQ(params.point_lights[3].position.x, 3.0F);
}
