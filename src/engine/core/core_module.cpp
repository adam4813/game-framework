#include "core_module.hpp"

#include <flecs.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include "core_types.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/scripting/scripting.hpp"

namespace engine::core {

CoreModule::CoreModule(const flecs::world& world) {
	// Register std::string as an opaque type with Flecs so that it can be used in components and serialized.
	world.component<std::string>("string")
		.opaque(flecs::String)
		.assign_string([](std::string* data, const char* value) { *data = value; })
		.serialize([](const flecs::serializer* s, const std::string* data) {
			const char* str = data->c_str();
			return s->value(flecs::String, &str);
		});

	world.component<glm::vec2>("vec2").member<float>("x").member<float>("y");
	world.component<glm::vec3>("vec3").member<float>("x").member<float>("y").member<float>("z");
	world.component<glm::vec4>("vec4").member<float>("x").member<float>("y").member<float>("z").member<float>("w");
	world.component<Rgba>()
		.member<std::uint8_t>("r")
		.member<std::uint8_t>("g")
		.member<std::uint8_t>("b")
		.member<std::uint8_t>("a");
	
	scripting::RegisterValueTypeForScripts(world, world.component<glm::vec2>("vec2"));
	scripting::RegisterValueTypeForScripts(world, world.component<glm::vec3>("vec3"));
	scripting::RegisterValueTypeForScripts(world, world.component<glm::vec4>("vec4"));
	scripting::RegisterValueTypeForScripts(world, world.component<Rgba>());

	scripting::RegisterObjectConstructorForScripts(
		world,
		world.component<glm::vec2>("vec2"),
		"float x, float y",
		[](scripting::ScriptCallContext& ctx) {
			new (ctx.GetObject()) glm::vec2(ctx.GetArgFloat(0), ctx.GetArgFloat(1));
		}
	);
	scripting::RegisterObjectConstructorForScripts(
		world,
		world.component<glm::vec3>("vec3"),
		"float x, float y, float z",
		[](scripting::ScriptCallContext& ctx) {
			new (ctx.GetObject()) glm::vec3(ctx.GetArgFloat(0), ctx.GetArgFloat(1), ctx.GetArgFloat(2));
		}
	);
	scripting::RegisterObjectConstructorForScripts(
		world,
		world.component<glm::vec4>("vec4"),
		"float x, float y, float z, float w",
		[](scripting::ScriptCallContext& ctx) {
			new (ctx.GetObject())
				glm::vec4(ctx.GetArgFloat(0), ctx.GetArgFloat(1), ctx.GetArgFloat(2), ctx.GetArgFloat(3));
		}
	);

	scripting::RegisterGlobalConstantsForScripts(
		world,
		{
			{.name = "Vec3_Right",
			 .type = scripting::ScriptValueType::MakeObject(world.component<glm::vec3>()),
			 .value_str = "vec3(1.0, 0.0, 0.0)"},
			{.name = "Vec3_Up",
			 .type = scripting::ScriptValueType::MakeObject(world.component<glm::vec3>()),
			 .value_str = "vec3(0.0, 1.0, 0.0)"},
			{.name = "Vec3_Forward",
			 .type = scripting::ScriptValueType::MakeObject(world.component<glm::vec3>()),
			 .value_str = "vec3(0.0, 0.0, -1.0)"},
		}
	);

	spdlog::info("[CoreModule] Registered core types and constants");
}

} // namespace engine::core
