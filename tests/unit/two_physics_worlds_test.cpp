#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <Jolt/Jolt.h>
// clang-format off
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
// clang-format on

#include "engine/engine.hpp"
#include "engine/physics/jolt/jolt_debug_config.hpp"
#include "null_platform.hpp"

#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Renderer/DebugRenderer.h>
#endif

using engine::spatial::WorldTransform;

namespace {

// A crate dropped onto a floor whose top face is at y = 0. Resting, the crate's center is at its half extent, 0.5.
constexpr auto kCrateLevel = R"({"entities": [
	{"name": "Floor", "components": {
		"rigid_body": {"motion_type": "static", "mass": 0.0, "use_gravity": false},
		"collision_shape": {"type": "box", "box_half_extents": [10.0, 0.5, 10.0]},
		"transform": {"position": [0.0, -0.5, 0.0]}
	}},
	{"name": "Crate", "components": {
		"rigid_body": {"motion_type": "dynamic", "mass": 1.0, "restitution": 0.0},
		"collision_shape": {"type": "box", "box_half_extents": [0.5, 0.5, 0.5]},
		"transform": {"position": [0.0, 3.0, 0.0]}
	}}
]})";

constexpr float kRestingCrateY = 0.5F;
constexpr float kRestTolerance = 0.05F;
constexpr int kFramesToSettle = 120;

// A headless Platform that claims a window, so its world installs the debug renderer like a game's would.
class WindowedPlatform final : public NullPlatform {
public:
	[[nodiscard]] bool HasWindow() const override { return true; }
};

// One world with physics and the crate level, on the given Platform.
struct PhysicsWorld {
	std::unique_ptr<engine::EngineContext> ctx;

	explicit PhysicsWorld(std::unique_ptr<engine::platform::Platform> platform = std::make_unique<NullPlatform>()) :
			ctx(std::make_unique<engine::EngineContext>(std::move(platform))) {
		ctx->GetModuleRegistry().Import(World(), {"physics"});
		engine::level::LoadLevelFromSource(World(), kCrateLevel);
	}

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }

	void Step(const int frames) const {
		for (int i = 0; i < frames; ++i) {
			World().progress(1.0F / 60.0F);
		}
	}

	[[nodiscard]] float CrateY() const { return World().lookup("Crate").get<WorldTransform>().position.y; }
};

TEST(TwoPhysicsWorlds, EachWorldLandsItsOwnCrate) {
	const PhysicsWorld a;
	const PhysicsWorld b;
	for (int i = 0; i < kFramesToSettle; ++i) {
		a.Step(1);
		b.Step(1);
	}

	EXPECT_NEAR(a.CrateY(), kRestingCrateY, kRestTolerance);
	EXPECT_NEAR(b.CrateY(), kRestingCrateY, kRestTolerance);
}

// Which of the two worlds is destroyed first.
enum class FirstDestroyed { Older, Newer };

class TwoPhysicsWorldsTeardown : public testing::TestWithParam<FirstDestroyed> {};

TEST_P(TwoPhysicsWorldsTeardown, DestroyingOneWorldLeavesTheOtherAndALaterWorldStepping) {
	auto older = std::make_unique<PhysicsWorld>();
	auto newer = std::make_unique<PhysicsWorld>();
	older->Step(10);
	newer->Step(10);

	const bool olderFirst = GetParam() == FirstDestroyed::Older;
	const auto& survivor = olderFirst ? newer : older;
	(olderFirst ? older : newer).reset();

	// Jolt's process-global Factory and default material, which the survivor's shapes and bodies read, outlive the
	// first teardown.
	EXPECT_NE(JPH::Factory::sInstance, nullptr);
	EXPECT_NE(JPH::PhysicsMaterial::sDefault, nullptr);
	survivor->Step(kFramesToSettle);
	EXPECT_NEAR(survivor->CrateY(), kRestingCrateY, kRestTolerance);

	older.reset();
	newer.reset();
	// Registered once per process: the Factory outlives the last world, so a later world reuses it.
	EXPECT_NE(JPH::Factory::sInstance, nullptr);

	const PhysicsWorld later;
	later.Step(kFramesToSettle);
	EXPECT_NEAR(later.CrateY(), kRestingCrateY, kRestTolerance);
}

INSTANTIATE_TEST_SUITE_P(
	EitherOrder,
	TwoPhysicsWorldsTeardown,
	testing::Values(FirstDestroyed::Older, FirstDestroyed::Newer),
	[](const testing::TestParamInfo<FirstDestroyed>& info) {
		return std::string{info.param == FirstDestroyed::Older ? "OlderFirst" : "NewerFirst"};
	}
);

#ifdef ENGINE_PHYSICS_DEBUG_RENDERER

TEST(TwoPhysicsWorlds, AHeadlessWorldInstallsNoDebugRenderer) {
	const PhysicsWorld headless;
	EXPECT_EQ(JPH::DebugRenderer::sInstance, nullptr);
}

TEST(TwoPhysicsWorlds, AHeadlessWorldLeavesTheRenderingWorldsDebugRendererInstalled) {
	const PhysicsWorld rendering{std::make_unique<WindowedPlatform>()};
	const JPH::DebugRenderer* installed = JPH::DebugRenderer::sInstance;
	ASSERT_NE(installed, nullptr);

	{
		const PhysicsWorld headless;
		headless.Step(1);
	}

	EXPECT_EQ(JPH::DebugRenderer::sInstance, installed);
}

#elif defined(JPH_DEBUG_RENDERER)

// ENABLE_PHYSICS_DEBUG_RENDERER=OFF: Jolt still has its debug renderer, but the engine never installs one.
TEST(TwoPhysicsWorlds, AWindowedWorldInstallsNoDebugRendererWhenTheEngineCompilesItOut) {
	const PhysicsWorld rendering{std::make_unique<WindowedPlatform>()};
	EXPECT_EQ(JPH::DebugRenderer::sInstance, nullptr);
}

#endif

} // namespace
