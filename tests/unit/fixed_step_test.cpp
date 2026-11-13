#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "engine/engine.hpp"

using engine::fixed_step::FixedStepConfig;
using engine::fixed_step::FixedStepState;
using engine::fixed_step::RunFrame;
using engine::fixed_step::RunTick;
namespace KeyCode = engine::input::KeyCode;

namespace {

// The default config: 60 Ticks per second, at most 4 per Frame.
constexpr float kTick = 1.0F / 60.0F;

// A headless world with two FixedStep systems, A then B, logging each run, and a per-Frame system counting Frames.
struct FixedStepWorld {
	std::unique_ptr<engine::EngineContext> ctx =
		std::make_unique<engine::EngineContext>(std::make_unique<engine::platform::HeadlessPlatform>());
	flecs::world& world = *ctx->GetWorld();
	std::vector<std::string> log;
	std::vector<float> tick_dts;
	int frames = 0;

	FixedStepWorld() {
		// B is registered first but sits in a later phase, so a Tick orders them by phase: A, then B.
		world.system("B")
			.kind(flecs::PostUpdate)
			.run([this](flecs::iter& it) {
				log.emplace_back("B");
				while (it.next()) {}
			})
			.add<engine::ecs::FixedStep>();
		world.system("A")
			.kind(flecs::OnUpdate)
			.run([this](flecs::iter& it) {
				log.emplace_back("A");
				tick_dts.push_back(it.delta_time());
				while (it.next()) {}
			})
			.add<engine::ecs::FixedStep>();
		world.system("Frame").kind(flecs::OnUpdate).run([this](flecs::iter& it) {
			++frames;
			while (it.next()) {}
		});
	}

