#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "engine/engine.hpp"

namespace {

constexpr float kTickDt = 1.0F / 60.0F;
constexpr float kTolerance = 1e-3F;

// A floor whose top face is at y = 0 and a row of 1 m boxes centered on the line y = 1, z = 0: Near at x = 0, Far at
// x = 4 and a Trigger-layer box at x = 8.
constexpr auto kLevel = R"({"entities": [
	{"name": "Floor", "components": {
		"rigid_body": {"motion_type": "static"},
		"collision_shape": {"type": "box", "box_half_extents": [50.0, 0.25, 50.0]},
		"transform": {"position": [0.0, -0.25, 0.0]}
	}},
	{"name": "Near", "components": {
		"rigid_body": {"motion_type": "static", "layer": "obstacle"},
		"collision_shape": {"type": "box", "box_half_extents": [0.5, 0.5, 0.5]},
		"transform": {"position": [0.0, 1.0, 0.0]}
	}},
	{"name": "Far", "components": {
		"rigid_body": {"motion_type": "static", "layer": "obstacle"},
		"collision_shape": {"type": "box", "box_half_extents": [0.5, 0.5, 0.5]},
		"transform": {"position": [4.0, 1.0, 0.0]}
	}},
	{"name": "Trigger", "components": {
		"rigid_body": {"motion_type": "static", "layer": "trigger"},
		"collision_shape": {"type": "box", "box_half_extents": [0.5, 0.5, 0.5]},
		"transform": {"position": [8.0, 1.0, 0.0]}
	}}
]})";

// Where a test script writes what its queries returned.
struct RayProbe {
	bool hit{false};
	float distance{0.0F};
	float normal_y{0.0F};
	std::string entity_name;
	bool miss_hit{true};
	std::string miss_name{"unset"};
	std::string ignored_name;
	std::string trigger_name;
	int count{-1};
	std::string names;
	float last_distance{0.0F};
	int empty_count{-1};
};

// A world with the level loaded and a "Prober" entity running `source`, after a few Frames.
struct ScriptWorld {
	std::unique_ptr<engine::EngineContext> ctx;

	explicit ScriptWorld(const char* source) {
		ctx = std::make_unique<engine::EngineContext>(std::make_unique<engine::platform::HeadlessPlatform>());
		flecs::world& world = World();
		ctx->GetModuleRegistry().Import(world, {"physics"});
		world.component<RayProbe>()
			.member("hit", &RayProbe::hit)
			.member("distance", &RayProbe::distance)
			.member("normal_y", &RayProbe::normal_y)
			.member("entity_name", &RayProbe::entity_name)
			.member("miss_hit", &RayProbe::miss_hit)
			.member("miss_name", &RayProbe::miss_name)
			.member("ignored_name", &RayProbe::ignored_name)
			.member("trigger_name", &RayProbe::trigger_name)
			.member("count", &RayProbe::count)
			.member("names", &RayProbe::names)
			.member("last_distance", &RayProbe::last_distance)
			.member("empty_count", &RayProbe::empty_count);
		engine::scripting::RegisterComponentForScripts(world, world.component<RayProbe>());
		engine::level::LoadLevelFromSource(world, kLevel);

		const auto prober = world.entity("Prober").set<RayProbe>({});
		world.entity().child_of(prober).set<engine::scripting::ScriptComponent>({.inline_source = source});
		for (int i = 0; i < 3; ++i) {
			engine::fixed_step::RunFrame(world, kTickDt);
		}
	}

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }
	[[nodiscard]] const RayProbe& Probe() const { return World().lookup("Prober").get<RayProbe>(); }
};

TEST(RaycastScript, RaycastReturnsTheClosestHitOrAMiss) {
	const ScriptWorld w(R"(
		void Tick(Entity self, float dt) {
			RayProbe@ p = self.GetRayProbe();
			RaycastHit down = Raycast(vec3(2, 5, 0), vec3(0, -1, 0), 100.0f, RaycastFilter());
			p.hit = down.hit;
			p.distance = down.distance;
			p.normal_y = down.normal.y;
			p.entity_name = down.entity.GetName();

			RaycastHit up = Raycast(vec3(2, 5, 0), vec3(0, 1, 0), 100.0f, RaycastFilter());
			p.miss_hit = up.hit;
			p.miss_name = up.entity.GetName();
		}
	)");
	const RayProbe& probe = w.Probe();
	EXPECT_TRUE(probe.hit);
	EXPECT_NEAR(probe.distance, 5.0F, kTolerance);
	EXPECT_NEAR(probe.normal_y, 1.0F, kTolerance);
	EXPECT_EQ(probe.entity_name, "Floor");
	EXPECT_FALSE(probe.miss_hit);
	EXPECT_EQ(probe.miss_name, "");
}

TEST(RaycastScript, RaycastFilterIgnoresAnEntityAndMasksLayers) {
	const ScriptWorld w(R"(
		void Tick(Entity self, float dt) {
			RayProbe@ p = self.GetRayProbe();
			RaycastFilter skip_near;
			skip_near.ignore = Raycast(vec3(-4, 1, 0), vec3(1, 0, 0), 100.0f, RaycastFilter()).entity;
			p.ignored_name = Raycast(vec3(-4, 1, 0), vec3(1, 0, 0), 100.0f, skip_near).entity.GetName();

			RaycastFilter triggers_only;
			triggers_only.layer_mask = 1 << Layer_Trigger;
			p.trigger_name = Raycast(vec3(-4, 1, 0), vec3(1, 0, 0), 100.0f, triggers_only).entity.GetName();
		}
	)");
	EXPECT_EQ(w.Probe().ignored_name, "Far");
	EXPECT_EQ(w.Probe().trigger_name, "Trigger");
}

// RaycastAll hands back an array (the scriptarray add-on), written both as `T[]` and `array<T>`.
TEST(RaycastScript, RaycastAllReturnsAnArrayOfHitsSortedByDistance) {
	const ScriptWorld w(R"(
		void Tick(Entity self, float dt) {
			RayProbe@ p = self.GetRayProbe();
			RaycastHit[]@ hits = RaycastAll(vec3(-4, 1, 0), vec3(1, 0, 0), 100.0f, RaycastFilter());
			p.count = hits.length();
			p.names = "";
			for (uint i = 0; i < hits.length(); ++i) {
				p.names += hits[i].entity.GetName() + ";";
			}
			p.last_distance = hits[hits.length() - 1].distance;

			array<RaycastHit>@ none = RaycastAll(vec3(-4, 1, 0), vec3(-1, 0, 0), 100.0f, RaycastFilter());
			p.empty_count = none.length();
		}
	)");
	const RayProbe& probe = w.Probe();
	EXPECT_EQ(probe.count, 2); // the Trigger is left out by the default mask
	EXPECT_EQ(probe.names, "Near;Far;");
	EXPECT_NEAR(probe.last_distance, 7.5F, kTolerance);
	EXPECT_EQ(probe.empty_count, 0);
}

} // namespace
