#pragma once

#include "script_component.hpp"
#include "script_method.hpp"

#include <flecs.h>

#include <string_view>

namespace engine::scripting {

// Per-entity script execution handle. The backend creates one instance per
// (entity, script) pair and owns its lifetime. ScriptComponent stores a raw
// non-owning pointer so systems can dispatch directly — no backend lookup in
// the hot path, just one vtable call.
class IScriptInstance {
public:
	virtual ~IScriptInstance() = default;

	// Called once on the first PreUpdate tick after the entity's ScriptComponent is set.
	virtual void OnInit(flecs::entity entity) = 0;

	// Called each frame for each phase (Pre → On → Post).
	// entity is the script child entity; call entity.parent() for the host.
	virtual void Tick(flecs::entity entity, float dt, ScriptTickPhase phase) = 0;

	// Called when the ScriptComponent is removed or the entity is destroyed.
	virtual void OnDestroy(flecs::entity entity) = 0;
};

// Abstract scripting backend. One instance per process; lives as a Flecs singleton
// (ScriptBackendSingleton). Only involved at load/unload time — tick dispatch goes
// directly through IScriptInstance.
class IScriptBackend {
public:
	virtual ~IScriptBackend() = default;

	// Called once during ScriptingModule construction. Register language-level
	// types, global functions, etc. here.
	virtual bool Init(flecs::world& world) = 0;

	// Called on ScriptingModule destruction. Release all backend resources.
	virtual void Shutdown() = 0;

	// Compile a script from disk and return a backend-owned instance for entity.
	// Returns nullptr on failure.
	virtual IScriptInstance* CreateInstanceFromFile(std::string_view path, flecs::entity entity) = 0;

	// Compile a script from an inline source string and return a backend-owned
	// instance for entity. module_name is used as a unique identifier.
	// Returns nullptr on failure.
	virtual IScriptInstance*
	CreateInstanceFromSource(std::string_view module_name, std::string_view source, flecs::entity entity) = 0;

	// Release a previously created instance. Called by the OnRemove observer.
	virtual void DestroyInstance(IScriptInstance* instance) = 0;

	// Register a Flecs-meta-reflected value type with the backend.
	virtual void RegisterValueType(flecs::entity type_entity) = 0;

	// Register a component type so scripts can read/write it per entity
	// (e.g. as `entity.GetTransform()` / `entity.SetTransform(v)`).
	virtual void RegisterComponentType(flecs::entity component_entity) = 0;

	// Register a singleton component so scripts can read it via a global `T GetT()`
	// that returns the world's singleton value.
	virtual void RegisterSingletonType(flecs::entity component_entity) = 0;

	// Associate a "normal" method with a reflected component value type so scripts can call
	// it on values of that type (e.g. `input.WasKeyPressed(key)`). Flecs meta only models data
	// members, so methods are bound through a generated thunk: `sig` is the neutral signature
	// and `thunk` marshals one call via a ScriptCallContext the backend supplies. The backend
	// wires it into its own generic dispatch (portable to targets without native calling
	// conventions). The component type must already be registered (via RegisterComponentType /
	// RegisterValueType); the backend registers it on demand if not.
	virtual void RegisterComponentMethod(
		flecs::entity component_entity,
		const ScriptMethodSignature& sig,
		ScriptGenericThunk thunk
	) = 0;

	// Register a free / global function visible to every script. `sig` is the neutral signature
	// (same structure as component methods, without is_const); `thunk` is called with the active
	// ScriptCallContext and the world each time the function is invoked from a script.
	virtual void RegisterGlobalFunction(const ScriptMethodSignature& sig, GlobalFunctionThunk thunk) = 0;

	// Register a non-default constructor on an already-registered VALUE type.
	// `params_signature` is the backend-native parameter list, e.g. "float x, float y, float z".
	// `ctor_thunk` — GetObject() points to uninitialized memory; use placement-new to construct.
	// `dtor_thunk` — GetObject() points to the live object; call its destructor. Pass a no-op
	// thunk (or nullptr for POD types where the destructor is trivial and unneeded by AngelScript).
	// Both thunks use the same ScriptGenericThunk type as component methods.
	virtual void RegisterObjectConstructor(
		flecs::entity type_entity,
		std::string_view params_signature,
		ScriptGenericThunk ctor_thunk,
		ScriptGenericThunk dtor_thunk
	) = 0;

	// Register a batch of named constants injected into the global namespace of every compiled
	// script. Each constant is language-agnostic: the backend converts the ScriptValueType +
	// value_str into its own syntax and includes it as a preamble on every subsequent compilation.
	// Only the primitive kinds (Int, Float, Bool, String) and Object (value types only) are supported.
	virtual void RegisterGlobalConstants(std::vector<ScriptConstant> constants) = 0;

	// Register a callback to run just before Shutdown(). Use this to release any
	// ScriptFunctionHandle references held by C++ objects (e.g. TileCallback lambdas) before
	// the backend's script engine is torn down, preventing "external reference" warnings.
	virtual void RegisterShutdownCallback(const std::function<void(flecs::world& world)>& /*callback*/) {}

	// Register a free function that binds a script callback to a C++ sink. Declared to scripts as
	// `void <name>(<fixed_params...>, <callback funcdef>@)`; the trailing argument is a script
	// function handle. When the script calls it, the backend marshals the fixed arguments into a
	// ScriptCallContext and invokes `sink` with a ScriptFunctionHandle wrapping the passed
	// function. This is the primitive behind callback-as-argument bindings (e.g. button onClick):
	// the sink retains and invokes the callback however it likes (typically by wrapping it in a
	// std::function stored on a component).
	virtual void RegisterCallbackFunction(const ScriptCallbackFunctionDesc& /*desc*/, ScriptCallbackSink /*sink*/) {}

	// Register a non-owning reference-view type (script name = type_entity's name) that exposes
	// plain fields of a C++ struct by offset plus callback slots. Instances are handles into
	// C++-owned storage (e.g. handed out by another method). Each callback slot exposes a setter
	// method `view.Set<Name>(@fn)` (the backend wraps the script function in a ScriptFunctionHandle
	// and calls the slot's sink, which typically stores it as a std::function on the object) and,
	// optionally, an invoker `view.<Name>(<params>)` that fires the stored callback directly. This
	// is the primitive behind callback-on-descriptor bindings (e.g. tile OnEnter).
	virtual void RegisterCallbackViewType(
		flecs::entity /*type_entity*/,
		const std::vector<ScriptTypeField>& /*fields*/,
		std::vector<ScriptCallbackProperty> /*callback_props*/
	) {}
};

} // namespace engine::scripting