	[[nodiscard]] const FixedStepState& State() const { return world.get<FixedStepState>(); }
};

TEST(FixedStep, AFrameWorthTwoAndAHalfTicksRunsTwoAndCarriesTheHalf) {
	FixedStepWorld w;
	RunFrame(w.world, 2.5F * kTick);

	EXPECT_EQ(w.State().tick, 2U);
	EXPECT_NEAR(w.State().alpha, 0.5F, 1e-3F);
	EXPECT_NEAR(w.State().accumulator, 0.5 * kTick, 1e-5);

	// The carried half and another half make one more Tick.
	RunFrame(w.world, 0.5F * kTick);
	EXPECT_EQ(w.State().tick, 3U);
}

TEST(FixedStep, AFrameWorthTenTicksRunsOnlyTheCatchUpCap) {
	FixedStepWorld w;
	RunFrame(w.world, 10.0F * kTick);

	EXPECT_EQ(w.State().tick, 4U);
	EXPECT_LT(w.State().accumulator, kTick);

	// The dropped six don't come back next Frame.
	RunFrame(w.world, kTick);
	EXPECT_EQ(w.State().tick, 5U);
}

TEST(FixedStep, ACatchUpRunsEachSystemOncePerTickInOrder) {
	FixedStepWorld w;
	RunFrame(w.world, 2.0F * kTick);

	EXPECT_EQ(w.log, (std::vector<std::string>{"A", "B", "A", "B"}));
	EXPECT_EQ(w.frames, 1);
}

TEST(FixedStep, FixedStepSystemsGetTheFixedDt) {
	FixedStepWorld w;
	w.world.set<FixedStepConfig>({.tick_rate = 30, .max_catch_up_ticks = 4});
	RunFrame(w.world, 0.1F);

	ASSERT_EQ(w.tick_dts.size(), 3U);
	EXPECT_FLOAT_EQ(w.tick_dts[0], 1.0F / 30.0F);
}

TEST(FixedStep, TheCounterCountsOnlyTicksThatRan) {
	FixedStepWorld w;
	RunFrame(w.world, 0.4F * kTick);
	EXPECT_EQ(w.State().tick, 0U);
	EXPECT_TRUE(w.log.empty());

	RunFrame(w.world, 0.4F * kTick);
	RunFrame(w.world, 0.4F * kTick);
	EXPECT_EQ(w.State().tick, 1U);
	EXPECT_EQ(w.frames, 3);
}

TEST(FixedStep, APausedFrameRunsNoTicksButStillRunsTheFrame) {
	FixedStepWorld w;
	w.world.add<engine::scene::Paused>();
	RunFrame(w.world, 3.0F * kTick);

	EXPECT_EQ(w.State().tick, 0U);
	EXPECT_TRUE(w.log.empty());
	EXPECT_EQ(w.frames, 1);

	// Paused time isn't saved up for when the world resumes.
	w.world.remove<engine::scene::Paused>();
	RunFrame(w.world, 0.5F * kTick);
	EXPECT_EQ(w.State().tick, 0U);
}

TEST(FixedStep, RunTickRunsExactlyOneTickAndLeavesTheAccumulator) {
	FixedStepWorld w;
	RunFrame(w.world, 0.5F * kTick);

	RunTick(w.world);

	EXPECT_EQ(w.State().tick, 1U);
	EXPECT_EQ(w.log, (std::vector<std::string>{"A", "B"}));
	EXPECT_NEAR(w.State().accumulator, 0.5 * kTick, 1e-5);
	EXPECT_EQ(w.frames, 1);
}

TEST(FixedStep, ProgressAloneNeverRunsAFixedStepSystem) {
	FixedStepWorld w;
	w.world.progress(3.0F * kTick);

	EXPECT_TRUE(w.log.empty());
	EXPECT_EQ(w.frames, 1);
}

TEST(FixedStep, EveryTickSeesInputSampledThisFrame) {
	FixedStepWorld w;
	std::vector<bool> pressed;
	w.world.system("Reader")
		.kind(flecs::OnUpdate)
		.run([&pressed](flecs::iter& it) {
			pressed.push_back(it.world().get<engine::input::InputState>().WasKeyPressed(KeyCode::D));
			while (it.next()) {}
		})
		.add<engine::ecs::FixedStep>();

	w.world.set<engine::input::KeyOverride>({.held = "D"});
	RunFrame(w.world, 2.0F * kTick);

	EXPECT_EQ(pressed, (std::vector<bool>{true, true}));
}

// A scene that excludes gameplay systems, as a menu does, from its Frame and fixed-step pipelines.
struct MenuTestScene {};
class MenuLikeScene final : public engine::scene::SceneBase {
public:
	void InitializePipeline(const flecs::world& world) override {
		pipeline_ = engine::ecs::FramePipeline(world).without<engine::scene::GameScene>().build();
		fixedStepPipeline_ = engine::ecs::FixedStepPipeline(world).without<engine::scene::GameScene>().build();
	}
};

TEST(FixedStep, ATickRunsOnlyTheActiveScenesFixedStepSystems) {
	FixedStepWorld w;
	int gameplay_ticks = 0;
	w.world.system("Gameplay")
		.kind(flecs::OnUpdate)
		.run([&gameplay_ticks](flecs::iter& it) {
			++gameplay_ticks;
			while (it.next()) {}
		})
		.add<engine::ecs::FixedStep>()
		.add<engine::scene::GameScene>();
	engine::scene::RegisterScene<MenuTestScene>(w.world, std::make_shared<MenuLikeScene>());
	engine::scene::ActivateScene<MenuTestScene>(w.world);

	RunFrame(w.world, kTick);

	EXPECT_EQ(gameplay_ticks, 0);
	EXPECT_EQ(w.log, (std::vector<std::string>{"A", "B"}));
}

// A crate falling freely from y = 10, with no floor.
constexpr auto kFallingCrateLevel = R"({"entities": [
	{"name": "Crate", "components": {
		"rigid_body": {"motion_type": "dynamic", "mass": 1.0},
		"collision_shape": {"type": "box", "box_half_extents": [0.5, 0.5, 0.5]},
		"transform": {"position": [0.0, 10.0, 0.0]}
	}}
]})";

struct FallingCrateWorld {
	std::unique_ptr<engine::EngineContext> ctx = std::make_unique<engine::EngineContext>();
	flecs::world& world = *ctx->GetWorld();

	FallingCrateWorld() {
		ctx->GetModuleRegistry().Import(world, {"physics"});
		engine::level::LoadLevelFromSource(world, kFallingCrateLevel);
	}

	[[nodiscard]] float CrateY() const {
		return world.lookup("Crate").get<engine::spatial::WorldTransform>().position.y;
	}
};

TEST(FixedStep, PhysicsStepsOncePerTickWhateverTheFrameRate) {
	const FallingCrateWorld at_60;
	const FallingCrateWorld at_30;
	for (int frame = 0; frame < 30; ++frame) {
		RunFrame(at_60.world, kTick);
	}
	for (int frame = 0; frame < 15; ++frame) {
		RunFrame(at_30.world, 2.0F * kTick);
	}

	EXPECT_LT(at_60.CrateY(), 9.0F);
	EXPECT_FLOAT_EQ(at_30.CrateY(), at_60.CrateY());
}

TEST(FixedStep, PhysicsHoldsStillWhilePaused) {
	const FallingCrateWorld w;
	RunFrame(w.world, kTick);
	const float before = w.CrateY();

	w.world.add<engine::scene::Paused>();
	for (int frame = 0; frame < 10; ++frame) {
		RunFrame(w.world, kTick);
	}

	EXPECT_FLOAT_EQ(w.CrateY(), before);
}

} // namespace
