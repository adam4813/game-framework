#include "game.hpp"

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

#include "engine/engine.hpp"
#include "football/football.hpp"
#include "football_scene.hpp"
#include "title_scene.hpp"

using namespace engine;

namespace game {

GameModule::GameModule(flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const std::string sfxDir = platform->AssetDirectory() + "/audio/sfx";

	world.get<EngineContextRef>().ptr->GetModuleRegistry().Import(world, {"save"});

	// Registers and reflects the football components.
	world.import<football::FootballModule>();
	football::LoadHighScore(world);

	// Register the default button click sound.
	ui::SetDefaultClickSound(world, sfxDir + "/click.wav");

	scene::RegisterScene<TitleSceneTag>(world, std::make_shared<TitleScene>());
	scene::RegisterScene<football::FootballSceneTag>(world, std::make_shared<FootballScene>());
	scene::ActivateScene<TitleSceneTag>(world);

	spdlog::info("[GameModule] Registered the title and football scenes and activated the title scene");
}

} // namespace game
