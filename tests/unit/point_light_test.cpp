#include <gtest/gtest.h>

#include <memory>
#include <span>
#include <string>
#include <vector>

#include "engine/engine.hpp"
#include "engine/platform/light_clusters.hpp"

using engine::platform::LightingStats;
using engine::platform::LightParams;
using engine::platform::PointLightParams;
using engine::render::EffectiveRange;
using engine::render::PointLight;

// ── Range ───────────────────────────────────────────────────────────────────────────────────────

TEST(PointLightRange, AnAuthoredRangeIsUsedAsIs) { EXPECT_FLOAT_EQ(EffectiveRange({.range = 6.5F}), 6.5F); }

TEST(PointLightRange, AnUnsetRangeIsWhereTheLightFallsToOne256th) {
	// 1 / (1 + d^2) * 4 = 1/256 at d^2 = 1023.
	EXPECT_NEAR(
		EffectiveRange({.intensity = 4.0F, .constant = 1.0F, .linear = 0.0F, .quadratic = 1.0F}),
		31.984F,
		0.01F
	);
	// 1 / (1 + d) = 1/256 at d = 255.
	EXPECT_NEAR(EffectiveRange({.constant = 1.0F, .linear = 1.0F, .quadratic = 0.0F}), 255.0F, 0.01F);
}

TEST(PointLightRange, ADefaultLightDerivesARangePastItsVisibleFade) { EXPECT_NEAR(EffectiveRange({}), 87.9F, 0.1F); }

TEST(PointLightRange, ALightThatNeverFadesIsCapped) {
	EXPECT_FLOAT_EQ(EffectiveRange({.constant = 1.0F, .linear = 0.0F, .quadratic = 0.0F}), 1000.0F);
	EXPECT_FLOAT_EQ(EffectiveRange({.constant = 1.0F, .linear = 0.001F, .quadratic = 0.0F}), 1000.0F);
}

TEST(PointLightRange, ALightTooDimToSeeHasNoRange) {
	EXPECT_FLOAT_EQ(EffectiveRange({.intensity = 0.0F}), 0.0F);
	EXPECT_FLOAT_EQ(EffectiveRange({.intensity = 0.001F}), 0.0F);
}

// ── Upload ──────────────────────────────────────────────────────────────────────────────────────

namespace {

// A headless platform that records the point lights it is given and reports the stats a test sets.
class LightPlatform : public engine::platform::HeadlessPlatform {
public:
	std::vector<PointLightParams> lights;
	LightingStats stats;

	LightingStats SetLighting(const LightParams&, const std::span<const PointLightParams> point_lights) override {
		lights.assign(point_lights.begin(), point_lights.end());
		return stats;
	}
};

// A world on a LightPlatform with a camera, its 3D pass opened and lit by hand each frame.
struct LightWorld {
	std::unique_ptr<engine::EngineContext> ctx;
	LightPlatform* platform{nullptr};

	LightWorld() {
		auto owned = std::make_unique<LightPlatform>();
		platform = owned.get();
		ctx = std::make_unique<engine::EngineContext>(std::move(owned));
		World()
			.entity("Camera")
			.set<engine::spatial::Transform>({.position = {0, 0, 10}})
			.set<engine::render::Camera>({});
	}

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }

	void AddLights(const int count) const {
		for (int i = 0; i < count; ++i) {
			World().entity().set<engine::spatial::Transform>({}).set<PointLight>({});
		}
	}

	void RunSystem(const std::string& name) const {
		flecs::entity found;
		World().query_builder<>().with(flecs::System).build().each([&](const flecs::entity e) {
			if (e.name() && name == e.name().c_str()) found = e;
		});
		ASSERT_TRUE(found) << "no system named " << name;
		World().system(found).run();
	}

	void Frame() const {
		RunSystem("Render3DBegin");
		RunSystem("RenderLightingUpload");
		RunSystem("Render3DEnd");
	}

	[[nodiscard]] const engine::render::RenderStats& Stats() const {
		return World().get<engine::render::RenderStats>();
	}
	[[nodiscard]] const engine::render::PointLightOverflowLog& Log() const {
		return World().get<engine::render::PointLightOverflowLog>();
	}
};

} // namespace

TEST(PointLightUpload, EveryLightInTheSceneReachesThePlatform) {
	const LightWorld world;
	world.AddLights(1000);
	world.Frame();
	EXPECT_EQ(world.platform->lights.size(), 1000U);
	EXPECT_EQ(world.Stats().lights, 1000);
}

TEST(PointLightUpload, ALightCarriesItsFieldsItsWorldPositionAndItsRange) {
	const LightWorld world;
	const auto entity = world.World()
							.entity("Lamp")
							.set<engine::spatial::Transform>({.position = {1, 2, 3}})
							.set<PointLight>(
								{.color = {.r = 10, .g = 20, .b = 30, .a = 255},
								 .intensity = 2.0F,
								 .constant = 1.0F,
								 .linear = 0.5F,
								 .quadratic = 0.25F,
								 .range = 8.0F}
							);
	world.Frame();
	ASSERT_EQ(world.platform->lights.size(), 1U);
	const PointLightParams& light = world.platform->lights[0];
	EXPECT_EQ(light.id, entity.id());
	EXPECT_EQ(light.position, glm::vec3(1, 2, 3));
	EXPECT_EQ(light.color.g, 20);
	EXPECT_FLOAT_EQ(light.intensity, 2.0F);
	EXPECT_FLOAT_EQ(light.linear, 0.5F);
	EXPECT_FLOAT_EQ(light.quadratic, 0.25F);
	EXPECT_FLOAT_EQ(light.range, 8.0F);
}

TEST(PointLightUpload, TheFramesLightCountsReachRenderStats) {
	const LightWorld world;
	world.AddLights(3);
	world.platform->stats = {.visible = 2, .dropped = 5, .cluster_dropped = 7, .cluster_ms = 0.5};
	world.Frame();
	EXPECT_EQ(world.Stats().lights_visible, 2);
	EXPECT_EQ(world.Stats().lights_dropped, 5);
	EXPECT_EQ(world.Stats().lights_cluster_dropped, 7);
	EXPECT_DOUBLE_EQ(world.Stats().light_cluster_ms, 0.5);
}

// A budget stays logged while the number of dropped lights changes with the camera, and is armed again only when the
// scene holds too few lights to exceed it.
TEST(PointLightUpload, ABudgetIsLoggedOnceUntilTheSceneFitsItAgain) {
	const LightWorld world;
	world.AddLights(engine::platform::kMaxVisibleLights + 1);
	EXPECT_FALSE(world.Log().warned_visible);

	world.platform->stats = {.visible = engine::platform::kMaxVisibleLights, .dropped = 1};
	world.Frame();
	EXPECT_TRUE(world.Log().warned_visible);
	EXPECT_FALSE(world.Log().warned_cluster);

	world.platform->stats = {.visible = 10, .dropped = 0};
	world.Frame();
	EXPECT_TRUE(world.Log().warned_visible);

	world.World().delete_with<PointLight>();
	world.platform->stats = {};
	world.Frame();
	EXPECT_FALSE(world.Log().warned_visible);
}
