#pragma once

#include <glm/glm.hpp>

#include "engine/engine.hpp"

namespace game {

// Identity tag for the tilemap scene (used to identify and switch to this scene)
struct TilemapSceneTag {};

// Tag: marks the player entity so input systems can query it directly.
struct PlayerControlled {};

// Drives smooth per-tile interpolation. Set moving=true with origin/target to start a move.
struct GridMover {
	glm::vec3 origin{};
	glm::vec3 target{};
	float step_size{0.5f}; // world-space size of one tile (geometry, not speed)
	float duration{0.15f}; // seconds to cross one tile; vary per tile type for mud/ice effects
	float elapsed{0.0f};
	bool moving{false};
	float hold_elapsed{0.0f};  // how long the current direction has been held
	float initial_delay{0.1f}; // hold time before repeat kicks in (must be < duration to avoid a gap)
};

// 2D tilemap demo scene with a playable character
class TilemapScene : public engine::scene::SceneBase {
public:
	explicit TilemapScene() = default;
	~TilemapScene() override = default;

	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void Unload(flecs::world& world) override;
	void Tick(const flecs::world& world, float deltaTime) override;
	void HandleInput(const flecs::world& world, const engine::input::InputState& input) override;

private:
	flecs::entity sceneRoot_;
	flecs::entity tilemap_entity_;
	flecs::entity player_entity_;
	flecs::entity uiRoot_;
	flecs::entity pauseModal_;

	void SetupTilemap(const flecs::world& world);
	void SetupPlayer(const flecs::world& world);
	void RegisterInputSystems(const flecs::world& world);
	void BuildUI(const flecs::world& world);
};

} // namespace game
