#include "room_harness.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

#include "engine/engine.hpp"
#include "null_platform.hpp"
#include "room/room_components.hpp"
#include "room/room_scene.hpp"
#include "signal_room_module.hpp"

namespace signal_room::test {

RoomHarness::RoomHarness() :
		ctx_(std::make_unique<engine::EngineContext>(std::make_unique<NullPlatform>(SIGNAL_ROOM_ASSET_DIR))) {
	auto& world = World();
	world.import<SignalRoomModule>();

	// The harness owns InputState: stop InputPoll overwriting it from the (keyless) platform.
	world.query_builder().with(flecs::System).build().each([](const flecs::entity system) {
		if (const char* name = system.name(); name && std::string_view{name} == "InputPoll") {
			system.disable();
		}
	});
}

RoomHarness::~RoomHarness() = default;

std::expected<void, std::string> RoomHarness::LoadShift(const std::string_view shiftPath) {
	StartRoom(World(), std::string{shiftPath});
	return LoadResult();
}

std::expected<void, std::string> RoomHarness::LoadShiftWithSeed(const std::string_view shiftPath, const uint64_t seed) {
	World().set<SeedPin>({.pinned = true, .value = seed});
	return LoadShift(shiftPath);
}

std::expected<void, std::string> RoomHarness::Reload() {
	ReloadRoom(World());
	return LoadResult();
}

std::expected<void, std::string> RoomHarness::LoadResult() const {
	if (const auto& error = World().get<RoomState>().loadError; !error.empty()) {
		return std::unexpected(error);
	}
	return {};
}

void RoomHarness::Hold(const int key) {
	held_.insert(key);
	touched_.insert(key);
}

void RoomHarness::Release(const int key) { held_.erase(key); }

void RoomHarness::Tap(const int key) {
	Hold(key);
	tapped_.insert(key);
}

void RoomHarness::Step(const int frames, const float deltaTime) {
	for (int i = 0; i < frames; ++i) {
		WriteKeys();
		World().progress(deltaTime);
		ctx_->GetDeferredTaskQueue().ProcessTasks();

		for (const int key : tapped_) {
			held_.erase(key);
		}
		tapped_.clear();
	}
}

void RoomHarness::StepSeconds(const float seconds) { Step(static_cast<int>(std::lround(seconds / kFixedDelta))); }

ShiftWindow RoomHarness::Window() const { return World().get<ShiftWindow>(); }

std::vector<Ticket> RoomHarness::OpenTickets() const {
	std::vector<Ticket> tickets;
	World().each([&](const Ticket& ticket) { tickets.push_back(ticket); });
	std::ranges::sort(tickets, {}, &Ticket::order);
	return tickets;
}

void RoomHarness::WriteKeys() const {
	auto& input = World().get_mut<engine::input::InputState>();
	for (const int key : touched_) {
		auto& keyState = input.keys[static_cast<size_t>(key)];
		const bool down = held_.contains(key);
		keyState.pressed = down && !keyState.state;
		keyState.state = down;
		keyState.current = down;
		keyState.repeat = keyState.pressed;
	}
}

flecs::world& RoomHarness::World() const { return *ctx_->GetWorld(); }

} // namespace signal_room::test
