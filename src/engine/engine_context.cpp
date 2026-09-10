#include "engine/engine_context.hpp"

#include "engine/assets/assets.hpp"
#include "engine/audio/audio.hpp"
#include "engine/core/core.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/ecs/flecs_remote.hpp"
#include "engine/input/input.hpp"
#include "engine/level/level.hpp"
#include "engine/particles/particles.hpp"
#include "engine/platform/platform.hpp"
#include "engine/spatial/spatial.hpp"
#include "engine/tilemap/tilemap.hpp"
#include "physics/physics.hpp"
#include "render/render.hpp"
#include "save/save.hpp"
#include "scene/scene.hpp"
#include "scripting/scripting.hpp"
#include "timer/timer.hpp"
#include "ui/ui.hpp"

#include <glm/glm.hpp>

namespace engine {

EngineContext::EngineContext(std::unique_ptr<platform::Platform> platform) : platform_(std::move(platform)) {
	world_.set<platform::PlatformRef>({platform_.get()});
	world_.set<EngineContextRef>({this});
	world_.set<ecs::RngState>({});

	// Level and scripting modules must be imported first, as other modules may register level loaders or script components.
	world_.import<level::LevelModule>();
	scripting::ScriptingModule::Import<scripting::angelscript::AngelScriptBackend>(world_);
	world_.import<core::CoreModule>();
	world_.import<spatial::SpatialModule>();
	world_.import<scene::SceneManagementModule>();
	world_.import<assets::AssetModule>();
	world_.import<audio::AudioModule>();
	world_.import<input::InputModule>();
	world_.import<physics::PhysicsModule>();
	world_.import<physics::jolt::JoltModule>();
	world_.import<render::RenderModule>();
	world_.import<ui::UIModule>();
	world_.import<save::SaveModule>();
	world_.import<timer::TimerModule>();
	world_.import<tilemap::TilemapModule>();
	world_.import<particles::ParticlesModule>();

#if !defined(__EMSCRIPTEN__)
	// REST API requires TCP sockets — unavailable in WASM browsers.
	ecs::InitializeRemoteAPI(world_);
#endif
}

EngineContext::~EngineContext() = default;

} // namespace engine
