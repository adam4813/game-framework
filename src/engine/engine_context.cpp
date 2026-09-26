#include "engine/engine_context.hpp"

#include "engine/ai/ai.hpp"
#include "engine/assets/assets.hpp"
#include "engine/audio/audio.hpp"
#include "engine/core/core.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/graph/graph.hpp"
#include "engine/input/input.hpp"
#include "engine/level/level.hpp"
#include "engine/noise/noise.hpp"
#include "engine/particles/particles.hpp"
#include "engine/pathfinding/pathfinding.hpp"
#include "engine/platform/platform.hpp"
#include "engine/profiling/profiling.hpp"
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

	// ── Required modules ─────────────────────────────────────────────────────────────────────
	// These are always imported; they form the foundation all other systems build on.

	// Level and scripting must come first — other modules register level loaders or script components.
	world_.import<level::LevelModule>();
	scripting::ScriptingModule::Import<scripting::angelscript::AngelScriptBackend>(world_);
	world_.import<core::CoreModule>();
	world_.import<spatial::SpatialModule>();
	world_.import<scene::SceneManagementModule>();
	world_.import<assets::AssetModule>();
	world_.import<audio::AudioModule>();
	world_.import<input::InputModule>();
	world_.import<render::RenderModule>();
	world_.import<ui::UIModule>();
	world_.import<timer::TimerModule>();
	world_.import<profiling::ProfilerModule>();

	// The per-system stats panel driver systems are registered once here so they exist in a
	// single deterministic scope regardless of which scene first builds a panel.
	ui::RegisterSystemStatsPanelSystems(world_);

	// ── Optional modules ─────────────────────────────────────────────────────────────────────
	// Register but do not import. Call GetModuleRegistry().Import(world, {...}) in GameModule
	// (or ImportAll to get everything). Entries import their own dependencies first.

	moduleRegistry_.Register("graph", [](flecs::world& w) { w.import<graph::GraphModule>(); });
	moduleRegistry_.Register("ai", [](flecs::world& w) {
		w.import<graph::GraphModule>(); // ai depends on graph
		w.import<ai::AIModule>();
	});
	moduleRegistry_.Register("physics", [](flecs::world& w) {
		w.import<physics::PhysicsModule>();
		w.import<physics::jolt::JoltModule>();
	});
	moduleRegistry_.Register("noise", [](flecs::world& w) {
		w.import<noise::NoiseModule>();
		w.import<noise::builtin::BuiltinNoiseModule>();
	});
	moduleRegistry_.Register("pathfinding", [](flecs::world& w) { w.import<pathfinding::PathfindingModule>(); });
	moduleRegistry_.Register("save", [](flecs::world& w) { w.import<save::SaveModule>(); });
	moduleRegistry_.Register("tilemap", [](flecs::world& w) {
		w.import<pathfinding::PathfindingModule>(); // tilemap nav adapter depends on pathfinding
		w.import<tilemap::TilemapModule>();
	});
	moduleRegistry_.Register("particles", [](flecs::world& w) { w.import<particles::ParticlesModule>(); });

	// Load the per-module profiling filter — runs after platform is ready so AssetDirectory() is valid.
	profiling::LoadModuleFilter(platform_->AssetDirectory() + "/config/profiling.json");
}

EngineContext::~EngineContext() = default;

} // namespace engine
