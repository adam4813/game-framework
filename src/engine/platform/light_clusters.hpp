#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "engine/platform/platform.hpp"

// Clustered forward shading's CPU half (ADR 0009), shared by every Platform backend: culls the scene's point lights to
// the camera, keeps the most important ones within the frame's budget, and lists the lights that reach each cluster of
// the view frustum. The backend only uploads the result. No GPU calls, so it is unit tested directly.
namespace engine::platform {

// The cluster grid: screen tiles across and up, and depth slices.
constexpr int kClusterTilesX = 16;
constexpr int kClusterTilesY = 9;
constexpr int kClusterSlices = 24;
constexpr int kClusterCount = kClusterTilesX * kClusterTilesY * kClusterSlices;

// The slices grow exponentially with depth between these two, which sets how thin they are where most lights are. The
// first slice also covers everything nearer, and the last everything farther.
constexpr float kFirstSliceDepth = 2.0f;
constexpr float kLastSliceDepth = 250.0f;

// The frame's budgets. Over either, the most important lights are kept (BuildLightClusters).
constexpr int kMaxVisibleLights = 512;
constexpr int kMaxLightsPerCluster = 64;

// The camera as the clusters see it.
struct ClusterView {
	glm::mat4 view{1.0f}; // world to view space; the camera looks along -Z
	glm::vec3 position{0.0f};
	bool orthographic = false;
	// Half the view's width and height: at depth 1 for a perspective camera, at every depth for an orthographic one.
	glm::vec2 half_extent{1.0f};
};

// `aspect` is the canvas width over its height.
[[nodiscard]] ClusterView MakeClusterView(const Camera3DParams& camera, float aspect);

// How a fragment finds its slice: floor(log(view depth) * scale + bias), clamped to the grid.
struct ClusterSliceMapping {
	float scale = 0.0f;
	float bias = 0.0f;
};
[[nodiscard]] ClusterSliceMapping SliceMapping();

// A cluster's entries in LightClusters::indices.
struct ClusterRange {
	uint32_t first = 0;
	uint32_t count = 0;
};

// One frame's clustered lights. Reuse one instance across frames to reuse its buffers.
struct LightClusters {
	std::vector<PointLightParams> lights; // the kept lights, most important first
	std::vector<ClusterRange> clusters;   // kClusterCount, at x + kClusterTilesX * (y + kClusterTilesY * slice)
	std::vector<uint16_t> indices;        // into `lights`, each cluster's most important first

	int dropped = 0;         // lights in view but over kMaxVisibleLights
	int cluster_dropped = 0; // cluster entries over kMaxLightsPerCluster, summed over the clusters

	std::vector<uint16_t> slots; // scratch: kMaxLightsPerCluster per cluster, before compaction into `indices`
};

// Fills `out` from the scene's lights. A light whose `range` sphere misses the view is left out, as is one that gives
// no light (zero intensity or range). Importance is intensity over squared distance to the camera, ties broken by
// `id`, so the same lights and camera always keep the same lights whatever order they come in.
void BuildLightClusters(std::span<const PointLightParams> lights, const ClusterView& view, LightClusters& out);

} // namespace engine::platform
