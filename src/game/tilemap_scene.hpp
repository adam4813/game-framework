#pragma once

#include <functional>
#include <string>

#include <glm/glm.hpp>

#include "engine/engine.hpp"
#include "grid_movement.hpp"

namespace game {

struct TilemapSceneTag {};
struct BtWandererNPC {};

/// Tag for the castle-hopping pathfinding demo agent.
/// Tracks which castle center is the current navigation target.
struct CastleHopper {
	int current_index{1}; ///< Index of next target in kCastleCenters
};

/// Cycles a PathAgent through a fixed list of world-space waypoints.
/// When PathAgent.reached_goal becomes true, the system advances to the next waypoint
/// and issues a new PathRequest.
struct WaypointCycler {
	std::vector<glm::vec3> waypoints;
	size_t current_index{0};
};

/// World-space centres of the three map castles (computed from overworld.json stamp positions).
inline constexpr std::array<glm::vec3, 3> kCastleCenters = {
	glm::vec3{12.25f, 0.0f, 4.75f},  // Castle NW  (stamp at tile 19,4  — centre tile 24,9)
	glm::vec3{19.75f, 0.0f, 4.75f},  // Castle NE  (stamp at tile 34,4  — centre tile 39,9)
	glm::vec3{15.75f, 0.0f, 26.75f}, // Castle S   (stamp at tile 26,48 — centre tile 31,53)
};

// Smooth per-tile interpolation.
// (GridMover + PlayerControlled now live in grid_movement.hpp, shared with the farm scene.)

/// Dialog text + two optional script-driven choices. Attach alongside GraphEdge children to
/// visualize the dialog graph in the Flecs Explorer. The choice callbacks are set by scripts
/// via SetDialogChoice(entity, index, label, @fn). Call ShowDialog(entity) to display it.
struct DialogNode {
	std::string text;
	std::string speaker;
	std::string choice_0_label;
	std::string choice_1_label;
	std::function<void(flecs::entity)> on_choice_0;
	std::function<void(flecs::entity)> on_choice_1;
};

/// World singleton set by ShowDialog(entity); removed by CloseDialog().
struct DialogActiveSingleton {
	flecs::entity current_node;
};

class TilemapScene : public engine::scene::SceneBase {
public:
	explicit TilemapScene() = default;
	~TilemapScene() override = default;

	void RegisterLoaders(const flecs::world& world) override;
	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void Unload(flecs::world& world) override;
	void Tick(const flecs::world& world, float deltaTime) override;
	void HandleInput(const flecs::world& world, const engine::input::InputState& input) override;

private:
	flecs::entity sceneRoot_;
	flecs::entity tilemap_entity_;
	flecs::entity player_entity_;
	flecs::entity castle_hopper_entity_;
	flecs::entity uiRoot_;
	flecs::entity pauseModal_;
	flecs::entity dialog_modal_;

	unsigned int spawned_npc_count_{0};
	std::vector<glm::vec3> walkable;

	void SetupTilemap(const flecs::world& world);
	void SetupPlayer(const flecs::world& world) const;
	void SetupNPCs(const flecs::world& world) const;
	void SetupPathfindingAgent();
	void SetupPathfindingStressTest();
	void SetupDialogSystem(const flecs::world& world);
	void BuildDialogUI(const flecs::world& world);
	void RegisterInputSystems(const flecs::world& world) const;
	void BuildUI(const flecs::world& world);
};

} // namespace game
