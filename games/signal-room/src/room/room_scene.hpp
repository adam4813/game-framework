#pragma once

#include <expected>
#include <string>

#include <flecs.h>

#include "engine/scene/scene.hpp"

namespace signal_room {

// The Room: the bunker floor of the current Shift. Load reads RoomState::shiftPath, re-reads the
// palette and greybox catalogue, loads that Shift and its Layout under one root entity, Tarps the
// Stations the Shift doesn't put in service and builds the greybox. A load error is recorded in
// RoomState::loadError and leaves the Room empty.
class RoomScene : public engine::scene::SceneBase {
public:
	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void Unload(flecs::world& world) override;

private:
	void LoadRoom(const flecs::world& world) const;
	[[nodiscard]] std::expected<void, std::string>
	BuildRoom(const flecs::world& world, const std::string& shiftPath) const;

	flecs::entity roomRoot_;
};

// Open the Room on a Shift file (asset-relative, or an absolute path): activates the Room scene, or
// reloads it when it is already active. Safe to call from inside a system (deferred).
void StartRoom(const flecs::world& world, const std::string& shiftPath);

// Re-read the current Shift and Layout from disk. No-op unless the Room scene is active.
void ReloadRoom(const flecs::world& world);

} // namespace signal_room
