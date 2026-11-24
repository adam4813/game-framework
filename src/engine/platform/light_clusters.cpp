#include "light_clusters.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

#include <glm/gtc/matrix_transform.hpp>

namespace engine::platform {

namespace {

// Keeps a light at the camera from dividing by zero; any light this close is as important as it gets.
constexpr float kMinDistanceSquared = 1e-4f;

struct Candidate {
	float importance;
	uint64_t id;
	uint32_t index; // into the caller's lights
};

// An inclusive run of tiles or slices.
struct CellRange {
	int first;
	int last;
};

// The x, y and slice runs a light's sphere can reach.
struct LightBounds {
	CellRange x;
	CellRange y;
	CellRange slice;
};

// Depths bounding each slice. The ends are the clip depths, so everything drawn falls in a slice.
const std::array<float, kClusterSlices + 1>& SliceDepths() {
	static const auto depths = [] {
		std::array<float, kClusterSlices + 1> d{};
		for (int i = 0; i <= kClusterSlices; ++i) {
			const float t = static_cast<float>(i) / static_cast<float>(kClusterSlices);
			d[static_cast<std::size_t>(i)] = kFirstSliceDepth * std::pow(kLastSliceDepth / kFirstSliceDepth, t);
		}
		d.front() = kClipNear;
		d.back() = kClipFar;
		return d;
	}();
	return depths;
}

int SliceOf(const float depth) {
	if (depth <= kFirstSliceDepth) return 0;
	const auto [scale, bias] = SliceMapping();
	return std::min(static_cast<int>(std::log(depth) * scale + bias), kClusterSlices - 1);
}

// Where tile boundary `i` of `count` crosses the view, as a fraction of the half extent in [-1, 1].
float TileEdge(const int i, const int count) { return 2.0f * static_cast<float>(i) / static_cast<float>(count) - 1.0f; }

// The tiles along one screen axis a sphere can reach: `center` and `depth` are its view-space position on that axis and
// along the camera's look direction, `half_extent` the view's. Nothing when it is off the screen's edge.
std::optional<CellRange> TilesReached(
	const float center,
	const float depth,
	const float radius,
	const float half_extent,
	const bool orthographic,
	const int count
) {
	// Signed distance from the plane through tile boundary `i`, positive on its far side. A perspective camera's
	// boundaries all pass through the eye; an orthographic one's are parallel.
	const auto distance = [&](const int i) {
		const float edge = TileEdge(i, count) * half_extent;
		if (orthographic) return center - edge;
		return (center - edge * depth) / std::sqrt(1.0f + edge * edge);
	};
	CellRange range{.first = count, .last = -1};
	for (int tile = 0; tile < count; ++tile) {
		if (distance(tile) >= -radius && distance(tile + 1) <= radius) {
			range.first = std::min(range.first, tile);
			range.last = tile;
		}
	}
	if (range.last < 0) return std::nullopt;
	return range;
}

// Where a view-space sphere falls in the grid, or nothing when it misses the view.
std::optional<LightBounds> BoundsOf(const glm::vec3 center, const float radius, const ClusterView& view) {
	const float depth = -center.z;
	// Written so that a NaN position is rejected too.
	if (!(depth + radius >= kClipNear && depth - radius <= kClipFar)) return std::nullopt;
	const auto x = TilesReached(center.x, depth, radius, view.half_extent.x, view.orthographic, kClusterTilesX);
	if (!x) return std::nullopt;
	const auto y = TilesReached(center.y, depth, radius, view.half_extent.y, view.orthographic, kClusterTilesY);
	if (!y) return std::nullopt;
	return LightBounds{
		.x = *x,
		.y = *y,
		.slice = {.first = SliceOf(depth - radius), .last = SliceOf(std::min(depth + radius, kClipFar))},
	};
}

// Squared distance from `value` to the interval [lo, hi].
float SquaredGap(const float value, const float lo, const float hi) {
	const float gap = std::max({lo - value, value - hi, 0.0f});
	return gap * gap;
}

// Squared distance from a view-space coordinate to a tile's extent within a slice, as a box around the whole tile.
float SquaredTileGap(
	const float center,
	const int tile,
	const int count,
	const float half_extent,
	const bool orthographic,
	const float near_depth,
	const float far_depth
) {
	const float lo = TileEdge(tile, count) * half_extent;
	const float hi = TileEdge(tile + 1, count) * half_extent;
	if (orthographic) return SquaredGap(center, lo, hi);
	return SquaredGap(center, std::min(lo * near_depth, lo * far_depth), std::max(hi * near_depth, hi * far_depth));
}

// Adds light `index` to every cluster its sphere reaches and that has room.
void AssignLight(
	const uint16_t index,
	const glm::vec3 center,
	const float radius,
	const LightBounds& bounds,
	const ClusterView& view,
	LightClusters& out
) {
	const auto& depths = SliceDepths();
	const float radius_squared = radius * radius;
	for (int slice = bounds.slice.first; slice <= bounds.slice.last; ++slice) {
		const float near_depth = depths[static_cast<std::size_t>(slice)];
		const float far_depth = depths[static_cast<std::size_t>(slice) + 1];
		const float gap_z = SquaredGap(-center.z, near_depth, far_depth);
		for (int y = bounds.y.first; y <= bounds.y.last; ++y) {
			const float gap_yz = gap_z
								 + SquaredTileGap(
									 center.y,
									 y,
									 kClusterTilesY,
									 view.half_extent.y,
									 view.orthographic,
									 near_depth,
									 far_depth
								 );
			if (gap_yz > radius_squared) continue;
			for (int x = bounds.x.first; x <= bounds.x.last; ++x) {
				const float gap_x = SquaredTileGap(
					center.x,
					x,
					kClusterTilesX,
					view.half_extent.x,
					view.orthographic,
					near_depth,
					far_depth
				);
				if (gap_x + gap_yz > radius_squared) continue;

				const auto cluster = static_cast<std::size_t>(x + kClusterTilesX * (y + kClusterTilesY * slice));
				uint32_t& count = out.clusters[cluster].count;
				if (count >= static_cast<uint32_t>(kMaxLightsPerCluster)) {
					++out.cluster_dropped;
					continue;
				}
				out.slots[cluster * kMaxLightsPerCluster + count] = index;
				++count;
			}
		}
	}
}

} // namespace

ClusterView MakeClusterView(const Camera3DParams& camera, const float aspect) {
	const bool orthographic = camera.projection == CameraProjection::Orthographic;
	const float half_height = orthographic ? camera.ortho_size * 0.5f : std::tan(glm::radians(camera.fov_y) * 0.5f);
	return {
		.view = glm::lookAt(camera.position, camera.target, camera.up),
		.position = camera.position,
		.orthographic = orthographic,
		.half_extent = {half_height * aspect, half_height},
	};
}

ClusterSliceMapping SliceMapping() {
	const float scale = static_cast<float>(kClusterSlices) / std::log(kLastSliceDepth / kFirstSliceDepth);
	return {.scale = scale, .bias = -std::log(kFirstSliceDepth) * scale};
}

void BuildLightClusters(const std::span<const PointLightParams> lights, const ClusterView& view, LightClusters& out) {
	out.lights.clear();
	out.indices.clear();
	out.clusters.assign(kClusterCount, ClusterRange{});
	out.dropped = 0;
	out.cluster_dropped = 0;

	std::vector<Candidate> visible;
	for (std::size_t i = 0; i < lights.size(); ++i) {
		const PointLightParams& light = lights[i];
		if (light.intensity == 0.0f || !(light.range > 0.0f)) continue;
		const auto center = glm::vec3(view.view * glm::vec4(light.position, 1.0f));
		if (!BoundsOf(center, light.range, view)) continue;
		const glm::vec3 offset = light.position - view.position;
		const float distance_squared = std::max(glm::dot(offset, offset), kMinDistanceSquared);
		visible.push_back({
			.importance = std::abs(light.intensity) / distance_squared,
			.id = light.id,
			.index = static_cast<uint32_t>(i),
		});
	}

	std::ranges::sort(visible, [](const Candidate& a, const Candidate& b) {
		if (a.importance != b.importance) return a.importance > b.importance;
		return a.id < b.id;
	});
	if (visible.size() > static_cast<std::size_t>(kMaxVisibleLights)) {
		out.dropped = static_cast<int>(visible.size()) - kMaxVisibleLights;
		visible.resize(kMaxVisibleLights);
	}
	if (visible.empty()) return;

	out.slots.resize(static_cast<std::size_t>(kClusterCount) * kMaxLightsPerCluster);
	// In importance order, so a full cluster has already taken its most important lights.
	for (const Candidate& candidate : visible) {
		const PointLightParams& light = lights[candidate.index];
		const auto center = glm::vec3(view.view * glm::vec4(light.position, 1.0f));
		const auto index = static_cast<uint16_t>(out.lights.size());
		out.lights.push_back(light);
		if (const auto bounds = BoundsOf(center, light.range, view)) {
			AssignLight(index, center, light.range, *bounds, view, out);
		}
	}

	for (std::size_t cluster = 0; cluster < out.clusters.size(); ++cluster) {
		ClusterRange& range = out.clusters[cluster];
		range.first = static_cast<uint32_t>(out.indices.size());
		const auto begin = out.slots.begin() + static_cast<std::ptrdiff_t>(cluster * kMaxLightsPerCluster);
		out.indices.insert(out.indices.end(), begin, begin + range.count);
	}
}

} // namespace engine::platform
