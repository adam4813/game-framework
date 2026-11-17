#pragma once

#include <mutex>
#include <span>
#include <string_view>
#include <vector>

#include <Jolt/Jolt.h>

#include "jolt_debug_config.hpp"

#ifdef ENGINE_PHYSICS_DEBUG_RENDERER
#include <Jolt/Core/Color.h>
#include <Jolt/Renderer/DebugRendererSimple.h>
#endif

#include <glm/glm.hpp>

#include "engine/physics/physics_types.hpp"
#include "engine/render/render.hpp"

namespace engine::physics {

#ifdef ENGINE_PHYSICS_DEBUG_RENDERER

/// Jolt's debug renderer, collecting what it's asked to draw as depth-tested 3D lines for the render module's
/// LineQueue: body wireframes from Jolt, and the raycasts made while it's enabled. Nothing is drawn directly; the
/// PhysicsDebugRender system moves the lines onto the LineQueue each frame (Flush).
class JoltDebugRendererAdapter : public JPH::DebugRendererSimple {
public:
	JoltDebugRendererAdapter();
	~JoltDebugRendererAdapter() override = default;

	void DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, JPH::ColorArg color) override;

	void DrawTriangle(
		JPH::RVec3Arg v1,
		JPH::RVec3Arg v2,
		JPH::RVec3Arg v3,
		JPH::ColorArg color,
		ECastShadow cast_shadow
	) override;

	/// Ignored: the body draw settings the engine uses emit no text, and a depth-tested line pass has no text.
	void DrawText3D(JPH::RVec3Arg position, const std::string_view& text, JPH::ColorArg color, float height) override;

	/// Off by default. Turning it off drops every collected line and ray.
	void SetEnabled(bool enabled);
	[[nodiscard]] bool Enabled() const;

	/// Records a raycast from `origin` to `end` (its full length) and its hits, nearest first and none for a miss.
	/// Ignored while disabled. Safe to call from any thread.
	void RecordRay(glm::vec3 origin, glm::vec3 end, std::span<const RaycastResult> hits);

	/// Appends this frame's lines to `out`: the body lines drawn since the last Flush, then the latest raycasts.
	/// Body lines are cleared. Raycasts stay until a later frame records any, so a one-off cast stays visible.
	void Flush(std::vector<render::Line3D>& out);

private:
	mutable std::mutex mutex_;
	bool enabled_{false};
	std::vector<render::Line3D> body_lines_;
	std::vector<render::Line3D> pending_rays_; // recorded since the last Flush
	std::vector<render::Line3D> shown_rays_;   // drawn every Flush until pending_rays_ has any
};

#endif // ENGINE_PHYSICS_DEBUG_RENDERER

} // namespace engine::physics
