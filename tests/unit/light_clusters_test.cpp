#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <glm/glm.hpp>

#include "engine/platform/light_clusters.hpp"

using namespace engine::platform;

namespace {

// At the origin looking down -Z with a square view: 90 degrees, so the view is 2 * depth across.
const ClusterView kPerspective = MakeClusterView({.position = {0, 0, 0}, .target = {0, 0, -1}, .fov_y = 90.0F}, 1.0F);

// Same place and direction, 20 units across at every depth.
const ClusterView kOrthographic = MakeClusterView(
	{.position = {0, 0, 0}, .target = {0, 0, -1}, .projection = CameraProjection::Orthographic, .ortho_size = 20.0F},
	1.0F
);

PointLightParams
Light(const glm::vec3 position, const float range, const uint64_t id = 0, const float intensity = 1.0F) {
	return {.id = id, .position = position, .intensity = intensity, .range = range};
}

LightClusters Build(const std::vector<PointLightParams>& lights, const ClusterView& view) {
	LightClusters clusters;
	BuildLightClusters(lights, view, clusters);
	return clusters;
}

// The cluster a fragment at `position` reads, worked out the way lighting.glsl does: from its pixel and view depth.
int ClusterAt(const glm::vec3 position, const ClusterView& view) {
	const float depth = -position.z;
	const float scale = view.orthographic ? 1.0F : depth;
	const float u = position.x / (view.half_extent.x * scale) * 0.5F + 0.5F;
	const float v = position.y / (view.half_extent.y * scale) * 0.5F + 0.5F;
	const auto [slice_scale, slice_bias] = SliceMapping();
	const auto cell = [](const float value, const int count) {
		return std::clamp(static_cast<int>(std::floor(value)), 0, count - 1);
	};
	return cell(u * static_cast<float>(kClusterTilesX), kClusterTilesX)
		   + kClusterTilesX
				 * (cell(v * static_cast<float>(kClusterTilesY), kClusterTilesY)
					+ kClusterTilesY * cell(std::log(depth) * slice_scale + slice_bias, kClusterSlices));
}

// The ids of the lights a fragment at `position` evaluates, in list order.
std::vector<uint64_t> LightsAt(const LightClusters& clusters, const ClusterView& view, const glm::vec3 position) {
	const ClusterRange range = clusters.clusters[static_cast<std::size_t>(ClusterAt(position, view))];
	std::vector<uint64_t> ids;
	for (uint32_t i = 0; i < range.count; ++i) {
		ids.push_back(clusters.lights[clusters.indices[range.first + i]].id);
	}
	return ids;
}

bool IsLit(const LightClusters& clusters, const ClusterView& view, const glm::vec3 position) {
	return !LightsAt(clusters, view, position).empty();
}

int ClustersLit(const LightClusters& clusters) {
	return static_cast<int>(std::ranges::count_if(clusters.clusters, [](const ClusterRange& r) {
		return r.count > 0;
	}));
}

class LightClustersBothViews : public testing::TestWithParam<const ClusterView*> {
protected:
	[[nodiscard]] const ClusterView& View() const { return *GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
	Projections,
	LightClustersBothViews,
	testing::Values(&kPerspective, &kOrthographic),
	[](const auto& info) { return info.param->orthographic ? "Orthographic" : "Perspective"; }
);

} // namespace

// ── Assignment ──────────────────────────────────────────────────────────────────────────────────

TEST_P(LightClustersBothViews, NoLightsLeavesEveryClusterEmpty) {
	const auto clusters = Build({}, View());
	EXPECT_TRUE(clusters.lights.empty());
	EXPECT_EQ(static_cast<int>(clusters.clusters.size()), kClusterCount);
	EXPECT_EQ(ClustersLit(clusters), 0);
}

TEST_P(LightClustersBothViews, ASmallLightInViewReachesOnlyTheClustersAroundIt) {
	const auto clusters = Build({Light({0.3F, 0.2F, -10.0F}, 0.1F, 7)}, View());
	EXPECT_EQ(LightsAt(clusters, View(), {0.3F, 0.2F, -10.0F}), (std::vector<uint64_t>{7}));
	EXPECT_FALSE(IsLit(clusters, View(), {0.3F, 0.2F, -20.0F}));
	EXPECT_FALSE(IsLit(clusters, View(), {6.0F, 0.2F, -10.0F}));
	EXPECT_LE(ClustersLit(clusters), 8);
}

TEST_P(LightClustersBothViews, ALightSpansEveryClusterItsRangeReaches) {
	const auto clusters = Build({Light({0.0F, 0.0F, -10.0F}, 4.0F)}, View());
	for (const glm::vec3 offset :
		 {glm::vec3{3.9F, 0, 0}, {-3.9F, 0, 0}, {0, 3.9F, 0}, {0, -3.9F, 0}, {0, 0, 3.9F}, {0, 0, -3.9F}}) {
		EXPECT_TRUE(IsLit(clusters, View(), glm::vec3{0.0F, 0.0F, -10.0F} + offset));
	}
	EXPECT_FALSE(IsLit(clusters, View(), {0.0F, 8.0F, -10.0F}));
}

// Every sampled point within a light's range must find the light in its cluster, or the light would have a hole.
TEST_P(LightClustersBothViews, NoPointWithinRangeMissesItsLight) {
	const std::vector<glm::vec3> positions{
		{0, 0, -0.2F},
		{0.5F, -0.3F, -1.0F},
		{-4, 3, -6},
		{9.5F, 9.5F, -12},
		{2, -8, -40},
		{-30, 10, -90},
		{0, 0, 1.5F},
		{3, 0, 0.5F},
	};
	for (const glm::vec3 center : positions) {
		constexpr float kRange = 3.0F;
		const auto clusters = Build({Light(center, kRange, 1)}, View());
		constexpr int kSteps = 12;
		for (int ix = -kSteps; ix <= kSteps; ++ix) {
			for (int iy = -kSteps; iy <= kSteps; ++iy) {
				for (int iz = -kSteps; iz <= kSteps; ++iz) {
					const glm::vec3 offset = glm::vec3(ix, iy, iz) * (kRange / static_cast<float>(kSteps));
					const glm::vec3 point = center + offset;
					const float depth = -point.z;
					const glm::vec2 edge = View().half_extent * (View().orthographic ? 1.0F : depth);
					const bool in_view = depth > kClipNear && std::abs(point.x) < edge.x && std::abs(point.y) < edge.y;
					if (!in_view || glm::length(offset) > kRange * 0.99F) continue;
					ASSERT_TRUE(IsLit(clusters, View(), point))
						<< "light at ("
						<< center.x
						<< ", "
						<< center.y
						<< ", "
						<< center.z
						<< "), point at ("
						<< point.x
						<< ", "
						<< point.y
						<< ", "
						<< point.z
						<< ")";
				}
			}
		}
	}
}

TEST_P(LightClustersBothViews, ALightOutsideTheViewIsCulled) {
	const std::vector<glm::vec3>
		outside{{60, 0, -20}, {-60, 0, -20}, {0, 60, -20}, {0, -60, -20}, {0, 0, 20}, {0, 0, -kClipFar - 20}};
	for (const glm::vec3 position : outside) {
		const auto clusters = Build({Light(position, 5.0F)}, View());
		EXPECT_TRUE(clusters.lights.empty()) << position.x << ", " << position.y << ", " << position.z;
		EXPECT_EQ(clusters.dropped, 0);
	}
}

TEST_P(LightClustersBothViews, ALightBehindTheCameraThatReachesPastTheNearPlaneIsKept) {
	const auto clusters = Build({Light({0.0F, 0.0F, 1.0F}, 3.0F)}, View());
	EXPECT_EQ(clusters.lights.size(), 1U);
	EXPECT_TRUE(IsLit(clusters, View(), {0.0F, 0.0F, -0.1F}));
	EXPECT_TRUE(IsLit(clusters, View(), {0.0F, 0.0F, -1.9F}));
	EXPECT_FALSE(IsLit(clusters, View(), {0.0F, 0.0F, -5.0F}));
}

TEST_P(LightClustersBothViews, ALightCenteredOffScreenStillLightsTheEdgeItReaches) {
	// Both views are 20 units across at depth 10, so the screen edge is at x = 10.
	const auto clusters = Build({Light({11.0F, 0.0F, -10.0F}, 2.0F)}, View());
	EXPECT_EQ(clusters.lights.size(), 1U);
	EXPECT_TRUE(IsLit(clusters, View(), {9.9F, 0.0F, -10.0F}));
	EXPECT_FALSE(IsLit(clusters, View(), {0.0F, 0.0F, -10.0F}));
}

TEST_P(LightClustersBothViews, ALightBiggerThanTheViewReachesEveryCluster) {
	const auto clusters = Build({Light({0.0F, 0.0F, -10.0F}, 5000.0F)}, View());
	EXPECT_EQ(ClustersLit(clusters), kClusterCount);
}

TEST_P(LightClustersBothViews, LightsThatGiveNoLightAreLeftOut) {
	constexpr float kNan = std::numeric_limits<float>::quiet_NaN();
	const auto clusters = Build(
		{Light({0, 0, -10}, 0.0F),
		 Light({0, 0, -10}, 5.0F, 1, 0.0F),
		 Light({kNan, 0, -10}, 5.0F),
		 Light({0, 0, kNan}, 5.0F)},
		View()
	);
	EXPECT_TRUE(clusters.lights.empty());
}

// The view follows the camera: the same light is in view for one facing it and culled for one facing away.
TEST(LightClusters, TheViewTurnsWithTheCamera) {
	const std::vector lights{Light({30.0F, 0.0F, 0.0F}, 2.0F)};
	const auto facing = MakeClusterView({.position = {0, 5, 0}, .target = {30, 0, 0}}, 16.0F / 9.0F);
	const auto away = MakeClusterView({.position = {0, 5, 0}, .target = {-30, 0, 0}}, 16.0F / 9.0F);
	EXPECT_EQ(Build(lights, facing).lights.size(), 1U);
	EXPECT_TRUE(Build(lights, away).lights.empty());
}

TEST(LightClusters, ABuildReplacesTheLastOne) {
	LightClusters clusters;
	BuildLightClusters(std::vector{Light({0, 0, -10}, 5.0F)}, kPerspective, clusters);
	BuildLightClusters({}, kPerspective, clusters);
	EXPECT_TRUE(clusters.lights.empty());
	EXPECT_TRUE(clusters.indices.empty());
	EXPECT_EQ(ClustersLit(clusters), 0);
}

// ── Budgets ─────────────────────────────────────────────────────────────────────────────────────

TEST(LightClusters, EveryLightWithinTheVisibleBudgetIsKept) {
	std::vector<PointLightParams> lights;
	for (int i = 0; i < 256; ++i) {
		const int row = i / 16;
		const glm::vec3 position{static_cast<float>(i % 16) - 7.5F, static_cast<float>(row) - 7.5F, -20.0F};
		lights.push_back(Light(position, 1.0F, static_cast<uint64_t>(i)));
	}
	const auto clusters = Build(lights, kPerspective);
	EXPECT_EQ(clusters.lights.size(), 256U);
	EXPECT_EQ(clusters.dropped, 0);
	EXPECT_EQ(clusters.cluster_dropped, 0);
	for (const auto& light : lights) {
		const auto ids = LightsAt(clusters, kPerspective, light.position);
		EXPECT_NE(std::ranges::find(ids, light.id), ids.end()) << "light " << light.id;
	}
}

TEST(LightClusters, OverTheVisibleBudgetTheMostImportantLightsAreKept) {
	// Equal intensity, so nearer is more important: ids 0 to kMaxVisibleLights - 1 are the nearest.
	std::vector<PointLightParams> lights;
	for (int i = 0; i < kMaxVisibleLights + 100; ++i) {
		lights.push_back(Light({0.0F, 0.0F, -5.0F - 0.5F * static_cast<float>(i)}, 0.2F, static_cast<uint64_t>(i)));
	}
	std::ranges::reverse(lights);
	const auto clusters = Build(lights, kPerspective);
	EXPECT_EQ(clusters.dropped, 100);
	ASSERT_EQ(static_cast<int>(clusters.lights.size()), kMaxVisibleLights);
	for (int i = 0; i < kMaxVisibleLights; ++i) {
		EXPECT_EQ(clusters.lights[static_cast<std::size_t>(i)].id, static_cast<uint64_t>(i));
	}
}

TEST(LightClusters, AStrongFarLightOutranksAWeakNearOne) {
	// Importance is intensity over squared distance: 9 / 10^2 beats 2 / 5^2, which beats 1 / 5^2.
	const auto clusters = Build(
		{Light({0, 0, -5}, 1.0F, 1, 1.0F), Light({0, 0, -10}, 1.0F, 2, 9.0F), Light({0, 0, -5}, 1.0F, 3, 2.0F)},
		kPerspective
	);
	ASSERT_EQ(clusters.lights.size(), 3U);
	EXPECT_EQ(clusters.lights[0].id, 2U);
	EXPECT_EQ(clusters.lights[1].id, 3U);
	EXPECT_EQ(clusters.lights[2].id, 1U);
}

TEST(LightClusters, CulledLightsDontUseTheVisibleBudget) {
	std::vector<PointLightParams> lights;
	for (int i = 0; i < kMaxVisibleLights * 4; ++i) {
		lights.push_back(Light({0.0F, 0.0F, 5.0F}, 1.0F, static_cast<uint64_t>(i + 1)));
	}
	lights.push_back(Light({0.0F, 0.0F, -50.0F}, 1.0F, 9999, 0.01F));
	const auto clusters = Build(lights, kPerspective);
	ASSERT_EQ(clusters.lights.size(), 1U);
	EXPECT_EQ(clusters.lights[0].id, 9999U);
	EXPECT_EQ(clusters.dropped, 0);
}

TEST(LightClusters, AFullClusterKeepsItsMostImportantLights) {
	std::vector<PointLightParams> lights;
	for (int i = 0; i < kMaxLightsPerCluster + 10; ++i) {
		// Brightness falls with id, so the first kMaxLightsPerCluster are the ones to keep.
		lights.push_back(Light({0.1F, 0.1F, -10.0F}, 0.05F, static_cast<uint64_t>(i), 100.0F - static_cast<float>(i)));
	}
	std::ranges::reverse(lights);
	const auto clusters = Build(lights, kPerspective);
	EXPECT_EQ(clusters.dropped, 0);
	EXPECT_GT(clusters.cluster_dropped, 0);
	const auto ids = LightsAt(clusters, kPerspective, {0.1F, 0.1F, -10.0F});
	ASSERT_EQ(static_cast<int>(ids.size()), kMaxLightsPerCluster);
	for (int i = 0; i < kMaxLightsPerCluster; ++i) {
		EXPECT_EQ(ids[static_cast<std::size_t>(i)], static_cast<uint64_t>(i));
	}
}

// Equally important lights are kept by id, whatever order the scene lists them in, so a still view never flickers.
TEST(LightClusters, TiesAreKeptByIdWhateverTheOrder) {
	std::vector<PointLightParams> lights;
	for (int i = 0; i < kMaxVisibleLights + 50; ++i) {
		lights.push_back(Light({0.0F, 0.0F, -10.0F}, 1.0F, static_cast<uint64_t>(i)));
	}
	const auto forward = Build(lights, kPerspective);
	std::ranges::reverse(lights);
	const auto backward = Build(lights, kPerspective);
	ASSERT_EQ(forward.lights.size(), backward.lights.size());
	for (std::size_t i = 0; i < forward.lights.size(); ++i) {
		EXPECT_EQ(forward.lights[i].id, i);
		EXPECT_EQ(backward.lights[i].id, i);
	}
	EXPECT_EQ(forward.indices, backward.indices);
}
