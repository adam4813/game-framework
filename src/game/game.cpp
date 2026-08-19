#include "game.hpp"

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

#include "cube_scene.hpp"
#include "engine/engine.hpp"
#include "meta_save_example.hpp"
#include "tilemap_scene.hpp"
#include "title_scene.hpp"

using namespace engine;

namespace game {

GameModule::GameModule(flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const std::string sfxDir = platform->AssetDirectory() + "/audio/sfx";

	world.set<GameData>({});

	// Worked example of the JSON save system: registers the save schema and the script-facing save
	// verbs used by assets/scripts/save_demo.as.
	RegisterMetaSaveExample(world);

	// Register the default button click sound so every CreateButton factory call auto-attaches it.
	ui::SetDefaultClickSound(world, sfxDir + "/click.wav");

	{
		const auto titleScene = std::make_shared<TitleScene>();
		titleScene->onPlayPressed = [w = &world]() { scene::ActivateScene<CubeSceneTag>(*w); };
		scene::RegisterScene<TitleSceneTag>(world, titleScene);
	}

	{
		const auto cubeScene = std::make_shared<CubeScene>();
		cubeScene->onQuitPressed = [w = &world]() { scene::ActivateScene<TitleSceneTag>(*w); };
		scene::RegisterScene<CubeSceneTag>(world, cubeScene);
	}

	{
		const auto tilemapScene = std::make_shared<TilemapScene>();
		scene::RegisterScene<TilemapSceneTag>(world, tilemapScene);
	}

	// Start with the title scene.
	scene::ActivateScene<TitleSceneTag>(world);

	spdlog::info("[GameModule] Registered game data, scenes, and activated the title scene");
}

} // namespace game
