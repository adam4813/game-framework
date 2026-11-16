#include <array>
#include <memory>

#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include "engine/engine.hpp"

using engine::physics::CollisionLayer;
using engine::physics::LayerBit;
using engine::physics::Ray;

namespace {

constexpr float kTickDt = 1.0F / 60.0F;
constexpr float kTolerance = 1e-3F;

// A floor whose top face is at y = 0, a row of 1 m boxes centered on the line y = 1, z = 0 (Near at x = 0, Far at
// x = 4, an Enemy at x = 8, a Trigger at x = 12), and two characters standing on the floor at z = 6 (Walker, with an
// inner body) and z = 9 (Ghost, without). Character origins are at their feet; their capsules span y = 0..2.
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
	{"name": "Enemy", "components": {
		"rigid_body": {"motion_type": "kinematic", "layer": "enemy", "use_gravity": false},
		"collision_shape": {"type": "box", "box_half_extents": [0.5, 0.5, 0.5]},
		"transform": {"position": [8.0, 1.0, 0.0]}
	}},
	{"name": "Trigger", "components": {
		"rigid_body": {"motion_type": "static", "layer": "trigger"},
		"collision_shape": {"type": "box", "box_half_extents": [0.5, 0.5, 0.5]},
		"transform": {"position": [12.0, 1.0, 0.0]}
	}},
	{"name": "Walker", "components": {
		"character_body": {},
		"collision_shape": {"type": "capsule", "capsule_radius": 0.5, "capsule_height": 1.0, "offset": [0.0, 1.0, 0.0]},
		"transform": {"position": [0.0, 0.0, 6.0]}
	}},
	{"name": "Ghost", "components": {
		"character_body": {"inner_body": false},
		"collision_shape": {"type": "capsule", "capsule_radius": 0.5, "capsule_height": 1.0, "offset": [0.0, 1.0, 0.0]},
		"transform": {"position": [0.0, 0.0, 9.0]}
	}}
]})";

struct RaycastWorld {
	std::unique_ptr<engine::EngineContext> ctx;

	RaycastWorld() {
		ctx = std::make_unique<engine::EngineContext>(std::make_unique<engine::platform::HeadlessPlatform>());
		ctx->GetModuleRegistry().Import(World(), {"physics"});
		engine::level::LoadLevelFromSource(World(), kLevel);
		// A few Ticks so the characters reach their authored spots and settle on the floor.
		for (int i = 0; i < 10; ++i) {
			engine::fixed_step::RunFrame(World(), kTickDt);
		}
	}

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }
	[[nodiscard]] flecs::entity_t Id(const char* name) const { return World().lookup(name).id(); }
};

// Along +x at box height, starting left of Near.
constexpr Ray kAlongRow{.origin = {-4.0F, 1.0F, 0.0F}, .direction = {1.0F, 0.0F, 0.0F}, .max_distance = 100.0F};

TEST(PhysicsRaycast, ClosestHitReportsTheEntityPointNormalAndDistance) {
	const RaycastWorld w;
	const auto hit =
		engine::physics::Raycast(w.World(), {.origin = {2.0F, 5.0F, 0.0F}, .direction = {0.0F, -1.0F, 0.0F}});
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->entity, w.Id("Floor"));
	EXPECT_NEAR(hit->distance, 5.0F, kTolerance);
	EXPECT_NEAR(hit->hit_point.x, 2.0F, kTolerance);
	EXPECT_NEAR(hit->hit_point.y, 0.0F, kTolerance);
	EXPECT_NEAR(hit->normal.y, 1.0F, kTolerance);
}

TEST(PhysicsRaycast, ClosestHitIsTheNearestBodyAndFacesTheRay) {
	const RaycastWorld w;
	const auto hit = engine::physics::Raycast(w.World(), kAlongRow);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->entity, w.Id("Near"));
	EXPECT_NEAR(hit->distance, 3.5F, kTolerance); // Near's left face is at x = -0.5
	EXPECT_NEAR(hit->normal.x, -1.0F, kTolerance);
}

TEST(PhysicsRaycast, DirectionNeedNotBeNormalized) {
	const RaycastWorld w;
	const auto hit = engine::physics::Raycast(
		w.World(),
		{.origin = {2.0F, 5.0F, 0.0F}, .direction = {0.0F, -10.0F, 0.0F}, .max_distance = 6.0F}
	);
	ASSERT_TRUE(hit.has_value());
	EXPECT_NEAR(hit->distance, 5.0F, kTolerance);
}

TEST(PhysicsRaycast, BeyondMaxDistanceMisses) {
	const RaycastWorld w;
	EXPECT_FALSE(
		engine::physics::Raycast(
			w.World(),
			{.origin = {2.0F, 5.0F, 0.0F}, .direction = {0.0F, -1.0F, 0.0F}, .max_distance = 4.0F}
		)
	);
}

