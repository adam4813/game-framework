#pragma once

#include <string_view>

#include <Jolt/Jolt.h>

#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Core/Color.h>
#include <Jolt/Renderer/DebugRendererSimple.h>
#endif

#include <flecs.h>
#include <glm/glm.hpp>

#include "engine/platform/platform.hpp"

namespace engine::physics {

#ifdef JPH_DEBUG_RENDERER

/// Adapter that inherits Jolt's DebugRendererSimple and forwards draw calls to the platform layer
class JoltDebugRendererAdapter : public JPH::DebugRendererSimple {
public:
	explicit JoltDebugRendererAdapter(const flecs::world& world, platform::Platform* platform);
	~JoltDebugRendererAdapter() override = default;

	void DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, JPH::ColorArg color) override;

	void DrawTriangle(
		JPH::RVec3Arg v1,
		JPH::RVec3Arg v2,
		JPH::RVec3Arg v3,
		JPH::ColorArg color,
		ECastShadow cast_shadow
	) override;

	void DrawText3D(JPH::RVec3Arg position, const std::string_view& text, JPH::ColorArg color, float height) override;

private:
	const flecs::world& world_;
	platform::Platform* platform_;

	static glm::vec3 ToGlm(JPH::RVec3Arg v);
	static glm::vec3 ToGlmColor(JPH::ColorArg c);
	static float ToGlmAlpha(JPH::ColorArg c);
	static platform::Rgba ToRgba(glm::vec3 color, float alpha);

	/// Project 3D world position to 2D screen coordinates using the camera
	[[nodiscard]] glm::vec2 ProjectToScreen(glm::vec3 world_pos) const;
};

#endif // JPH_DEBUG_RENDERER

} // namespace engine::physics
