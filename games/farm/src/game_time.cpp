#include "game_time.hpp"

#include <tuple>
#include <vector>

#include "engine/engine.hpp"
#include "game_constants.hpp"

namespace game {
namespace {

// Internal per-hook tracking kept out of the public GameTime struct.
struct TimeHookRegistry {
	struct HookDef {
		std::string name;
		int hour = 0;
		int lastFiredAbsDay = -1; // absoluteDay when last fired; -1 = never
	};
	std::vector<HookDef> hooks;
};

// Derive the DayPhase for a given hour using the phase boundaries from constants.hpp.
DayPhase PhaseFromHour(const int hour) {
	using namespace game::constats;
	if (hour >= kDawnHourStart && hour <= kDawnHourEnd) return DayPhase::Dawn;
	if (hour >= kDayHourStart && hour <= kDayHourEnd) return DayPhase::Day;
	if (hour >= kDuskHourStart && hour <= kDuskHourEnd) return DayPhase::Dusk;
	return DayPhase::Night;
}

} // namespace

void AddTimeHook(const flecs::world& world, std::string name, int hour) {
	world.get_mut<TimeHookRegistry>().hooks.push_back(TimeHookRegistry::HookDef{.name = std::move(name), .hour = hour});
}

void RegisterGameTime(const flecs::world& world) {
	using namespace game::constats;

	// Enum reflection — constants registered manually, not via meta-reflection.
	world.component<Season>()
		.constant("Spring", Season::Spring)
		.constant("Summer", Season::Summer)
		.constant("Autumn", Season::Autumn)
		.constant("Winter", Season::Winter);

	world.component<DayPhase>()
		.constant("Dawn", DayPhase::Dawn)
		.constant("Day", DayPhase::Day)
		.constant("Dusk", DayPhase::Dusk)
		.constant("Night", DayPhase::Night);

	world.component<GameTime>()
		.member<int>("tick")
		.member<int>("day")
		.member<int>("hour")
		.member<int>("absoluteDay")
		.member<int>("season") // backed by Season enum
		.member<int>("phase"); // backed by DayPhase enum

	world.component<TimeHookFired>().member<int>("hour"); // const char* name skipped — not a reflectable type

	world.component<TimeHookRegistry>();

	// Singletons — defaults from constants; tests may override config fields after calling this.
	world.set<GameTime>({
		.ticksPerHour = kTicksPerHour,
		.daysPerSeason = kDaysPerSeason,
	});
	world.set<TimeHookRegistry>({});

	// TimeAdvanceSystem: increments tick each frame and advances hour/day/season/phase.
	std::ignore = world.system("TimeAdvanceSystem")
					  .kind(flecs::OnUpdate)
					  .run([](flecs::iter& it) {
						  while (it.next()) {
							  auto& gt = it.world().get_mut<GameTime>();

							  ++gt.tick;
							  if (gt.tick % gt.ticksPerHour != 0) continue; // not a complete hour yet

							  ++gt.hour;
							  if (gt.hour >= gt.hoursPerDay) {
								  gt.hour = 0;
								  ++gt.absoluteDay;
								  if (gt.day >= gt.daysPerSeason) {
									  gt.day = 1;
									  gt.season = static_cast<Season>((static_cast<int>(gt.season) + 1) % 4);
								  }
								  else {
									  ++gt.day;
								  }
							  }
							  gt.phase = PhaseFromHour(gt.hour);

							  // Fire any hooks whose hour has been reached for the first time today.
							  auto& registry = it.world().get_mut<TimeHookRegistry>();
							  for (auto& hook : registry.hooks) {
								  if (hook.lastFiredAbsDay != gt.absoluteDay && gt.hour >= hook.hour) {
									  hook.lastFiredAbsDay = gt.absoluteDay;
									  TimeHookFired payload{.name = hook.name.c_str(), .hour = hook.hour};
									  // Use the event builder with .id<GameTime>() so the ids field is
									  // populated; this lets observers with .with<GameTime>() match.
									  it.world()
										  .event<TimeHookFired>()
										  .id<GameTime>()
										  .entity(it.world().entity<GameTime>())
										  .ctx(payload)
										  .emit();
								  }
							  }
						  }
					  })
					  .add<engine::ecs::Pausable>();
}

} // namespace game
