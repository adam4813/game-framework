#include "football_high_score.hpp"

#include <spdlog/spdlog.h>

#include "engine/platform/platform.hpp"
#include "engine/save/save.hpp"
#include "football_components.hpp"
#include "football_constants.hpp"

namespace game::football {

void LoadHighScore(flecs::world& world) {
	world.set<HighScore>({});
	world.get_mut<engine::save::SaveRegistry>()
		.Singleton<HighScore>("high_score")
		.Member("best_points", &HighScore::best_points);

	const auto* platform = world.get<engine::platform::PlatformRef>().ptr;
	if (!platform->HasSave(constants::kHighScoreSlot)) {
		return;
	}
	engine::save::LoadFromString(world, platform->ReadSave(constants::kHighScoreSlot));
	spdlog::info("[Football] High score {}", world.get<HighScore>().best_points);
}

bool RecordScore(const flecs::world& world, const int points) {
	auto& best = world.get_mut<HighScore>();
	if (points <= best.best_points) {
		return false;
	}
	best.best_points = points;
	world.get<engine::platform::PlatformRef>().ptr->WriteSave(
		constants::kHighScoreSlot,
		engine::save::SaveToString(world)
	);
	return true;
}

} // namespace game::football
