#include "crabnado_module.hpp"

#include <memory>

#include <spdlog/spdlog.h>

#include "arena_scene.hpp"
#include "claw_snap.hpp"
#include "controls.hpp"
#include "crabnado.hpp"
#include "engine/engine.hpp"
#include "horde.hpp"
#include "player.hpp"
#include "run.hpp"
#include "title_scene.hpp"

using namespace engine;

namespace crabnado {

CrabnadoModule::CrabnadoModule(flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	ui::SetDefaultClickSound(world, platform->AssetDirectory() + "/audio/sfx/click.wav");

	// Systems in one phase run in registration order: input, run clock, player, crabnado, then the horde
	// (whose grid the claw snap reads).
	RegisterControls(world);
	RegisterRun(world);
	RegisterPlayer(world);
	RegisterCrabnado(world);
	RegisterHorde(world);
	RegisterClawSnap(world);

	{
		const auto titleScene = std::make_shared<TitleScene>();
		titleScene->onPlayPressed = [w = &world]() { scene::ActivateScene<ArenaSceneTag>(*w); };
		scene::RegisterScene<TitleSceneTag>(world, titleScene);
	}

	{
		const auto arenaScene = std::make_shared<ArenaScene>();
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
