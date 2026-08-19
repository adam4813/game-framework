#pragma once

#include "script_backend.hpp"
#include "script_method_factory.hpp"

#include <flecs.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <string>
#include <utility>

namespace engine::scripting {

// Flecs singleton that owns the active scripting backend.
// Must be set on the world BEFORE calling world.import<ScriptingModule>().
// Use ScriptingModule::Import<TBackend>(world) to do both in one call.
struct ScriptBackendSingleton {
	std::unique_ptr<IScriptBackend> backend;
};

// Free helper — expose a Flecs-meta-registered value type to scripts by name.
// Call once per value type after registration.
inline void RegisterValueTypeForScripts(const flecs::world& world, const flecs::entity& type_entity) {
	if (world.has<ScriptBackendSingleton>()) {
		world.get_mut<ScriptBackendSingleton>().backend->RegisterValueType(type_entity);
	}
}

// Free helper — expose a Flecs-meta-registered component to scripts by name.
// Call once per component type after registration.
inline void RegisterComponentForScripts(const flecs::world& world, const flecs::entity& component_entity) {
	if (world.has<ScriptBackendSingleton>()) {
		world.get_mut<ScriptBackendSingleton>().backend->RegisterComponentType(component_entity);
	}
}

// Free helper — expose a singleton component to scripts as a global `T GetT()` accessor.
// Call once per singleton type.
inline void RegisterSingletonForScripts(const flecs::world& world, const flecs::entity& component_entity) {
	if (world.has<ScriptBackendSingleton>()) {
		world.get_mut<ScriptBackendSingleton>().backend->RegisterSingletonType(component_entity);
	}
}

// Free helper — associate a "normal" method with a reflected component value type, deduced
// from a member function pointer passed as a template argument. Bound through a generated thunk
// that marshals via the backend's ScriptCallContext (portable, no native calling convention):
//
//   RegisterComponentMethodForScripts<&InputState::WasKeyPressed>(world, "WasKeyPressed");
//
// The component entity is derived from the method's class, so only the world and script-facing
// name are passed. Call once per method after the component itself has been registered.
template<auto Method>
void RegisterComponentMethodForScripts(const flecs::world& world, std::string name) {
	if (!world.has<ScriptBackendSingleton>()) {
		return;
	}
	using Traits = detail::MemberFnTraits<decltype(Method)>;
	using Class = Traits::Class;

	const ScriptMethodSignature sig = Traits::Signature(world, std::move(name), Traits::kConst);
	const ScriptGenericThunk thunk = &Traits::template Thunk<Method>;
	world.get_mut<ScriptBackendSingleton>().backend->RegisterComponentMethod(world.component<Class>(), sig, thunk);
}

// Free helper — expose a global (free) script function visible to all scripts.
// `sig` describes the function signature; `thunk` is called with the active ScriptCallContext
// and the world each time the function is invoked. Capture any context needed (e.g. a platform
// pointer) in the thunk lambda. Call once per function after the backend is initialised.
//
//   RegisterGlobalFunctionForScripts(world,
//       {.name="PlaySoundHandle", .return_type=ScriptValueType::MakeVoid(),
//        .params={{ScriptValueType::MakeInt(), false, "handle"}}},
//       [platform](ScriptCallContext& ctx, flecs::world&) {
//           const int h = ctx.GetArgInt(0);
//           if (platform && h >= 0) platform->PlaySound(h);
//       });
inline void RegisterGlobalFunctionForScripts(
	const flecs::world& world,
	const ScriptMethodSignature& sig,
	GlobalFunctionThunk thunk
) {
	if (world.has<ScriptBackendSingleton>()) {
		world.get_mut<ScriptBackendSingleton>().backend->RegisterGlobalFunction(sig, std::move(thunk));
	}
}

// Free helper — register a batch of named constants visible as globals in every script.
// Each entry is language-agnostic: backends emit their own syntax from ScriptValueType +
// value_str (e.g. AngelScript: "const int Key_Space = 32;", Lua: "Key_Space = 32").
// Call once during module initialisation, before any scripts are compiled.
//
//   RegisterGlobalConstantsForScripts(world, {
//       {.name="Key_Space", .type=ScriptValueType::MakeInt(), .value_str="32"},
//       {.name="Pi",        .type=ScriptValueType::MakeFloat(), .value_str="3.14159"},
//       {.name="Gravity",   .type=ScriptValueType::MakeObject(world.component<glm::vec3>()),
//                            .value_str="vec3(0.0, -9.81, 0.0)"},
//   });
inline void RegisterGlobalConstantsForScripts(const flecs::world& world, std::vector<ScriptConstant> constants) {
	if (world.has<ScriptBackendSingleton>()) {
		world.get_mut<ScriptBackendSingleton>().backend->RegisterGlobalConstants(std::move(constants));
	}
}

// Free helper — register a non-default constructor on an already-registered value type.
// `params_signature` is the backend-native parameter list (e.g. "float x, float y, float z").
// `ctor_thunk` does placement-new into GetObject(); `dtor_thunk` calls the destructor.
// Pass nullptr for `dtor_thunk` if the type is trivially destructible (POD). Passing a real
// dtor for a non-POD type prevents resource leaks; the backend warns if it is missing.
inline void RegisterObjectConstructorForScripts(
	const flecs::world& world,
	const flecs::entity& type_entity,
	const std::string_view params_signature,
	const ScriptGenericThunk ctor_thunk,
	const ScriptGenericThunk dtor_thunk = nullptr
) {
	if (world.has<ScriptBackendSingleton>()) {
		world.get_mut<ScriptBackendSingleton>()
			.backend->RegisterObjectConstructor(type_entity, params_signature, ctor_thunk, dtor_thunk);
	}
}

// Free helper — register a callback to run just before the backend's Shutdown().
// Use this to nullify any ScriptFunctionHandle-holding std::functions owned by C++ objects
// (e.g. TileCallback lambdas) so the backend's script functions can be released cleanly.
inline void
RegisterScriptShutdownCallback(const flecs::world& world, const std::function<void(flecs::world&)>& callback) {
	if (world.has<ScriptBackendSingleton>()) {
		world.get_mut<ScriptBackendSingleton>().backend->RegisterShutdownCallback(callback);
	}
}

// Free helper — register a callback-binding free function. See
// IScriptBackend::RegisterCallbackFunction for full documentation. The function is declared to
// scripts as `void <name>(<fixed_params...>, <callback funcdef>@)`; `sink` receives the fixed
// arguments (via the ScriptCallContext) plus a ScriptFunctionHandle wrapping the passed script
// function, and decides how to retain and invoke it (typically by wrapping it in a std::function
// stored on a component or descriptor).
inline void RegisterCallbackFunctionForScripts(
	const flecs::world& world,
	const ScriptCallbackFunctionDesc& desc,
	ScriptCallbackSink sink
) {
	if (world.has<ScriptBackendSingleton>()) {
		world.get_mut<ScriptBackendSingleton>().backend->RegisterCallbackFunction(desc, std::move(sink));
	}
}

// Free helper — register a non-owning reference-view type with plain fields and write-only callback
// properties. See IScriptBackend::RegisterCallbackViewType. Instances are handles into C++-owned
// storage (e.g. returned by a component method); `view.Set<Name>(@fn)` calls the property's sink
// with the C++ object and a ScriptFunctionHandle wrapping the script function.
inline void RegisterCallbackViewTypeForScripts(
	const flecs::world& world,
	const flecs::entity& type_entity,
	const std::vector<ScriptTypeField>& fields,
	std::vector<ScriptCallbackProperty> callback_props
) {
	if (world.has<ScriptBackendSingleton>()) {
		world.get_mut<ScriptBackendSingleton>().backend->RegisterCallbackViewType(
			type_entity,
			fields,
			std::move(callback_props)
		);
	}
}

// Free helper — register a component/value-type method backed by a custom thunk (rather than a
// deduced member-function pointer). Use this when the method needs bespoke marshalling, e.g.
// returning a reference-view handle into C++ storage. `sig` is the neutral signature; `thunk`
// marshals one call through a ScriptCallContext the backend supplies.
inline void RegisterComponentMethodForScripts(
	const flecs::world& world,
	const flecs::entity& component_entity,
	const ScriptMethodSignature& sig,
	ScriptGenericThunk thunk
) {
	if (world.has<ScriptBackendSingleton>()) {
		world.get_mut<ScriptBackendSingleton>().backend->RegisterComponentMethod(component_entity, sig, thunk);
	}
}

// ScriptingModule — Flecs module that drives the scripting system.
//
// Preferred usage (backend constructed + imported together):
//   ScriptingModule::Import<AngelScriptBackend>(world);
//
// Manual usage (if you need to configure the backend first):
//   world.set<ScriptBackendSingleton>({ std::make_unique<AngelScriptBackend>() });
//   world.import<ScriptingModule>();
//
// Multiple scripts per entity — attach each as a child with one ScriptComponent:
//   auto s = world.entity("FooScript").child_of(host);
//   s.set<ScriptComponent>({ .source_path = "scripts/foo.as" });
class ScriptingModule {
public:
	explicit ScriptingModule(flecs::world& world);

	// Typed factory: creates the backend, stores it as a singleton, then imports.
	// TBackend must be default-constructible (or use the Args... overload below).
	template<typename TBackend>
	static void Import(flecs::world& world) {
		world.set<ScriptBackendSingleton>({std::make_unique<TBackend>()});
		world.import<ScriptingModule>();
	}

	template<typename TBackend, typename... Args>
	static void Import(flecs::world& world, Args&&... args) {
		world.set<ScriptBackendSingleton>({std::make_unique<TBackend>(std::forward<Args>(args)...)});
		world.import<ScriptingModule>();
	}

private:
	void RegisterObservers(const flecs::world& world);
	void RegisterSystems(const flecs::world& world);
};

} // namespace engine::scripting
