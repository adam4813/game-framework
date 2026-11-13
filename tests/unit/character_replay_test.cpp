#include <memory>

#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include "engine/engine.hpp"
#include "engine/physics/jolt/jolt_backend.hpp"

using engine::physics::CharacterMove;
using engine::physics::CharacterState;
using engine::physics::JoltBackend;
using engine::spatial::Transform;

namespace {

constexpr float kTickDt = 1.0F / 60.0F;
constexpr float kGravity = -9.81F;

// A floor whose top face is at y = 0, a walker that lands on it, and two bodies falling beside it: a second character
// and a dynamic crate. The capsule's origin is at its feet.
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
	}},
	{"name": "Other", "components": {
		"character_body": {},
		"collision_shape": {"type": "capsule", "capsule_radius": 0.5, "capsule_height": 1.0, "offset": [0.0, 1.0, 0.0]},
		"transform": {"position": [5.0, 30.0, 0.0]}
	}},
	{"name": "Crate", "components": {
		"rigid_body": {"motion_type": "dynamic", "mass": 1.0},
		"collision_shape": {"type": "box", "box_half_extents": [0.5, 0.5, 0.5]},
		"transform": {"position": [-5.0, 30.0, 0.0]}
	}}
]})";

struct ReplayWorld {
	std::unique_ptr<engine::EngineContext> ctx;

	ReplayWorld() {
		ctx = std::make_unique<engine::EngineContext>(std::make_unique<engine::platform::HeadlessPlatform>());
		ctx->GetModuleRegistry().Import(World(), {"physics"});
		engine::level::LoadLevelFromSource(World(), kLevel);
		// Two seconds: the walker lands; Other and the crate are still falling.
		for (int i = 0; i < 120; ++i) {
			engine::fixed_step::RunFrame(World(), kTickDt);
		}
	}

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }
	[[nodiscard]] flecs::entity Walker() const { return World().lookup("Walker"); }
	[[nodiscard]] JoltBackend& Backend() const { return World().get_mut<JoltBackend>(); }
};

// The prediction flow: put the character back where the server had it, airborne and falling, then step it once.
TEST(CharacterReplay, RestoredPositionAndVelocityDriveTheNextStep) {
	const ReplayWorld w;
	const auto walker = w.Walker();
	ASSERT_TRUE(walker.get<CharacterState>().grounded);

	w.Backend().SetCharacterPosition(walker.id(), {0.0F, 2.0F, 0.0F});
	w.Backend().SetCharacterVelocity(walker.id(), {0.0F, -2.0F, 0.0F});
	w.Backend().RefreshCharacterContacts(walker.id());
	EXPECT_FALSE(w.Backend().GetCharacterSyncResult(walker.id()).grounded);

	engine::physics::StepCharacterTick(walker);

	// Falling at 2 m/s plus one Tick of gravity: y = 2 + (-2 + g * dt) * dt.
	const float expected_y = 2.0F + (-2.0F + kGravity * kTickDt) * kTickDt;
	EXPECT_NEAR(walker.get<Transform>().position.y, expected_y, 1e-3F);
	EXPECT_FALSE(walker.get<CharacterState>().grounded);
	EXPECT_NEAR(walker.get<CharacterState>().velocity.y, -2.0F + kGravity * kTickDt, 1e-3F);
}

TEST(CharacterReplay, SteppingOneCharacterLeavesTheRestOfTheWorldAlone) {
	const ReplayWorld w;
	const auto walker = w.Walker();
	const auto other = w.World().lookup("Other");
	const auto crate = w.World().lookup("Crate");
	const glm::vec3 other_before = other.get<Transform>().position;
	const glm::vec3 crate_before = w.Backend().GetPhysicsSyncResult(crate.id()).position;
	const uint64_t tick_before = w.World().get<engine::fixed_step::FixedStepState>().tick;

	walker.set<CharacterMove>({.velocity = {3.0F, 0.0F, 0.0F}});
	engine::physics::StepCharacterTick(walker);

	EXPECT_NEAR(walker.get<Transform>().position.x, 3.0F * kTickDt, 1e-3F);
	EXPECT_EQ(other.get<Transform>().position, other_before);
	EXPECT_EQ(w.Backend().GetCharacterSyncResult(other.id()).position, other_before);
	EXPECT_EQ(w.Backend().GetPhysicsSyncResult(crate.id()).position, crate_before);
	EXPECT_EQ(w.World().get<engine::fixed_step::FixedStepState>().tick, tick_before);
}

// Replaying a character must land where the Tick put it, or a prediction drifts from what the fixed step would do.
TEST(CharacterReplay, AStepLandsWhereATickWould) {
	const ReplayWorld ticked;
	const ReplayWorld replayed;
	for (const ReplayWorld* w : {&ticked, &replayed}) {
		w->Walker().set<CharacterMove>({.velocity = {2.0F, 0.0F, 1.0F}});
	}

	for (int i = 0; i < 30; ++i) {
		engine::fixed_step::RunTick(ticked.World());
		engine::physics::StepCharacterTick(replayed.Walker());
	}

	const glm::vec3 ticked_position = ticked.Walker().get<Transform>().position;
	const glm::vec3 replayed_position = replayed.Walker().get<Transform>().position;
	EXPECT_NEAR(ticked_position.x, 1.0F, 0.05F); // half a second at 2 m/s
	EXPECT_NEAR(replayed_position.x, ticked_position.x, 1e-5F);
	EXPECT_NEAR(replayed_position.y, ticked_position.y, 1e-5F);
	EXPECT_NEAR(replayed_position.z, ticked_position.z, 1e-5F);
	EXPECT_EQ(replayed.Walker().get<CharacterState>().grounded, ticked.Walker().get<CharacterState>().grounded);
}

} // namespace
