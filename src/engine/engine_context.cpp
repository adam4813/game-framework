#include "engine/engine_context.hpp"

#include "engine/assets/assets.hpp"
#include "engine/audio/audio.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/ecs/flecs_remote.hpp"
#include "engine/input/input.hpp"
#include "engine/level/level.hpp"
#include "engine/particles/particles.hpp"
#include "engine/platform/platform.hpp"
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

	// Scripting is imported first so that other modules can expose their own components
	// to scripts from within their constructors.
	scripting::ScriptingModule::Import<scripting::angelscript::AngelScriptBackend>(world_);

	// Register std::string as an opaque type with Flecs so that it can be used in components and
	// serialized. The explicit name "string" matches AngelScript's scriptstdstring registration so
	// ResolveMemberTypeName can bridge the two without special-casing.
	world_.component<std::string>("string")
		.opaque(flecs::String)
		.assign_string([](std::string* data, const char* value) { *data = value; })
		.serialize([](const flecs::serializer* s, const std::string* data) {
			const char* str = data->c_str();
			return s->value(flecs::String, &str); // Forward underlying C-string
		});

	// Register core reflected types shared across modules. These must exist before any
	// module that references them (e.g. physics components use vec3, render primitives use
	// vec2 and Rgba), and must be registered here (not inside an observer) so the structural
	// changes are not deferred.
	world_.component<glm::vec2>("vec2").member<float>("x").member<float>("y");
	world_.component<glm::vec3>("vec3").member<float>("x").member<float>("y").member<float>("z");
	world_.component<glm::vec4>("vec4").member<float>("x").member<float>("y").member<float>("z").member<float>("w");
	world_.component<platform::Rgba>()
		.member<std::uint8_t>("r")
		.member<std::uint8_t>("g")
		.member<std::uint8_t>("b")
		.member<std::uint8_t>("a");
	world_.component<ecs::Transform>()
		.member<glm::vec3>("position")
		.member<glm::vec3>("rotation")
		.member<glm::vec3>("scale");
	world_.component<ecs::WorldTransform>()
		.member<glm::vec3>("position")
		.member<glm::vec3>("rotation")
		.member<glm::vec3>("scale");

	ecs::RegisterTransformPropagation(world_);

	scripting::RegisterValueTypeForScripts(world_, world_.component<glm::vec2>("vec2"));
	scripting::RegisterValueTypeForScripts(world_, world_.component<glm::vec3>("vec3"));
	scripting::RegisterValueTypeForScripts(world_, world_.component<glm::vec4>("vec4"));
	scripting::RegisterValueTypeForScripts(world_, world_.component<platform::Rgba>());
	scripting::RegisterComponentForScripts(world_, world_.component<ecs::Transform>());
	scripting::RegisterComponentForScripts(world_, world_.component<ecs::WorldTransform>());

	// Register multi-argument constructors for math value types so scripts can use initializer
	// syntax: vec2(x, y), vec3(x, y, z). glm types are trivially destructible (POD) so no
	// destructor thunk is needed — pass nullptr.
	scripting::RegisterObjectConstructorForScripts(
		world_,
		world_.component<glm::vec2>("vec2"),
		"float x, float y",
		[](scripting::ScriptCallContext& ctx) {
			new (ctx.GetObject()) glm::vec2(ctx.GetArgFloat(0), ctx.GetArgFloat(1));
		}
	);
	scripting::RegisterObjectConstructorForScripts(
		world_,
		world_.component<glm::vec3>("vec3"),
		"float x, float y, float z",
		[](scripting::ScriptCallContext& ctx) {
			new (ctx.GetObject()) glm::vec3(ctx.GetArgFloat(0), ctx.GetArgFloat(1), ctx.GetArgFloat(2));
		}
	);

	// Register common direction and axis constants as script globals using the vec3 constructor
	// form above. This also demonstrates the ScriptConstant Object kind.
	scripting::RegisterGlobalConstantsForScripts(
		world_,
		{
			{.name = "Vec3_Right",
			 .type = scripting::ScriptValueType::MakeObject(world_.component<glm::vec3>()),
			 .value_str = "vec3(1.0, 0.0, 0.0)"},
			{.name = "Vec3_Up",
			 .type = scripting::ScriptValueType::MakeObject(world_.component<glm::vec3>()),
			 .value_str = "vec3(0.0, 1.0, 0.0)"},
			{.name = "Vec3_Forward",
			 .type = scripting::ScriptValueType::MakeObject(world_.component<glm::vec3>()),
			 .value_str = "vec3(0.0, 0.0, -1.0)"},
		}
	);

	world_.import<scene::SceneManagementModule>();
	world_.import<assets::AssetModule>();
	world_.import<audio::AudioModule>();
	world_.import<input::InputModule>();
	world_.import<physics::PhysicsModule>();
	world_.import<render::RenderModule>();
	world_.import<ui::UIModule>();
	world_.import<save::SaveModule>();
	world_.import<level::LevelModule>();
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
