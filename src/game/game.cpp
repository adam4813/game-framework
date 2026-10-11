#include "game.hpp"

#include <memory>
#include <string>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "cube_scene.hpp"
#include "dialog/dialog.hpp"
#include "engine/engine.hpp"
#include "grid_movement.hpp"
#include "meta_save_example.hpp"
#include "noise_scene.hpp"
#include "tilemap_scene.hpp"
#include "title_scene.hpp"

using namespace engine;

namespace game {

GameModule::GameModule(flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const std::string sfxDir = platform->AssetDirectory() + "/audio/sfx";

	// Import the optional engine subsystems this game uses.
	auto& registry = world.get<EngineContextRef>().ptr->GetModuleRegistry();
	registry.Import(world, {"ai", "physics", "noise", "pathfinding", "save", "tilemap", "particles"});

	world.set<GameData>({});

	// Interaction/dialog-tree system (data-driven node graph) usable by any game scene.
	world.import<dialog::DialogModule>();

	// Worked example of the JSON save system: registers the save schema and the script-facing save
	// verbs used by assets/scripts/save_demo.as.
	RegisterMetaSaveExample(world);

	// Register the default button click sound so every CreateButton factory call auto-attaches it.
	ui::SetDefaultClickSound(world, sfxDir + "/click.wav");

	// Grid movement (player_controlled tag + grid_mover loader) is shared by the tilemap demo and
	// the farm scene, so register its loaders once here. The next-farm selection lives in FarmSession.
	RegisterGridMovementLoaders(world);

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
		tilemapScene->RegisterLoaders(world); // register the tilemap scene's level loaders (player, grid_mover, etc.)
		scene::RegisterScene<TilemapSceneTag>(world, tilemapScene);
	}

	{
		const auto noiseScene = std::make_shared<NoiseScene>();
		noiseScene->onBackPressed = [w = &world]() { scene::ActivateScene<TitleSceneTag>(*w); };
		scene::RegisterScene<NoiseSceneTag>(world, noiseScene);
	}

	// Start with the title scene.
	scene::ActivateScene<TitleSceneTag>(world);

	spdlog::info("[GameModule] Registered game data, scenes, and activated the title scene");
}

} // namespace game
