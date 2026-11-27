#include "farm_game_module.hpp"

#include <memory>
#include <string>


#include "engine/engine.hpp"
#include "farm/farm_save.hpp"
#include "farm/farm_title_scene.hpp"
#include "farm/farm_world_gen.hpp"
#include "farm/farm_world_scene.hpp"
#include "farm/upright_object_mesh_builder.hpp"
#include "farm/villager_system.hpp"
#include "farm/worldgen_preview_scene.hpp"
#include "game_time.hpp"
#include "player_inventory.hpp"

using namespace engine;

namespace farm {

FarmGameModule::FarmGameModule(flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const std::string sfxDir = platform->AssetDirectory() + "/audio/sfx";

	auto& registry = world.get<EngineContextRef>().ptr->GetModuleRegistry();
	registry.Import(world, {"ai", "physics", "noise", "pathfinding", "save", "tilemap", "particles"});

	world.set<FarmGameData>({});

	world.import<dialog::DialogModule>();

	ui::SetDefaultClickSound(world, sfxDir + "/click.wav");

	game::RegisterGameTime(world);
	game::RegisterPlayerInventory(world);
	game::farm::RegisterFarmSave(world);
	game::farm::RegisterFarmWorldComponents(world);

	tilemap::RegisterGridMovementLoaders(world);

	game::farm::RegisterVillagerLoaders(world);

	// Install the upright-object mesh builder (WorldObjectLayer OnSet build/upload observer) and
	// pre-register its components before any system runs.
	game::farm::UprightObjectMeshBuilder::Register(world);

	{
		const auto titleScene = std::make_shared<game::farm::FarmTitleScene>();
		scene::RegisterScene<game::farm::FarmTitleSceneTag>(world, titleScene);
	}
	{
		const auto farmWorldScene = std::make_shared<game::farm::FarmWorldScene>();
		farmWorldScene->onBackPressed = [w = &world]() { scene::ActivateScene<game::farm::FarmTitleSceneTag>(*w); };
		scene::RegisterScene<game::farm::FarmWorldSceneTag>(world, farmWorldScene);
	}
	{
		const auto previewScene = std::make_shared<game::farm::WorldGenPreviewScene>();
		previewScene->onBackPressed = [w = &world]() { scene::ActivateScene<game::farm::FarmTitleSceneTag>(*w); };
		previewScene->onPlayPressed = [w = &world]() { scene::ActivateScene<game::farm::FarmWorldSceneTag>(*w); };
		scene::RegisterScene<game::farm::WorldGenPreviewSceneTag>(world, previewScene);
	}

	scene::ActivateScene<game::farm::FarmTitleSceneTag>(world);

	engine::ecs::Log(world).info("[FarmGameModule] Registered farm game and activated the farm title scene");
}

} // namespace farm
