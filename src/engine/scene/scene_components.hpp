#pragma once

#include <memory>
#include "scene-base.hpp"

namespace engine::scene {

// Wrapper component that holds a SceneBase instance
// Used to attach the actual SceneBase object to an ECS entity
struct SceneComponent {
	std::shared_ptr<SceneBase> instance;
};

// **Mode/Filter Tags** — Used to filter systems to run only in specific scene categories.
// These are orthogonal to scene identity and can be combined.

// The pipeline built by each scene filters using .without<OtherTag>():
//   GameScene pipeline: .without<MenuScene>()
//   MenuScene pipeline: .without<GameScene>()
//
struct GameScene {}; // Marks systems that should run in gameplay scenes (not menus)
struct MenuScene {}; // Marks systems that should run only in menu/overlay scenes

// Tag component: marks an entity as the active scene.
struct Active {};

// State tag added to the world (world.add<Paused>()) to pause the active scene.
struct Paused {};

} // namespace engine::scene
