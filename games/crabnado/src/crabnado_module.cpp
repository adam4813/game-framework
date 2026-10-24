#include "crabnado_module.hpp"

#include <memory>

#include <spdlog/spdlog.h>

#include "arena_scene.hpp"
#include "boss.hpp"
#include "bubbles.hpp"
#include "claw_snap.hpp"
#include "controls.hpp"
#include "crabnado.hpp"
#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "horde.hpp"
#include "player.hpp"
#include "riptide.hpp"
#include "run.hpp"
#include "settings.hpp"
#include "sfx.hpp"
#include "shells.hpp"
#include "title_backdrop.hpp"
#include "title_logo.hpp"
#include "title_scene.hpp"
#include "ui_theme.hpp"
#include "upgrades.hpp"
#include "waves.hpp"
#include "wind.hpp"

using namespace engine;

namespace crabnado {

CrabnadoModule::CrabnadoModule(flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	LoadSfx(world);
	RegisterSettings(world);
	ApplyUiTheme(world);

	// Systems in one phase run in registration order: input, run clock, wind, waves, player, crabnado, then the horde
	// (whose grid the claw snap reads), then shells and molting.
	RegisterControls(world);
	RegisterRun(world);
	RegisterWind(world);
	RegisterWaves(world);
	RegisterPlayer(world);
	RegisterCrabnado(world);
	RegisterBoss(world);
	RegisterHorde(world);
	RegisterClawSnap(world);
	RegisterBubbles(world);
	RegisterRiptide(world);
	RegisterShells(world);
	RegisterTitleBackdrop(world);
	RegisterTitleLogo(world);
	RegisterUpgrades(world, platform->AssetDirectory() + std::string{constants::kUpgradesPath});

	{
		const auto titleScene = std::make_shared<TitleScene>();
		titleScene->onIntroFinished = [w = &world]() { scene::ActivateScene<ArenaSceneTag>(*w); };
		scene::RegisterScene<TitleSceneTag>(world, titleScene);
	}

	{
		const auto arenaScene = std::make_shared<ArenaScene>();
		arenaScene->RegisterLoaders(world);
		arenaScene->onTitlePressed = [w = &world]() { scene::ActivateScene<TitleSceneTag>(*w); };
		scene::RegisterScene<ArenaSceneTag>(world, arenaScene);
	}

	// The module constructor runs inside the module's scope, so a Load() triggered here would name its
	// entities under crabnado::CrabnadoModule. Activate at root scope so scene entities get the same
	// paths (e.g. "TitleUI") as when a later button click loads them.
	const flecs::entity moduleScope = world.set_scope(flecs::entity::null());
	scene::ActivateScene<TitleSceneTag>(world);
	world.set_scope(moduleScope);

	spdlog::info("[CrabnadoModule] Registered scenes and activated the title scene");
}

} // namespace crabnado
