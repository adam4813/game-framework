#include "football_module.hpp"

#include <string>

#include <spdlog/spdlog.h>

#include "engine/scripting/scripting_module.hpp"
#include "football_components.hpp"

namespace game::football {

FootballModule::FootballModule(const flecs::world& world) {
	// Reflect the run-state singleton so a designer can watch/poke the drive live in the Explorer.
	world.component<FootballState>()
		.member<int>("segment_index")
		.member<int>("down")
		.member<int>("yards")
		.member<int>("target")
		.member<int>("total_points")
		.member<int>("coins")
		.member<int>("segments_converted")
		.member<bool>("last_converted")
		.member<bool>("boss_touchdown")
		.member<bool>("boss_field_goal")
		.member<std::string>("selected_play")
		.member<int>("selected_hand_index");
	engine::scripting::RegisterComponentForScripts(world, world.component<FootballState>());

	// Persisted best score; restored and saved by football_high_score.
	world.component<HighScore>().member<int>("best_points");

	// The remaining football components (Deck, Hand, LastOutcome, ShopState, PlayRegistry) hold
	// dynamic vectors/maps; they are registered as plain components (no member reflection) the first
	// time they are set on the world. Naming the scene identity tag keeps it stable over the Remote
	// API (switch-scene Football).
	world.component<FootballSceneTag>();

	spdlog::info("[FootballModule] Registered football components");
}

} // namespace game::football
