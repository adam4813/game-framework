#pragma once

#include "engine/scripting/script_backend.hpp"
#include "engine/scripting/script_context.hpp"

#include <angelscript.h>
#include <flecs.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace engine::scripting::angelscript {

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
	void RegisterShutdownCallback(const std::function<void(flecs::world&)>& callback) override;
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
	static void ComponentAddGeneric(asIScriptGeneric* gen);
	static void ComponentSetGeneric(asIScriptGeneric* gen);
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
	// Stable storage for global function thunks so their addresses remain valid as auxiliaries
	// passed to the AngelScript generic dispatcher (vector reallocation would invalidate them).
	std::vector<std::unique_ptr<GlobalFunctionThunk>> global_thunks_;
	// Shutdown callbacks are run before the engine is released to give registrants a chance
	// to nullify any ScriptFunctionHandle-holding std::functions they own.
	std::vector<std::function<void(flecs::world&)>> shutdown_callbacks_;
	// Stable storage for callback-binding state, each passed as a generic auxiliary.
	std::vector<std::unique_ptr<CallbackBinding>> callback_bindings_;
	// Stable storage for callback view-property state, each passed as a generic auxiliary.
	std::vector<std::unique_ptr<CallbackViewProperty>> callback_view_props_;
	// Preamble injected as a script section into every module at compile time so constants are
	// visible in all scripts without import or include statements.
	std::string constants_preamble_;
	// Monotonic counter used to give every compiled module a unique name (see CompileSource).
	// Mutable because CompileSource is logically const but must advance the counter.
	mutable std::uint64_t next_module_id_{0};
};

} // namespace engine::scripting::angelscript
