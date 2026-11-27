#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include "engine/engine.hpp"

using engine::physics::CharacterMove;
using engine::physics::CharacterState;
using engine::physics::Gravity;
using engine::spatial::Transform;

namespace {

// The capsule's origin is at its feet: offset 1.0 lifts the center by half the height plus the radius.
// The floor's top face is at y = 0. character_body is authored before collision_shape, as the loader orders them.
constexpr auto kLevel = R"({"entities": [
	{"name": "Floor", "components": {
		"rigid_body": {"motion_type": "static", "mass": 0.0, "use_gravity": false},
		"collision_shape": {"type": "box", "box_half_extents": [20.0, 0.25, 20.0]},
		"transform": {"position": [0.0, -0.25, 0.0]}
	}},
	{"name": "Walker", "components": {
		"character_body": {},
		"collision_shape": {"type": "capsule", "capsule_radius": 0.5, "capsule_height": 1.0, "offset": [0.0, 1.0, 0.0]},
		"transform": {"position": [0.0, 3.0, 0.0]}
	}}
]})";

struct CharacterWorld {
	std::unique_ptr<engine::EngineContext> ctx;

	CharacterWorld() {
		ctx = std::make_unique<engine::EngineContext>(std::make_unique<engine::platform::HeadlessPlatform>());
		ctx->GetModuleRegistry().Import(World(), {"physics"});
		engine::level::LoadLevelFromSource(World(), kLevel);
	}

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }

	void Step(const int frames) const {
		for (int i = 0; i < frames; ++i) {
			engine::fixed_step::RunFrame(World(), 1.0F / 60.0F);
		}
	}
};

TEST(CharacterBody, FallsUnderWorldGravityAndLandsOnTheFloor) {
	const CharacterWorld w;
	w.Step(120);

	const auto walker = w.World().lookup("Walker");
	EXPECT_TRUE(walker.get<CharacterState>().grounded);
	EXPECT_NEAR(walker.get<Transform>().position.y, 0.0F, 0.05F);
}

TEST(CharacterBody, WalksAtTheRequestedPlanarVelocity) {
	const CharacterWorld w;
	w.Step(120);
	const auto walker = w.World().lookup("Walker");
	const float start_x = walker.get<Transform>().position.x;

	walker.set<CharacterMove>({.velocity = {2.0F, 0.0F, 0.0F}});
	w.Step(60);

	EXPECT_NEAR(walker.get<Transform>().position.x - start_x, 2.0F, 0.2F);
	EXPECT_TRUE(walker.get<CharacterState>().grounded);
}

TEST(CharacterBody, UnderZeroGravityLiftsOffTheFloorAtTheFullRequestedVelocity) {
	const CharacterWorld w;
	w.Step(120);
	const auto walker = w.World().lookup("Walker");
	ASSERT_TRUE(walker.get<CharacterState>().grounded);
	const glm::vec3 start = walker.get<Transform>().position;

	walker.set<Gravity>({.acceleration = glm::vec3{0.0F}});
	walker.set<CharacterMove>({.velocity = {1.0F, 2.0F, 0.0F}});
	w.Step(60);

	const glm::vec3 moved = walker.get<Transform>().position - start;
	EXPECT_NEAR(moved.x, 1.0F, 0.1F);
	EXPECT_NEAR(moved.y, 2.0F, 0.1F);
	EXPECT_FALSE(walker.get<CharacterState>().grounded);
}

TEST(CharacterBody, UnderZeroGravityHoldsStillWithoutACharacterMoveRatherThanDrifting) {
	const CharacterWorld w;
	const auto walker = w.World().lookup("Walker");
	walker.set<Gravity>({.acceleration = glm::vec3{0.0F}});
	// Already rising, as a Server correction might leave it: free movement doesn't carry that speed on.
	engine::physics::RestoreCharacter(walker, {0.0F, 3.0F, 0.0F}, {0.0F, 2.0F, 0.0F});

	w.Step(60);

	EXPECT_NEAR(walker.get<Transform>().position.y, 3.0F, 0.01F);
}

