#include "game.hpp"

#include <memory>
#include <string>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "cube_scene.hpp"
#include "engine/engine.hpp"
#include "meta_save_example.hpp"
#include "noise_scene.hpp"
#include "synth_scene.hpp"
#include "tilemap_scene.hpp"
#include "title_scene.hpp"

using namespace engine;

namespace game {

GameModule::GameModule(flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const std::string sfxDir = platform->AssetDirectory() + "/audio/sfx";

	// Import engine subsystems used by sandbox scenes.
	auto& registry = world.get<EngineContextRef>().ptr->GetModuleRegistry();
	registry.Import(world, {"ai", "physics", "noise", "pathfinding", "save", "tilemap", "particles", "dialog"});

	world.set<GameData>({});

	// Worked example of the JSON save system: registers the save schema and the script-facing save
	// verbs used by assets/scripts/save_demo.as.
	RegisterMetaSaveExample(world);

	// Register the default button click sound.
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
		tilemapScene->RegisterLoaders(world);
		scene::RegisterScene<TilemapSceneTag>(world, tilemapScene);
	}

	{
		const auto noiseScene = std::make_shared<NoiseScene>();
		noiseScene->onBackPressed = [w = &world]() { scene::ActivateScene<TitleSceneTag>(*w); };
		scene::RegisterScene<NoiseSceneTag>(world, noiseScene);
	}

	{
		const auto synthScene = std::make_shared<SynthScene>();
		synthScene->onBackPressed = [w = &world]() { scene::ActivateScene<TitleSceneTag>(*w); };
		scene::RegisterScene<SynthSceneTag>(world, synthScene);
	}

	scene::ActivateScene<TitleSceneTag>(world);

	spdlog::info("[GameModule] Registered sandbox scenes and activated the title scene");
}

} // namespace game
