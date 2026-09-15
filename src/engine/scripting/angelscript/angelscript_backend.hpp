#pragma once

#include "engine/scripting/script_backend.hpp"
#include "engine/scripting/script_context.hpp"

#include <angelscript.h>
#include <flecs.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace engine::scripting::angelscript {

// Forward declaration: defined in the .cpp anonymous namespace, used by AngelScriptInstance for
// handle invalidation. Full definition is not needed in callers' translation units.
class AngelScriptFunctionHandle;

// AngelScript implementation of IScriptInstance.
// One instance exists per (entity, ScriptComponent) pair.
// Caches resolved function pointers at construction to avoid repeated lookups.
class AngelScriptInstance final : public IScriptInstance {
public:
	AngelScriptInstance(asIScriptEngine* engine, asIScriptModule* module, flecs::entity entity);
	~AngelScriptInstance() override;

	void OnInit(flecs::entity entity) override;
	void Tick(flecs::entity entity, float dt, ScriptTickPhase phase) override;
	void OnDestroy(flecs::entity entity) override;

	// Track a handle created from a script function in this instance's module.
	// On instance destruction, all tracked handles have their AS function reference released
	// (fn_ nulled) before the module is discarded — eliminating "external reference" warnings.
	void TrackHandle(AngelScriptFunctionHandle* handle);
	// Called by the handle's own destructor when the handle dies before the instance.
	void UntrackHandle(AngelScriptFunctionHandle* handle);

private:
	void Call(asIScriptFunction* fn, flecs::entity entity, float dt = 0.0f, bool pass_dt = false);

	asIScriptEngine* engine_; // non-owning (owned by backend)
	asIScriptModule* module_; // non-owning (owned by engine)
	asIScriptContext* ctx_;   // owned — one context per instance

	ScriptEntityRef entity_ref_; // "self" passed into every script call

	// Cached entry points — null if not defined in the script (all optional).
	asIScriptFunction* fn_on_init_{nullptr};
	asIScriptFunction* fn_pre_tick_{nullptr};
	asIScriptFunction* fn_on_tick_{nullptr};
	asIScriptFunction* fn_post_tick_{nullptr};
	asIScriptFunction* fn_on_destroy_{nullptr};

	// All live handles created from this instance's module functions. Invalidated before discard.
	std::vector<AngelScriptFunctionHandle*> tracked_handles_;
};

// AngelScript implementation of IScriptBackend.
// Owns the asIScriptEngine and all compiled modules.
class AngelScriptBackend final : public IScriptBackend {
public:
	AngelScriptBackend() = default;
	~AngelScriptBackend() override;

	// Returns the raw asIScriptEngine pointer for AngelScript-specific registrations
	// (funcdef declarations, non-trivial value types, etc.) that have no language-agnostic
	// equivalent in the neutral IScriptBackend API. Null before Init() is called.
	[[nodiscard]] asIScriptEngine* GetEngine() const { return engine_; }

	bool Init(flecs::world& world) override;
	void Shutdown() override;

	IScriptInstance* CreateInstanceFromFile(std::string_view path, flecs::entity entity) override;
	IScriptInstance*
	CreateInstanceFromSource(std::string_view module_name, std::string_view source, flecs::entity entity) override;

