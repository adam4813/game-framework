#pragma once

#include <string>

#include <flecs.h>

#include "engine/scene/scene.hpp"

namespace signal_room {

// The Room: the bunker floor of the current Shift. Load reads RoomState::shiftPath, loads that Shift
// and its Layout under one root entity, and records any load error in RoomState::loadError.
class RoomScene : public engine::scene::SceneBase {
public:
	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void Unload(flecs::world& world) override;

private:
	void LoadRoom(const flecs::world& world) const;

	flecs::entity roomRoot_;
};

// Open the Room on a Shift file (asset-relative, or an absolute path): activates the Room scene, or
// reloads it when it is already active. Safe to call from inside a system (deferred).
void StartRoom(const flecs::world& world, const std::string& shiftPath);

// Re-read the current Shift and Layout from disk. No-op unless the Room scene is active.
void ReloadRoom(const flecs::world& world);

} // namespace signal_room
