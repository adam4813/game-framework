#pragma once

#include <flecs.h>

#include "engine/engine.hpp"

namespace game {

// Identity tag for the translucent-pass demo scene.
struct TranslucentSceneTag {};

// Demo of the render translucent pass: two overlapping translucent robots (one skinned and animated) in front of an
// opaque one, a translucent cube and sphere in front of an opaque capsule, and an occluder-fade wall between the
// camera and its LookAt focus. Authored in assets/levels/translucent_demo.level.json; the wall is a runtime-uploaded
// DynamicMesh, so it is built here. Escape returns to the title.
class TranslucentScene : public engine::scene::SceneBase {
public:
	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void HandleInput(const flecs::world& world, const engine::input::InputState& input) override;
	void Unload(flecs::world& world) override;

private:
	void BuildOccluderWall(const flecs::world& world) const;

	flecs::entity sceneRoot_;
};

} // namespace game