	void DestroyInstance(IScriptInstance* instance) override;
	void RegisterValueType(flecs::entity type_entity) override;
	void RegisterComponentType(flecs::entity component_entity) override;
	void RegisterSingletonType(flecs::entity component_entity) override;
	void RegisterComponentMethod(
		flecs::entity component_entity,
		const ScriptMethodSignature& sig,
		ScriptGenericThunk thunk
	) override;
	void RegisterGlobalFunction(const ScriptMethodSignature& sig, GlobalFunctionThunk thunk) override;
	void RegisterObjectConstructor(
		flecs::entity type_entity,
		std::string_view params_signature,
		ScriptGenericThunk ctor_thunk,
		ScriptGenericThunk dtor_thunk
	) override;
	void RegisterGlobalConstants(std::vector<ScriptConstant> constants) override;
	void RegisterCallbackFunction(const ScriptCallbackFunctionDesc& desc, ScriptCallbackSink sink) override;
	void RegisterCallbackViewType(
		flecs::entity type_entity,
		const std::vector<ScriptTypeField>& fields,
		std::vector<ScriptCallbackProperty> callback_props
	) override;

private:
	void RegisterEntityType();
	void RegisterGlobalFunctions() const;
	void RegisterValueTypeFromMeta(flecs::entity type_entity) const;
	static void ComponentGetRefGeneric(asIScriptGeneric* gen);
	static void ComponentGetMutRefGeneric(asIScriptGeneric* gen);
	static void ComponentGetConstRefGeneric(asIScriptGeneric* gen);
	static void ComponentAddGeneric(asIScriptGeneric* gen);
	static void ComponentSetGeneric(asIScriptGeneric* gen);
	static void ComponentHasGeneric(asIScriptGeneric* gen);
	static void SingletonGetGeneric(asIScriptGeneric* gen);
	static void ComponentMethodGeneric(asIScriptGeneric* gen);
	static void GlobalFunctionGeneric(asIScriptGeneric* gen);
	static void CallbackFunctionGeneric(asIScriptGeneric* gen);
	static void CallbackPropertySetGeneric(asIScriptGeneric* gen);
	static void CallbackPropertyInvokeGeneric(asIScriptGeneric* gen);
	[[nodiscard]] asIScriptModule* CompileSource(std::string_view module_name, std::string_view source) const;

	// Stable per-registration state for a callback-binding function (see RegisterCallbackFunction).
	// Heap-allocated so its address stays valid as the generic auxiliary; carries the C++ sink,
	// the callback's parameter list (for handle marshalling), and the funcdef argument's index.
	struct CallbackBinding {
		ScriptCallbackSink sink;
		std::vector<ScriptMethodParam> callback_params;
		int callback_arg_index{0};
	};

	// Stable per-property state for a callback view slot (see RegisterCallbackViewType).
	struct CallbackViewProperty {
		std::function<void(void* object, ScriptFunctionHandle* handle)> sink;
		std::function<void(void* object, ScriptCallContext& ctx)> invoke;
		std::vector<ScriptMethodParam> params;
		asIScriptEngine* engine{nullptr};
	};

	asIScriptEngine* engine_{nullptr};
	flecs::world* world_{nullptr};
	std::unordered_set<AngelScriptInstance*> instances_; // for bulk cleanup on Shutdown
	// Maps AS declaration → owned thunk. The heap object's address is stable (unique_ptr never
	// moves the managed allocation), so it is safe as the AngelScript generic auxiliary.
	// On re-registration the thunk is updated in-place; the AS engine's auxiliary is unchanged.
	std::unordered_map<std::string, std::unique_ptr<GlobalFunctionThunk>> global_thunks_;
	// Maps AS declaration → owned binding (same pointer-stability guarantee as global_thunks_).
	std::unordered_map<std::string, std::unique_ptr<CallbackBinding>> callback_bindings_;
	// Maps "TypeName::PropName" → owned property state (same pointer-stability guarantee).
	std::unordered_map<std::string, std::unique_ptr<CallbackViewProperty>> callback_view_props_;
	// Names of funcdefs successfully registered with the engine (GetTypeInfoByName does not
	// cover funcdefs, so we track them ourselves to avoid duplicate registration attempts).
	std::unordered_set<std::string> registered_funcdefs_;
	// Preamble injected as a script section into every module at compile time so constants are
	// visible in all scripts without import or include statements.
	std::string constants_preamble_;
	// Monotonic counter used to give every compiled module a unique name (see CompileSource).
	// Mutable because CompileSource is logically const but must advance the counter.
	mutable std::uint64_t next_module_id_{0};
};

} // namespace engine::scripting::angelscript
