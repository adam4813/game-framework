#include "jolt_debug_renderer.hpp"

#include <glm/glm.hpp>

#include "engine/ecs/ecs.hpp"
#include "engine/render/render.hpp"
#include "engine/spatial/spatial.hpp"

namespace engine::physics {

#ifdef JPH_DEBUG_RENDERER

JoltDebugRendererAdapter::JoltDebugRendererAdapter(const flecs::world& world, platform::Platform* platform) :
		world_(world), platform_(platform) {
	Initialize();
}

glm::vec3 JoltDebugRendererAdapter::ToGlm(JPH::RVec3Arg v) { return {v.GetX(), v.GetY(), v.GetZ()}; }

glm::vec3 JoltDebugRendererAdapter::ToGlmColor(const JPH::ColorArg c) {
	return {static_cast<float>(c.r) / 255.0F, static_cast<float>(c.g) / 255.0F, static_cast<float>(c.b) / 255.0F};
}

float JoltDebugRendererAdapter::ToGlmAlpha(const JPH::ColorArg c) { return static_cast<float>(c.a) / 255.0F; }

core::Rgba JoltDebugRendererAdapter::ToRgba(const glm::vec3 color, const float alpha) {
	return {
		.r = static_cast<uint8_t>(color.x * 255.0F),
		.g = static_cast<uint8_t>(color.y * 255.0F),
		.b = static_cast<uint8_t>(color.z * 255.0F),
		.a = static_cast<uint8_t>(alpha * 255.0F),
	};
}

glm::vec2 JoltDebugRendererAdapter::ProjectToScreen(const glm::vec3 world_pos) const {
	if (!platform_) return {0.0F, 0.0F};
	const auto camera = world_.query<const spatial::WorldTransform, const render::Camera>().first();
	if (!camera) return {0.0F, 0.0F};
	const auto projector = render::ScreenProjector::FromCamera(
		camera,
		static_cast<float>(platform_->Width()),
		static_cast<float>(platform_->Height())
	);
	return projector.Project(world_pos);
}

void JoltDebugRendererAdapter::DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, const JPH::ColorArg color) {
	if (!platform_) return;

	const glm::vec3 from_glm = ToGlm(from);
	const glm::vec3 to_glm = ToGlm(to);
	const auto rgba = ToRgba(ToGlmColor(color), ToGlmAlpha(color));

	const auto screen_from = ProjectToScreen(from_glm);
	const auto screen_to = ProjectToScreen(to_glm);

	platform_->DrawLine(screen_from, screen_to, 1.0F, rgba);
}

void JoltDebugRendererAdapter::DrawTriangle(
	JPH::RVec3Arg v1,
	JPH::RVec3Arg v2,
	JPH::RVec3Arg v3,
	const JPH::ColorArg color,
	ECastShadow /*cast_shadow*/
) {
	if (!platform_) return;

	// Draw triangle as 3 lines
	DrawLine(v1, v2, color);
	DrawLine(v2, v3, color);
	DrawLine(v3, v1, color);
}

void JoltDebugRendererAdapter::
	DrawText3D(JPH::RVec3Arg position, const std::string_view& text, const JPH::ColorArg color, float /*height*/) {
	if (!platform_) return;

	const glm::vec3 pos_glm = ToGlm(position);
	const auto rgba = ToRgba(ToGlmColor(color), ToGlmAlpha(color));

	const auto screen_pos = ProjectToScreen(pos_glm);
	platform_->DrawText(text, screen_pos.x, screen_pos.y, 16.0F, rgba);
}

#endif // JPH_DEBUG_RENDERER

} // namespace engine::physics
