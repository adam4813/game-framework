#include "translucent_scene.hpp"

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include "engine/engine.hpp"
#include "title_scene.hpp"

using namespace engine;

namespace game {

namespace {

// The occluder wall: an upright quad between the camera and its focus, so the occluder fade has something to fade.
constexpr float kWallHalfWidth = 1.6F;
constexpr float kWallHeight = 3.0F;
constexpr glm::vec3 kWallPosition{0.0F, 0.0F, -1.5F};

} // namespace

void TranslucentScene::InitializePipeline(const flecs::world& world) {
	pipeline_ = ecs::FramePipeline(world).without<scene::MenuScene>().build();
	fixedStepPipeline_ = ecs::FixedStepPipeline(world).without<scene::MenuScene>().build();
}

void TranslucentScene::Load(flecs::world& world) {
	sceneRoot_ = world.entity("TranslucentSceneRoot");
	level::LoadLevel(world, assets::ResolveAsset(world, "levels/translucent_demo.level.json"), sceneRoot_);
	world.set<render::OcclusionFade>({});
	BuildOccluderWall(world);
}

// The wall draws with the fade-aware cutout shader and carries TransparentPass, the occluder-fade ghost. Its vertex
// alpha of 1 marks every vertex fadeable (cutout.fs reads the vertex alpha as that flag).
void TranslucentScene::BuildOccluderWall(const flecs::world& world) const {
	auto* platform = world.get<platform::PlatformRef>().ptr;
	const std::vector<glm::vec3> vertices{
		{-kWallHalfWidth, 0.0F, 0.0F},
		{kWallHalfWidth, 0.0F, 0.0F},
		{kWallHalfWidth, kWallHeight, 0.0F},
		{-kWallHalfWidth, kWallHeight, 0.0F},
	};
	const std::vector<uint32_t> indices{0, 1, 2, 0, 2, 3};
	const std::vector<glm::vec4> colors(vertices.size(), glm::vec4{1.0F});
	const std::vector<glm::vec2> uvs{{0.0F, 2.0F}, {2.0F, 2.0F}, {2.0F, 0.0F}, {0.0F, 0.0F}};
	const int handle = platform->UploadDynamicMesh(vertices, indices, colors, uvs);
	if (handle < 0) {
		spdlog::warn("[TranslucentScene] Occluder wall mesh upload failed; occluder fade demo skipped");
		return;
	}
	std::ignore =
		world.entity()
			.child_of(sceneRoot_)
			.set_name("OccluderWall")
			.set<spatial::Transform>({.position = kWallPosition})
			.set<render::Material>({.color = {.r = 255, .g = 255, .b = 255, .a = 255}, .cast_shadow = false, .double_sided = true})
			.set<render::AlbedoMap>({.path = assets::ResolveAsset(world, "textures/checker.png")})
			.set<render::ShaderMap>(
				{.stages = {{.type = "vertex", .path = "tilemap.vs"}, {.type = "fragment", .path = "cutout.fs"}}}
			)
			.set<render::DynamicMesh>({.handle = handle})
			.add<render::TransparentPass>();
}

void TranslucentScene::HandleInput(const flecs::world& world, const input::InputState& input) {
	if (input.keys[input::KeyCode::Escape].pressed) {
		scene::ActivateScene<TitleSceneTag>(world);
	}
}

void TranslucentScene::Unload(flecs::world& world) {
	// Destroying the root removes the wall's DynamicMesh, whose observer unloads the uploaded mesh.
	if (sceneRoot_) {
		sceneRoot_.destruct();
		sceneRoot_ = flecs::entity{};
	}
	world.remove<render::OcclusionFade>();
	if (world.has<render::AmbientLight>()) {
		world.remove<render::AmbientLight>();
	}
}

} // namespace game
