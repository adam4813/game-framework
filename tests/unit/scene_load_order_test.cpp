#include <memory>

#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include "engine/engine.hpp"
#include "engine/physics/jolt/jolt_backend.hpp"
#include "null_platform.hpp"

using engine::physics::JoltBackend;
using engine::spatial::Transform;
using engine::spatial::WorldTransform;

namespace {

// rigid_body is authored before transform, matching the alphabetical key order the level loader sees.
constexpr auto kFloorLevel = R"({"entities": [
	{"name": "Floor", "components": {
		"rigid_body": {"motion_type": "static", "mass": 0.0, "use_gravity": false},
		"collision_shape": {"type": "box", "box_half_extents": [5.0, 0.25, 5.0]},
		"transform": {"position": [1.0, -0.5, 2.0], "scale": [10.0, 0.5, 10.0]}
	}}
]})";

constexpr auto kParentedLevel = R"({"entities": [
	{"name": "Parent", "components": {"transform": {"position": [10.0, 0.0, 0.0]}},
	 "children": [{"name": "Child", "components": {"transform": {"position": [0.0, 2.0, 0.0]}}}]}
]})";

enum class LoadMode { Undeferred, Deferred };

// Same engine stack as the game. Load() runs the level the way a scene Load can: undeferred (the
// scene module suspends deferral) or inside a deferred region (merged on defer_end).
struct LoadWorld {
	std::unique_ptr<engine::EngineContext> ctx;

	LoadWorld() {
		ctx = std::make_unique<engine::EngineContext>(std::make_unique<NullPlatform>());
		ctx->GetModuleRegistry().Import(World(), {"physics"});
	}

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }

	void Load(const LoadMode mode, const char* level) const {
		if (mode == LoadMode::Deferred) {
			World().defer_begin();
		}
		engine::level::LoadLevelFromSource(World(), level);
		if (mode == LoadMode::Deferred) {
			World().defer_end();
		}
	}
};

class SceneLoadOrder : public testing::TestWithParam<LoadMode> {};

TEST_P(SceneLoadOrder, ARigidBodyAuthoredBeforeItsTransformGetsThatTransform) {
	const LoadWorld w;
	w.Load(GetParam(), kFloorLevel);

	const auto floor = w.World().lookup("Floor");
	ASSERT_TRUE(floor);
	const auto& wt = floor.get<WorldTransform>();
	EXPECT_EQ(wt.position, glm::vec3(1.0F, -0.5F, 2.0F));
	EXPECT_EQ(wt.scale, glm::vec3(10.0F, 0.5F, 10.0F));

	const auto& physics = w.World().get<JoltBackend>();
	ASSERT_TRUE(physics.HasBody(floor.id()));
	const auto body = physics.GetPhysicsSyncResult(floor.id());
	EXPECT_NEAR(body.position.x, 1.0F, 1e-4F);
	EXPECT_NEAR(body.position.y, -0.5F, 1e-4F);
	EXPECT_NEAR(body.position.z, 2.0F, 1e-4F);
}

TEST_P(SceneLoadOrder, AStaticBodyStaysPutAfterTheWorldSteps) {
	const LoadWorld w;
	w.Load(GetParam(), kFloorLevel);
	for (int i = 0; i < 30; ++i) {
		w.World().progress(1.0F / 60.0F);
	}

	const auto floor = w.World().lookup("Floor");
	EXPECT_NEAR(floor.get<WorldTransform>().position.y, -0.5F, 1e-4F);
}

TEST_P(SceneLoadOrder, AChildsWorldTransformIsComposedWithItsParentBeforeTheFirstFrame) {
	const LoadWorld w;
	w.Load(GetParam(), kParentedLevel);

	const auto child = w.World().lookup("Parent::Child");
	ASSERT_TRUE(child);
	EXPECT_EQ(child.get<WorldTransform>().position, glm::vec3(10.0F, 2.0F, 0.0F));
}

INSTANTIATE_TEST_SUITE_P(
	LoadModes, SceneLoadOrder, testing::Values(LoadMode::Undeferred, LoadMode::Deferred),
	[](const testing::TestParamInfo<LoadMode>& info) {
		return info.param == LoadMode::Undeferred ? "Undeferred" : "Deferred";
	}
);

// A body spawned at runtime (RigidBody set before Transform) starts where its Transform says, not at
// the origin until a later sync moves it.
TEST(RuntimeSpawn, ARigidBodySpawnedBeforeItsTransformStartsAtThatTransform) {
	const LoadWorld w;
	const auto crab = w.World()
						  .entity("Crab")
						  .set<engine::physics::CollisionShape>({})
						  .set<engine::physics::RigidBody>({})
						  .set<Transform>({.position = {4.0F, 3.0F, -2.0F}});

	const auto body = w.World().get<JoltBackend>().GetPhysicsSyncResult(crab.id());
	EXPECT_NEAR(body.position.x, 4.0F, 1e-4F);
	EXPECT_NEAR(body.position.y, 3.0F, 1e-4F);
	EXPECT_NEAR(body.position.z, -2.0F, 1e-4F);
}

// Setting a rigid body's Transform after it exists teleports it (save restore relies on this).
TEST(RuntimeSpawn, SettingARigidBodysTransformTeleportsIt) {
	const LoadWorld w;
	const auto cube = w.World()
						  .entity("Cube")
						  .set<Transform>({})
						  .set<engine::physics::CollisionShape>({})
						  .set<engine::physics::RigidBody>({});
	cube.get_mut<Transform>().position = {0.0F, 7.0F, 0.0F};
	cube.modified<Transform>();

	EXPECT_NEAR(w.World().get<JoltBackend>().GetPhysicsSyncResult(cube.id()).position.y, 7.0F, 1e-4F);
}

} // namespace
