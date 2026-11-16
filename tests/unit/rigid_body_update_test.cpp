#include <memory>

#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include "engine/engine.hpp"

using engine::physics::CollisionLayer;
using engine::physics::CollisionShape;
using engine::physics::LayerBit;
using engine::physics::MotionType;
using engine::physics::PhysicsImpulse;
using engine::physics::PhysicsVelocity;
using engine::physics::Ray;
using engine::physics::RigidBody;
using engine::spatial::WorldTransform;

namespace {

constexpr float kTickDt = 1.0F / 60.0F;
constexpr float kTolerance = 1e-3F;

// No floor: Faller and Twin fall freely side by side, Ledge hangs in the air (static), Floater drifts in zero gravity
// with no damping, and Wall is a 1 m static box centered on (0, 1, 10).
constexpr auto kLevel = R"({"entities": [
	{"name": "Faller", "components": {
		"rigid_body": {"motion_type": "dynamic"},
		"collision_shape": {"type": "box", "box_half_extents": [0.5, 0.5, 0.5]},
		"transform": {"position": [0.0, 50.0, 0.0]}
	}},
	{"name": "Twin", "components": {
		"rigid_body": {"motion_type": "dynamic"},
		"collision_shape": {"type": "box", "box_half_extents": [0.5, 0.5, 0.5]},
		"transform": {"position": [5.0, 50.0, 0.0]}
	}},
	{"name": "Ledge", "components": {
		"rigid_body": {"motion_type": "static"},
		"collision_shape": {"type": "box", "box_half_extents": [0.5, 0.5, 0.5]},
		"transform": {"position": [-5.0, 50.0, 0.0]}
	}},
	{"name": "Floater", "components": {
		"rigid_body": {"motion_type": "dynamic", "use_gravity": false, "linear_damping": 0.0, "angular_damping": 0.0},
		"collision_shape": {"type": "sphere", "sphere_radius": 0.5},
		"transform": {"position": [0.0, 0.0, -20.0]}
	}},
	{"name": "Wall", "components": {
		"rigid_body": {"motion_type": "static"},
		"collision_shape": {"type": "box", "box_half_extents": [0.5, 0.5, 0.5]},
		"transform": {"position": [0.0, 1.0, 10.0]}
	}}
]})";

struct UpdateWorld {
	std::unique_ptr<engine::EngineContext> ctx;

	UpdateWorld() {
		ctx = std::make_unique<engine::EngineContext>(std::make_unique<engine::platform::HeadlessPlatform>());
		ctx->GetModuleRegistry().Import(World(), {"physics"});
		engine::level::LoadLevelFromSource(World(), kLevel);
		Ticks(10);
	}

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }
	[[nodiscard]] flecs::entity Get(const char* name) const { return World().lookup(name); }
	[[nodiscard]] float Height(const char* name) const { return Get(name).get<WorldTransform>().position.y; }

	void Ticks(const int count) const {
		for (int i = 0; i < count; ++i) {
			engine::fixed_step::RunFrame(World(), kTickDt);
		}
	}

	// Copies the entity's RigidBody, lets `change` edit it, and sets it back, as a script or REST write would.
	template<typename Change>
	void Edit(const char* name, Change change) const {
		RigidBody rb = Get(name).get<RigidBody>();
		change(rb);
		Get(name).set<RigidBody>(rb);
	}
};

// Along +x at the Wall's height, starting 4 m left of its center.
constexpr Ray kAtWall{.origin = {-4.0F, 1.0F, 10.0F}, .direction = {1.0F, 0.0F, 0.0F}, .max_distance = 20.0F};

TEST(RigidBodyUpdate, ADynamicBodySetToStaticStopsFalling) {
	const UpdateWorld w;
	w.Edit("Faller", [](RigidBody& rb) { rb.motion_type = MotionType::Static; });
	const float stopped_at = w.Height("Faller");
	w.Ticks(30);
	EXPECT_NEAR(w.Height("Faller"), stopped_at, kTolerance);
	EXPECT_LT(w.Height("Twin"), stopped_at - 1.0F); // the untouched twin kept falling
}

TEST(RigidBodyUpdate, AStaticBodySetToDynamicStartsFalling) {
	const UpdateWorld w;
	w.Edit("Ledge", [](RigidBody& rb) { rb.motion_type = MotionType::Dynamic; });
	w.Ticks(30);
	// 0.5 s of free fall is about 1.2 m.
	EXPECT_LT(w.Height("Ledge"), 49.0F);
}

TEST(RigidBodyUpdate, AChangedMassChangesTheResponseToAnImpulse) {
	const UpdateWorld w;
	w.Edit("Floater", [](RigidBody& rb) { rb.mass = 4.0F; });
	w.Get("Floater").set<PhysicsImpulse>({.impulse = {8.0F, 0.0F, 0.0F}});
	w.Ticks(1);
	// 8 N·s on 4 kg.
	EXPECT_NEAR(w.Get("Floater").get<PhysicsVelocity>().linear.x, 2.0F, kTolerance);
}

TEST(RigidBodyUpdate, AChangedLayerReachesRaycasts) {
	const UpdateWorld w;
	const auto enemies_only = engine::physics::RaycastFilter{.layer_mask = LayerBit(CollisionLayer::Enemy)};
	EXPECT_FALSE(engine::physics::Raycast(w.World(), kAtWall, enemies_only));

	w.Edit("Wall", [](RigidBody& rb) { rb.layer = CollisionLayer::Enemy; });
	const auto hit = engine::physics::Raycast(w.World(), kAtWall, enemies_only);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->entity, w.Get("Wall").id());
}

TEST(RigidBodyUpdate, AChangedShapeRecreatesTheBodyWithTheNewShape) {
	const UpdateWorld w;
	auto shape = w.Get("Wall").get<CollisionShape>();
	shape.box_half_extents = {2.0F, 2.0F, 2.0F};
	w.Get("Wall").set<CollisionShape>(shape);

	const auto hit = engine::physics::Raycast(w.World(), kAtWall);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->entity, w.Get("Wall").id());
	EXPECT_NEAR(hit->distance, 2.0F, kTolerance); // the face moved from x = -0.5 to x = -2
}

TEST(RigidBodyUpdate, AChangedShapeKeepsAFallingBodysPositionAndVelocity) {
	const UpdateWorld w;
	auto shape = w.Get("Faller").get<CollisionShape>();
	shape.box_half_extents = {0.25F, 0.25F, 0.25F};
	w.Get("Faller").set<CollisionShape>(shape);
	w.Ticks(10);
	EXPECT_NEAR(w.Height("Faller"), w.Height("Twin"), kTolerance);
}

TEST(RigidBodyUpdate, SettingTheSameValuesAgainLeavesTheBodyAlone) {
	const UpdateWorld w;
	w.Edit("Faller", [](RigidBody&) {});
	w.Get("Faller").set<CollisionShape>(w.Get("Faller").get<CollisionShape>());
	w.Ticks(10);
	EXPECT_NEAR(w.Height("Faller"), w.Height("Twin"), kTolerance);
	EXPECT_NEAR(
		w.Get("Faller").get<PhysicsVelocity>().linear.y,
		w.Get("Twin").get<PhysicsVelocity>().linear.y,
		kTolerance
	);
}

} // namespace
