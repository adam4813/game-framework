#include <memory>

#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include "engine/engine.hpp"
#include "null_platform.hpp"

using engine::physics::CharacterMove;
using engine::physics::CharacterState;
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
		ctx = std::make_unique<engine::EngineContext>(std::make_unique<NullPlatform>());
		ctx->GetModuleRegistry().Import(World(), {"physics"});
		engine::level::LoadLevelFromSource(World(), kLevel);
	}

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }

	void Step(const int frames) const {
		for (int i = 0; i < frames; ++i) {
			World().progress(1.0F / 60.0F);
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

} // namespace
