#include <gtest/gtest.h>

#include <flecs.h>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "engine/core/json_util.hpp"
#include "engine/level/level_module.hpp"

using nlohmann::json;

namespace {

TEST(JsonFieldParsers, AWellFormedArrayParses) {
	const auto j = json::parse(R"({"v": [1, 2, 3], "c": [10, 20, 30, 40]})");

	EXPECT_EQ(engine::core::JVec3(j, "v", glm::vec3{0.0F}), glm::vec3(1.0F, 2.0F, 3.0F));
	EXPECT_EQ(engine::core::JRgba(j, "c", {}).b, 30);
}

TEST(JsonFieldParsers, AnArrayTooShortFallsBackToTheDefault) {
	const auto j = json::parse(R"({"v": [1, 2]})");

	EXPECT_EQ(engine::core::JVec3(j, "v", glm::vec3{7.0F}), glm::vec3{7.0F});
}

TEST(JsonFieldParsers, NonNumericElementsFallBackToTheDefaultInsteadOfThrowing) {
	const auto j = json::parse(R"({"v": ["a", "b", "c"], "p": [1, "x"], "c": [1, 2, "3", 4]})");

	EXPECT_EQ(engine::core::JVec3(j, "v", glm::vec3{7.0F}), glm::vec3{7.0F});
	EXPECT_EQ(engine::core::JVec2(j, "p", glm::vec2{5.0F}), glm::vec2{5.0F});
	EXPECT_EQ(engine::core::JRgba(j, "c", {.r = 9}).r, 9);
}

struct Probe {
	int value{0};
};

struct Marker {};

TEST(LevelLoading, AComponentWithAWronglyTypedValueIsSkippedAndTheRestOfTheEntityLoads) {
	flecs::world world;
	world.import<engine::level::LevelModule>();
	world.component<Probe>();
	world.component<Marker>();
	engine::level::RegisterComponentLoader(world, "probe", [](const flecs::entity e, const json& j) {
		e.set<Probe>({.value = j.at("value").get<int>()}); // throws on a string value
	});
	engine::level::RegisterTag<Marker>(world, "marker");

	const int count = engine::level::LoadLevelFromSource(
		world,
		R"({"entities": [{"name": "Thing", "components": {"probe": {"value": "seven"}, "marker": {}}}]})"
	);

	EXPECT_EQ(count, 1);
	const auto thing = world.lookup("Thing");
	ASSERT_TRUE(thing);
	EXPECT_FALSE(thing.has<Probe>());
	EXPECT_TRUE(thing.has<Marker>());
}

} // namespace
