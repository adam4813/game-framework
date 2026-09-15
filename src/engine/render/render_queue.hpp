#pragma once

#include <array>
#include <vector>

#include <glm/glm.hpp>

#include "engine/platform/platform.hpp"

// Per-frame render command buffer and supporting types. The primitive render systems accumulate
// platform::RenderCommand entries into the RenderQueue singleton; a single flush system hands the
// whole buffer to the platform, which groups and GPU-instances it. This replaces one draw call per
// entity with one draw call per (mesh, shader, texture, flags) group.
namespace engine::render {

// Singleton: the frame's accumulated draw commands, cleared after each flush.
struct RenderQueue {
	std::vector<platform::RenderCommand> commands;
};

// Singleton: render statistics for the on-screen overlay, refreshed each frame.
struct RenderStats {
	int queued{0};      // commands enqueued this frame (after culling)
	int culled{0};      // commands rejected by frustum culling
	int draw_calls{0};  // GL draw calls issued by the batch
	int instances{0};   // instances drawn by the batch
	double enqueue_ms{0.0}; // CPU time building the queue (all primitive enqueue systems)
	double group_ms{0.0};   // CPU time grouping/sorting commands into batches
	double draw_ms{0.0};    // CPU time issuing the instanced draws
};

// Six world-space frustum planes extracted from a view-projection matrix, for sphere culling.
struct Frustum {
	// Each plane is (a, b, c, d) with a normalized normal; a point is inside when
	// dot(normal, p) + d >= 0.
	std::array<glm::vec4, 6> planes{};

	// Extracts the planes from a column-major view-projection matrix (Gribb-Hartmann method).
	void FromViewProj(const glm::mat4& view_proj);
	// True when a world-space bounding sphere is at least partially inside the frustum.
	[[nodiscard]] bool ContainsSphere(glm::vec3 center, float radius) const;
};

// Singleton: the active camera frustum for culling. `valid` is false when no camera exists, in
// which case culling is skipped (everything is enqueued).
struct CameraFrustum {
	Frustum frustum{};
	bool valid{false};
};

} // namespace engine::render