// The floor's entity origin is at y = 0, and its offset moves the box down so its top face is at y = 0, not 0.25.
TEST(CharacterBody, LandsOnAPrimitiveColliderAtItsAuthoredOffset) {
	const auto ctx = std::make_unique<engine::EngineContext>(std::make_unique<engine::platform::HeadlessPlatform>());
	auto& world = *ctx->GetWorld();
	ctx->GetModuleRegistry().Import(world, {"physics"});
	engine::level::LoadLevelFromSource(world, R"({"entities": [
		{"name": "Floor", "components": {
			"rigid_body": {"motion_type": "static", "mass": 0.0, "use_gravity": false},
			"collision_shape": {"type": "box", "box_half_extents": [20.0, 0.25, 20.0], "offset": [0.0, -0.25, 0.0]},
			"transform": {"position": [0.0, 0.0, 0.0]}
		}},
		{"name": "Walker", "components": {
			"character_body": {},
			"collision_shape": {"type": "capsule", "capsule_radius": 0.5, "capsule_height": 1.0, "offset": [0.0, 1.0, 0.0]},
			"transform": {"position": [0.0, 3.0, 0.0]}
		}}
	]})");
	for (int i = 0; i < 120; ++i) {
		engine::fixed_step::RunFrame(world, 1.0F / 60.0F);
	}

	const auto walker = world.lookup("Walker");
	EXPECT_TRUE(walker.get<CharacterState>().grounded);
	EXPECT_NEAR(walker.get<Transform>().position.y, 0.0F, 0.05F);
}

TEST(CharacterBody, RotatingTheTransformNeverTeleportsButMovingItDoes) {
	const CharacterWorld w;
	w.Step(120);
	const auto walker = w.World().lookup("Walker");

	walker.get_mut<Transform>().rotation = {0.0F, 1.0F, 0.0F};
	w.Step(10);
	EXPECT_NEAR(walker.get<Transform>().position.y, 0.0F, 0.05F);

	walker.get_mut<Transform>().position = {5.0F, 3.0F, 0.0F};
	w.Step(1);
	EXPECT_NEAR(walker.get<Transform>().position.x, 5.0F, 0.05F);
	EXPECT_GT(walker.get<Transform>().position.y, 1.0F);
}

// A floor whose top face is at y = 0, a walker at the origin and a second character three meters along +x whose
// `inner_body` is `blocker_inner_body`.
std::string InnerBodyLevel(const bool blocker_inner_body) {
	return std::string{R"({"entities": [
		{"name": "Floor", "components": {
			"rigid_body": {"motion_type": "static"},
			"collision_shape": {"type": "box", "box_half_extents": [20.0, 0.25, 20.0]},
			"transform": {"position": [0.0, -0.25, 0.0]}
		}},
		{"name": "Walker", "components": {
			"character_body": {},
			"collision_shape": {"type": "capsule", "capsule_radius": 0.5, "capsule_height": 1.0, "offset": [0.0, 1.0, 0.0]},
			"transform": {"position": [0.0, 0.0, 0.0]}
		}},
		{"name": "Blocker", "components": {
			"character_body": {"inner_body": )"}
		   + (blocker_inner_body ? "true" : "false")
		   + R"(},
			"collision_shape": {"type": "capsule", "capsule_radius": 0.5, "capsule_height": 1.0, "offset": [0.0, 1.0, 0.0]},
			"transform": {"position": [3.0, 0.0, 0.0]}
		}}
	]})";
}

struct InnerBodyWorld {
	std::unique_ptr<engine::EngineContext> ctx;

	explicit InnerBodyWorld(const bool blocker_inner_body) {
		ctx = std::make_unique<engine::EngineContext>(std::make_unique<engine::platform::HeadlessPlatform>());
		ctx->GetModuleRegistry().Import(World(), {"physics"});
		engine::level::LoadLevelFromSource(World(), InnerBodyLevel(blocker_inner_body));
	}

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }

	void Step(const int frames) const {
		for (int i = 0; i < frames; ++i) {
			engine::fixed_step::RunFrame(World(), 1.0F / 60.0F);
		}
	}
};

// Walking into another character stops at touching capsules: centers one capsule diameter (1 m) apart.
TEST(CharacterBody, CharactersBlockEachOther) {
	const InnerBodyWorld w(true);
	const auto walker = w.World().lookup("Walker");
	walker.set<CharacterMove>({.velocity = {2.0F, 0.0F, 0.0F}});
	w.Step(120);

	EXPECT_NEAR(walker.get<Transform>().position.x, 2.0F, 0.1F);
}

TEST(CharacterBody, ACharacterWithoutAnInnerBodyIsWalkedThrough) {
	const InnerBodyWorld w(false);
	const auto walker = w.World().lookup("Walker");
	walker.set<CharacterMove>({.velocity = {2.0F, 0.0F, 0.0F}});
	w.Step(120);

	EXPECT_NEAR(walker.get<Transform>().position.x, 4.0F, 0.2F); // two seconds at 2 m/s, nothing in the way
}

} // namespace
