#pragma once

#include <expected>
#include <memory>
#include <set>
#include <string>
#include <string_view>

#include <flecs.h>

#include "engine/engine_context.hpp"

namespace signal_room::test {

// Fixed frame step used by Step() unless a test passes its own.
inline constexpr float kFixedDelta = 1.0F / 60.0F;

// Headless Room harness: the real EngineContext on a NullPlatform (no window or GPU) with the Signal
// Room module imported. Tests load a Shift, press keys, step the world and query it.
//
// Input: InputPoll is disabled, and the harness writes the keys it holds straight into InputState
// before each frame (with a rising-edge `pressed` on the first frame a key goes down).
class RoomHarness {
public:
	RoomHarness();
	~RoomHarness();

	// Load a Shift file into the Room scene (asset-relative, or an absolute path). Returns the load
	// error when the Shift or its Layout can't be loaded.
	std::expected<void, std::string> LoadShift(std::string_view shiftPath);

	// Re-read the current Shift and Layout from disk, as the Room panel's Reload button does.
	std::expected<void, std::string> Reload();

	void Hold(int key);
	void Release(int key);
	// Hold a key for the next frame only.
	void Tap(int key);

	void Step(int frames = 1, float deltaTime = kFixedDelta);

	[[nodiscard]] flecs::world& World() const;

private:
	[[nodiscard]] std::expected<void, std::string> LoadResult() const;
	void WriteKeys() const;

	std::unique_ptr<engine::EngineContext> ctx_;
	std::set<int> held_;
	std::set<int> tapped_;
	std::set<int> touched_;
};

} // namespace signal_room::test