TEST(PhysicsRaycast, DegenerateRaysHitNothing) {
	const RaycastWorld w;
	const Ray zero_direction{.origin = {-4.0F, 1.0F, 0.0F}, .direction = {0.0F, 0.0F, 0.0F}};
	const Ray zero_length{.origin = {-4.0F, 1.0F, 0.0F}, .direction = {1.0F, 0.0F, 0.0F}, .max_distance = 0.0F};
	const Ray negative_length{.origin = {-4.0F, 1.0F, 0.0F}, .direction = {1.0F, 0.0F, 0.0F}, .max_distance = -5.0F};
	for (const Ray& ray : {zero_direction, zero_length, negative_length}) {
		EXPECT_FALSE(engine::physics::Raycast(w.World(), ray));
		EXPECT_TRUE(engine::physics::RaycastAll(w.World(), ray).empty());
	}
}

TEST(PhysicsRaycast, StartingInsideAShapeHitsItAtDistanceZero) {
	const RaycastWorld w;
	const auto hit =
		engine::physics::Raycast(w.World(), {.origin = {0.0F, 1.0F, 0.0F}, .direction = {1.0F, 0.0F, 0.0F}});
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->entity, w.Id("Near"));
	EXPECT_NEAR(hit->distance, 0.0F, kTolerance);
}

TEST(PhysicsRaycast, RaycastAllReturnsEveryBodyAlongTheRaySortedByDistance) {
	const RaycastWorld w;
	const auto hits = engine::physics::RaycastAll(w.World(), kAlongRow);
	// The Trigger is left out by the default mask.
	ASSERT_EQ(hits.size(), 3U);
	EXPECT_EQ(hits[0].entity, w.Id("Near"));
	EXPECT_EQ(hits[1].entity, w.Id("Far"));
	EXPECT_EQ(hits[2].entity, w.Id("Enemy"));
	EXPECT_NEAR(hits[0].distance, 3.5F, kTolerance);
	EXPECT_NEAR(hits[1].distance, 7.5F, kTolerance);
	EXPECT_NEAR(hits[2].distance, 11.5F, kTolerance);
	EXPECT_NEAR(hits[1].normal.x, -1.0F, kTolerance);
}

TEST(PhysicsRaycast, LayerMaskSkipsBodiesOnOtherLayers) {
	const RaycastWorld w;
	const auto hit = engine::physics::Raycast(w.World(), kAlongRow, {.layer_mask = LayerBit(CollisionLayer::Enemy)});
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->entity, w.Id("Enemy"));
}

TEST(PhysicsRaycast, IgnoredEntitiesAreSkipped) {
	const RaycastWorld w;
	const std::array ignore{w.Id("Near"), w.Id("Far")};
	const auto hit = engine::physics::Raycast(w.World(), kAlongRow, {.ignore = ignore});
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->entity, w.Id("Enemy"));

	const auto hits = engine::physics::RaycastAll(w.World(), kAlongRow, {.ignore = ignore});
	ASSERT_EQ(hits.size(), 1U);
	EXPECT_EQ(hits[0].entity, w.Id("Enemy"));
}

TEST(PhysicsRaycast, TriggersAreSkippedUnlessTheMaskAsksForThem) {
	const RaycastWorld w;
	const Ray at_trigger{.origin = {10.0F, 1.0F, 0.0F}, .direction = {1.0F, 0.0F, 0.0F}, .max_distance = 10.0F};
	EXPECT_FALSE(engine::physics::Raycast(w.World(), at_trigger));

	const auto hit = engine::physics::Raycast(w.World(), at_trigger, {.layer_mask = LayerBit(CollisionLayer::Trigger)});
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->entity, w.Id("Trigger"));
}

TEST(PhysicsRaycast, ARayHitsACharactersInnerBody) {
	const RaycastWorld w;
	const auto hit = engine::physics::Raycast(
		w.World(),
		{.origin = {-5.0F, 1.0F, 6.0F}, .direction = {1.0F, 0.0F, 0.0F}, .max_distance = 10.0F}
	);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->entity, w.Id("Walker"));
	EXPECT_NEAR(hit->distance, 4.5F, 0.01F); // the capsule's radius is 0.5
	EXPECT_NEAR(hit->normal.x, -1.0F, 0.01F);
}

TEST(PhysicsRaycast, ACharacterWithoutAnInnerBodyIsMissed) {
	const RaycastWorld w;
	EXPECT_FALSE(
		engine::physics::Raycast(
			w.World(),
			{.origin = {-5.0F, 1.0F, 9.0F}, .direction = {1.0F, 0.0F, 0.0F}, .max_distance = 10.0F}
		)
	);
}

TEST(PhysicsRaycast, ACharactersInnerBodyIsOnItsLayer) {
	const RaycastWorld w;
	const Ray at_walker{.origin = {-5.0F, 1.0F, 6.0F}, .direction = {1.0F, 0.0F, 0.0F}, .max_distance = 10.0F};
	EXPECT_FALSE(engine::physics::Raycast(w.World(), at_walker, {.layer_mask = LayerBit(CollisionLayer::Enemy)}));
	EXPECT_TRUE(engine::physics::Raycast(w.World(), at_walker, {.layer_mask = LayerBit(CollisionLayer::Player)}));
}

} // namespace
