#include "angelscript_backend.hpp"
#include "angelscript_helpers.hpp"

#include <angelscript/autowrapper/aswrappedcall.h>
#include <angelscript/scriptstdstring/scriptstdstring.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>
#include <string>

namespace engine::scripting::angelscript {

// -------------------------------------------------------------------------
// Helpers (local to core runtime)
// -------------------------------------------------------------------------

namespace {

void MessageCallback(const asSMessageInfo* msg, void* /*param*/) {
	switch (msg->type) {
	case asMSGTYPE_ERROR:
		spdlog::error("[AngelScript] {} ({},{}): {}", msg->section, msg->row, msg->col, msg->message);
		break;
	case asMSGTYPE_WARNING:
		spdlog::warn("[AngelScript] {} ({},{}): {}", msg->section, msg->row, msg->col, msg->message);
		break;
	case asMSGTYPE_INFORMATION:
		// Also log information messages at info level so AS follow-up details are visible
		spdlog::info("[AngelScript] {} ({},{}): {}", msg->section, msg->row, msg->col, msg->message);
		break;
	default: spdlog::debug("[AngelScript] {} ({},{}): {}", msg->section, msg->row, msg->col, msg->message); break;
	}
}

const flecs::Component* GetComponentInfo(const flecs::world& world, const ecs_entity_t component_id) {
	return world.entity(component_id).try_get<flecs::Component>();
}

// Component accessors are exposed as Entity methods, but a script's "self" is the script
// child entity — the component-bearing host is its parent. Resolve to the parent when one
// exists so `self.GetTransform()` reads the host's Transform; fall back to the entity itself
// for top-level entities that have no parent.
flecs::entity ResolveHostEntity(const flecs::entity entity) {
	const flecs::entity parent = entity.parent();
	return parent.is_valid() ? parent : entity;
}
void PrintFn(const std::string& msg) { spdlog::info("[Script] {}", msg); }

// Adapts AngelScript's generic-call context to the backend-neutral ScriptCallContext so a
// factory-generated thunk can marshal a component method without knowing about AngelScript.
// AngelScript stores registered value types (including std::string) by pointer in the generic
// frame, so object/string arguments resolve through GetArgObject / GetAddressOfReturnLocation.
class AngelScriptCallContext final : public ScriptCallContext {
public:
	explicit AngelScriptCallContext(asIScriptGeneric* gen) : gen_(gen) {}

	[[nodiscard]] void* GetObject() const override { return gen_->GetObject(); }

	[[nodiscard]] bool GetArgBool(const int index) const override {
		return gen_->GetArgByte(static_cast<asUINT>(index)) != 0;
	}
	[[nodiscard]] int GetArgInt(const int index) const override {
		return static_cast<int>(gen_->GetArgDWord(static_cast<asUINT>(index)));
	}
	[[nodiscard]] float GetArgFloat(const int index) const override {
		return gen_->GetArgFloat(static_cast<asUINT>(index));
	}
	[[nodiscard]] std::string GetArgString(const int index) const override {
		const auto* str = static_cast<const std::string*>(gen_->GetArgObject(static_cast<asUINT>(index)));
		return str ? *str : std::string{};
	}
	[[nodiscard]] void* GetArgObject(const int index) const override {
		return gen_->GetArgObject(static_cast<asUINT>(index));
	}

	void SetReturnBool(const bool value) override { gen_->SetReturnByte(value ? 1 : 0); }
	void SetReturnInt(const int value) override { gen_->SetReturnDWord(static_cast<asDWORD>(value)); }
	void SetReturnFloat(const float value) override { gen_->SetReturnFloat(value); }
	void SetReturnString(const std::string& value) override {
		new (gen_->GetAddressOfReturnLocation()) std::string(value);
	}

	[[nodiscard]] void* GetReturnObjectLocation() override { return gen_->GetAddressOfReturnLocation(); }

	void SetReturnObjectHandle(void* ptr) override { gen_->SetReturnAddress(ptr); }

private:
	asIScriptGeneric* gen_;
};

// Backend-neutral ScriptFunctionHandle over an asIScriptFunction*. Created when a script passes a
// function handle to a callback-binding function (see RegisterCallbackFunction); the sink wraps it
// (typically in a std::function on a component or descriptor) and fires it via Invoke(). The
// recorded parameter list drives argument marshalling back into AngelScript at invoke time.
class AngelScriptFunctionHandle final : public ScriptFunctionHandle {
public:
	AngelScriptFunctionHandle(asIScriptEngine* engine, asIScriptFunction* fn, std::vector<ScriptMethodParam> params) :
			engine_(engine), fn_(fn), params_(std::move(params)) {
		fn_->AddRef();
	}

	~AngelScriptFunctionHandle() override { fn_->Release(); }

	void AddRef() override { ++ref_count_; }

	void Release() override {
		if (--ref_count_ <= 0) delete this;
	}

	void Invoke(const void* const* args, const int arg_count) override {
		asIScriptContext* ctx = engine_->CreateContext();
		if (!ctx) return;
		if (ctx->Prepare(fn_) < 0) {
			ctx->Unprepare();
			ctx->Release();
			return;
		}
		for (int i = 0; i < arg_count && i < static_cast<int>(params_.size()); ++i) {
			const auto idx = static_cast<asUINT>(i);
			switch (params_[i].type.kind) {
			case ScriptValueKind::Bool: ctx->SetArgByte(idx, *static_cast<const bool*>(args[i]) ? 1 : 0); break;
			case ScriptValueKind::Int:
				ctx->SetArgDWord(idx, static_cast<asDWORD>(*static_cast<const int*>(args[i])));
				break;
			case ScriptValueKind::Float: ctx->SetArgFloat(idx, *static_cast<const float*>(args[i])); break;
			case ScriptValueKind::String:
			case ScriptValueKind::Object:
				// Value-type args (including Entity and the registered std::string) are passed by
				// pointer; AngelScript copies the bytes into the call frame.
				ctx->SetArgObject(idx, const_cast<void*>(args[i]));
				break;
			default:
				spdlog::warn("[AngelScript] Unsupported callback arg kind {}", static_cast<int>(params_[i].type.kind));
				break;
			}
		}
		if (ctx->Execute() == asEXECUTION_EXCEPTION) {
			spdlog::error("[AngelScript] Exception in callback '{}': {}", fn_->GetName(), ctx->GetExceptionString());
		}
		ctx->Unprepare();
		ctx->Release();
	}

private:
	asIScriptEngine* engine_;
	asIScriptFunction* fn_;
	std::vector<ScriptMethodParam> params_;
	int ref_count_{1};
};

} // namespace

// -------------------------------------------------------------------------
// AngelScriptBackend — core lifecycle and entity/global registration
// (Type registration lives in angelscript_type_registration.cpp)
// -------------------------------------------------------------------------

AngelScriptBackend::~AngelScriptBackend() { Shutdown(); }

bool AngelScriptBackend::Init(flecs::world& world) {
	engine_ = asCreateScriptEngine();
	if (!engine_) {
		spdlog::error("[AngelScript] asCreateScriptEngine() failed");
		return false;
	}

	world_ = &world;
	engine_->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL);
	engine_->SetUserData(world_, 1);
	RegisterStdString(engine_);
	RegisterEntityType();
	RegisterGlobalFunctions();

	// Register ScriptEntityRef with Flecs meta under the "Entity" name so that callback
	// parameter types can reference it via ScriptValueType::MakeObject(world.component<ScriptEntityRef>())
	// and ScriptTypeName() resolves it to the "Entity" string AngelScript expects.
	world.component<ScriptEntityRef>("Entity");

	spdlog::info("[AngelScript] Engine initialized");
	return true;
}

void AngelScriptBackend::Shutdown() {
	// Run shutdown callbacks first so that C++ objects (like TileCallback lambdas) can release
	// their ScriptFunctionHandle references before the engine is torn down.
	for (auto& cb : shutdown_callbacks_) {
		if (cb) cb(*world_);
	}
	shutdown_callbacks_.clear();

	spdlog::debug("[AngelScript] Shutdown: {} script instances remaining", instances_.size());
	for (const AngelScriptInstance* inst : instances_) {
		delete inst;
	}
	instances_.clear();

	if (engine_) {
		// NOTE: A residual warning may still appear for script functions if AS's internal
		// GC/context-pool processing increments externalRefCount between our cleanup and the
		// module-discard check inside ShutDownAndRelease. This is a benign timing issue.
		engine_->GarbageCollect(asGC_FULL_CYCLE);
		engine_->ShutDownAndRelease();
		engine_ = nullptr;
	}

	world_ = nullptr;
}

IScriptInstance* AngelScriptBackend::CreateInstanceFromFile(std::string_view path, const flecs::entity entity) {
	const std::ifstream file{std::string{path}};
	if (!file.is_open()) {
		spdlog::error("[AngelScript] Cannot open script file '{}'", path);
		return nullptr;
	}

	std::ostringstream ss;
	ss << file.rdbuf();
	return CreateInstanceFromSource(path, ss.str(), entity);
}

IScriptInstance* AngelScriptBackend::CreateInstanceFromSource(
	const std::string_view module_name,
	const std::string_view source,
	const flecs::entity entity
) {
	asIScriptModule* module = CompileSource(module_name, source);
	if (!module) {
		return nullptr;
	}

	auto* inst = new AngelScriptInstance(engine_, module, entity);
	instances_.insert(inst);
	return inst;
}

void AngelScriptBackend::DestroyInstance(IScriptInstance* instance) {
	auto* inst = dynamic_cast<AngelScriptInstance*>(instance);
	instances_.erase(inst);
	delete inst;
}

// -------------------------------------------------------------------------
// Private helpers — entity type + global function registration
// -------------------------------------------------------------------------

void AngelScriptBackend::RegisterEntityType() {
	// Register Entity as a VALUE type. ScriptEntityRef is essentially two POD values
	// (world ptr + entity id), so copy semantics work perfectly and avoid the
	// handle-vs-temporary lifetime mismatch that crashes with asOBJ_REF | asOBJ_NOCOUNT.
	engine_->RegisterObjectType("Entity", sizeof(ScriptEntityRef), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CDAK);

	// All methods are bound with asCALL_GENERIC via the autowrapper (WRAP_MFN).

	// Identity
	engine_->RegisterObjectMethod("Entity", "uint64 GetId() const", WRAP_MFN(ScriptEntityRef, GetId), asCALL_GENERIC);
	engine_
		->RegisterObjectMethod("Entity", "string GetName() const", WRAP_MFN(ScriptEntityRef, GetName), asCALL_GENERIC);

	// Component presence
	engine_->RegisterObjectMethod(
		"Entity",
		"bool HasComponent(const string &in) const",
		WRAP_MFN(ScriptEntityRef, HasComponent),
		asCALL_GENERIC
	);
	engine_->RegisterObjectMethod(
		"Entity",
		"Entity GetParent() const",
		WRAP_MFN(ScriptEntityRef, GetParent),
		asCALL_GENERIC
	);

	// Float field access
	engine_->RegisterObjectMethod(
		"Entity",
		"float GetFloat(const string &in, const string &in) const",
		WRAP_MFN(ScriptEntityRef, GetFloat),
		asCALL_GENERIC
	);
	engine_->RegisterObjectMethod(
		"Entity",
		"void SetFloat(const string &in, const string &in, float)",
		WRAP_MFN(ScriptEntityRef, SetFloat),
		asCALL_GENERIC
	);

	// Int field access
	engine_->RegisterObjectMethod(
		"Entity",
		"int GetInt(const string &in, const string &in) const",
		WRAP_MFN(ScriptEntityRef, GetInt),
		asCALL_GENERIC
	);
	engine_->RegisterObjectMethod(
		"Entity",
		"void SetInt(const string &in, const string &in, int)",
		WRAP_MFN(ScriptEntityRef, SetInt),
		asCALL_GENERIC
	);

	// Bool field access
	engine_->RegisterObjectMethod(
		"Entity",
		"bool GetBool(const string &in, const string &in) const",
		WRAP_MFN(ScriptEntityRef, GetBool),
		asCALL_GENERIC
	);
	engine_->RegisterObjectMethod(
		"Entity",
		"void SetBool(const string &in, const string &in, bool)",
		WRAP_MFN(ScriptEntityRef, SetBool),
		asCALL_GENERIC
	);

	// Observer registration
	engine_->RegisterObjectMethod(
		"Entity",
		"void RegisterObserver(const string &in, const string &in)",
		WRAP_MFN(ScriptEntityRef, RegisterObserver),
		asCALL_GENERIC
	);
}

void AngelScriptBackend::RegisterGlobalFunctions() const {
	engine_->RegisterGlobalFunction("void Print(const string &in msg)", WRAP_FN(PrintFn), asCALL_GENERIC);
}

void AngelScriptBackend::ComponentGetRefGeneric(asIScriptGeneric* gen) {
	// Returns a mutable handle (T@) directly into Flecs component storage so scripts can modify
	// the component in-place without a separate SetT() call.
	//
	// **Modified notification trade-off**: ecs_modified_id is called unconditionally after every
	// GetT() so that in-place mutations (e.g. `self.GetSoundEffect().Fire()`) are seen by OnSet
	// observers and change-detection queries without requiring a manual SetT() write-back. The
	// downside is that every call — even a read-only access — marks the component dirty and can
	// wake observers or invalidate caches. This is intentional for correctness and convenience at
	// the cost of some per-tick overhead. See scripting/README.md for the design rationale and
	// the TODO for a future fine-grained solution (e.g. const vs mutable overloads).
	//
	// Pointer stability: structural changes (add/remove) are deferred during world.progress(), so
	// the returned pointer is stable for the duration of the tick. If the component is absent a
	// warning is logged and a null handle returned — the script will fault on access, which is
	// intentional: call AddT() first to ensure the component exists.
	const auto component_id = reinterpret_cast<uintptr_t>(gen->GetAuxiliary());
	const auto* entity_ref = static_cast<ScriptEntityRef*>(gen->GetObject());
	if (!entity_ref || component_id == 0) {
		gen->SetReturnAddress(nullptr);
		return;
	}

	const flecs::entity host = ResolveHostEntity(entity_ref->GetEntity());
	void* ptr = ecs_get_mut_id(host.world().c_ptr(), host.id(), component_id);
	if (ptr == nullptr) {
		spdlog::warn(
			"[AngelScript] GetT: entity '{}' does not have component id {}; returning null handle — call AddT() first",
			host.name(),
			component_id
		);
	}
	else {
		host.modified(component_id);
	}
	gen->SetReturnAddress(ptr);
}

void AngelScriptBackend::ComponentAddGeneric(asIScriptGeneric* gen) {
	// Ensures the component exists on the entity (adds zero-initialised if absent), then returns
	// a mutable handle (T@) into the ECS storage. This replaces the old "create local value, call
	// SetT()" pattern: scripts call AddT() once, modify fields via the handle, and the ECS
	// component is immediately updated without a separate write-back step.
	// NOTE: ecs_ensure_id fires OnAdd (if newly added) but NOT OnSet. Call SetT() explicitly
	// when an OnSet observer must trigger (e.g. AudioSource which resolves handles on OnSet).
	const auto component_id = reinterpret_cast<uintptr_t>(gen->GetAuxiliary());
	const auto* entity_ref = static_cast<ScriptEntityRef*>(gen->GetObject());
	if (!entity_ref || component_id == 0) {
		gen->SetReturnAddress(nullptr);
		return;
	}

	const flecs::entity host = ResolveHostEntity(entity_ref->GetEntity());
	void* ptr = ecs_ensure_id(host.world().c_ptr(), host.id(), component_id);
	gen->SetReturnAddress(ptr);
}

void AngelScriptBackend::ComponentSetGeneric(asIScriptGeneric* gen) {
	const auto component_id = reinterpret_cast<uintptr_t>(gen->GetAuxiliary());
	// The method receiver is the Entity; the argument is a T@ handle (ref type).
	// GetArgObject returns the raw pointer for a handle argument (the T* itself).
	const auto* entity_ref = static_cast<ScriptEntityRef*>(gen->GetObject());
	const void* value = gen->GetArgObject(0);
	if (!entity_ref || !value || component_id == 0) {
		return;
	}

	const flecs::entity host = ResolveHostEntity(entity_ref->GetEntity());
	const auto* component_info = GetComponentInfo(host.world(), component_id);
	if (!component_info) {
		spdlog::warn("[AngelScript] ComponentSetGeneric: component_id {} has no type info", component_id);
		return;
	}

	// set_ptr adds the component if the entity doesn't already have it.
	host.set_ptr(component_id, static_cast<size_t>(component_info->size), value);
}

void AngelScriptBackend::SingletonGetGeneric(asIScriptGeneric* gen) {
	const auto component_id = reinterpret_cast<uintptr_t>(gen->GetAuxiliary());
	void* out = gen->GetAddressOfReturnLocation();
	const asIScriptContext* ctx = asGetActiveContext();
	if (!out || component_id == 0 || ctx == nullptr) {
		return;
	}

	const auto* world = static_cast<flecs::world*>(ctx->GetEngine()->GetUserData(1));
	if (world == nullptr) {
		return;
	}

	const auto* component_info = GetComponentInfo(*world, component_id);
	if (!component_info) {
		return;
	}

	// Singletons are stored as the component set on the world; try_get(id) returns that value.
	if (const void* singleton = world->try_get(component_id)) {
		std::memcpy(out, singleton, static_cast<size_t>(component_info->size));
		return;
	}

	// Singleton not set — return a zero-initialised value.
	std::memset(out, 0, static_cast<size_t>(component_info->size));
}

void AngelScriptBackend::ComponentMethodGeneric(asIScriptGeneric* gen) {
	// The per-method thunk is carried as the generic auxiliary. Wrap the AngelScript call frame
	// in a neutral context and let the factory-generated thunk marshal the arguments and return.
	const auto thunk = reinterpret_cast<ScriptGenericThunk>(gen->GetAuxiliary());
	if (thunk == nullptr) {
		return;
	}
	AngelScriptCallContext ctx{gen};
	thunk(ctx);
}

void AngelScriptBackend::GlobalFunctionGeneric(asIScriptGeneric* gen) {
	// The thunk pointer is stored as the auxiliary so each registered global function can carry
	// its own callable without a secondary lookup. The world pointer is retrieved from the engine
	// user data (slot 1) — the same channel used by IsKeyDownFn.
	const auto* thunk = static_cast<GlobalFunctionThunk*>(gen->GetAuxiliary());
	auto* world = static_cast<flecs::world*>(gen->GetEngine()->GetUserData(1));
	if (thunk == nullptr || world == nullptr) {
		return;
	}
	AngelScriptCallContext ctx{gen};
	(*thunk)(ctx, *world);
}

void AngelScriptBackend::RegisterGlobalFunction(const ScriptMethodSignature& sig, GlobalFunctionThunk thunk) {
	if (!engine_ || !world_) {
		return;
	}

	const std::string declaration = BuildMethodDeclaration(*world_, sig);
	if (declaration.empty()) {
		spdlog::warn("[AngelScript] Skipping global function '{}' — unresolved parameter/return type", sig.name);
		return;
	}

	// Keep the thunk alive in a stable heap allocation so its address remains valid as the
	// auxiliary pointer passed to GlobalFunctionGeneric (a vector reallocation would invalidate
	// a pointer into the vector's element storage).
	auto thunk_ptr = std::make_unique<GlobalFunctionThunk>(std::move(thunk));
	if (engine_->RegisterGlobalFunction(
			declaration.c_str(),
			asFUNCTION(AngelScriptBackend::GlobalFunctionGeneric),
			asCALL_GENERIC,
			thunk_ptr.get()
		)
		< 0) {
		spdlog::error("[AngelScript] Failed to register global function '{}'", sig.name);
		return;
	}
	global_thunks_.push_back(std::move(thunk_ptr));
}

void AngelScriptBackend::CallbackFunctionGeneric(asIScriptGeneric* gen) {
	// The binding (sink + callback signature) is carried as the auxiliary; the world comes from
	// engine user data (slot 1). The trailing argument is always the script function handle.
	auto* binding = static_cast<CallbackBinding*>(gen->GetAuxiliary());
	auto* world = static_cast<flecs::world*>(gen->GetEngine()->GetUserData(1));
	if (binding == nullptr || !binding->sink || world == nullptr) {
		return;
	}

	auto* as_fn = static_cast<asIScriptFunction*>(gen->GetArgObject(static_cast<asUINT>(binding->callback_arg_index)));
	if (as_fn == nullptr) {
		return; // null callback passed; nothing to bind
	}

	// The handle holds the backend's single reference for the duration of the call. The sink
	// AddRef()s it if it retains the callback; we drop our reference immediately afterwards.
	auto* handle = new AngelScriptFunctionHandle(gen->GetEngine(), as_fn, binding->callback_params);
	AngelScriptCallContext ctx{gen};
	binding->sink(ctx, *world, handle);
	handle->Release();
}

void AngelScriptBackend::RegisterCallbackFunction(const ScriptCallbackFunctionDesc& desc, ScriptCallbackSink sink) {
	if (!engine_ || !world_ || !sink) {
		return;
	}

	const std::string& fdname = desc.callback.funcdef_name;
	if (fdname.empty()) {
		spdlog::warn("[AngelScript] Callback function '{}' has no funcdef name", desc.name);
		return;
	}

	// Format one parameter as its AngelScript declaration fragment, honoring handle/reference
	// (delegates to the shared helper so all declaration builders stay consistent).
	const auto format_param = [this](const ScriptMethodParam& p) -> std::string { return FormatParam(*world_, p); };

	// Register the callback funcdef (deduplicated by name across bindings that share a signature).
	if (engine_->GetTypeInfoByName(fdname.c_str()) == nullptr) {
		const std::string fdecl = BuildFuncdefDeclaration(*world_, fdname, desc.callback.params);
		if (fdecl.empty()) {
			spdlog::error("[AngelScript] Unresolved callback param type for funcdef '{}'", fdname);
			return;
		}
		if (engine_->RegisterFuncdef(fdecl.c_str()) < 0) {
			spdlog::error("[AngelScript] Failed to register funcdef '{}'", fdecl);
			return;
		}
	}

	// Build `void <name>(<fixed params...>, <funcdef>@ <callback name>)`.
	std::string decl = "void " + desc.name + "(";
	for (size_t i = 0; i < desc.fixed_params.size(); ++i) {
		const std::string frag = format_param(desc.fixed_params[i]);
		if (frag.empty()) {
			spdlog::warn("[AngelScript] Skipping callback function '{}' — unresolved parameter type", desc.name);
			return;
		}
		decl += (i == 0 ? "" : ", ") + frag;
	}
	if (!desc.fixed_params.empty()) {
		decl += ", ";
	}
	decl += fdname + "@ " + (desc.callback.name.empty() ? "callback" : desc.callback.name);
	decl += ")";

	auto binding = std::make_unique<CallbackBinding>();
	binding->sink = std::move(sink);
	binding->callback_params = desc.callback.params;
	binding->callback_arg_index = static_cast<int>(desc.fixed_params.size());

	if (engine_->RegisterGlobalFunction(
			decl.c_str(),
			asFUNCTION(AngelScriptBackend::CallbackFunctionGeneric),
			asCALL_GENERIC,
			binding.get()
		)
		< 0) {
		spdlog::error("[AngelScript] Failed to register callback function '{}'", desc.name);
		return;
	}
	callback_bindings_.push_back(std::move(binding));
}

void AngelScriptBackend::CallbackPropertySetGeneric(asIScriptGeneric* gen) {
	// Auxiliary carries the property's sink + callback signature. The receiver is the C++ view
	// object; the single argument is the assigned script function (null when the property is cleared).
	auto* state = static_cast<CallbackViewProperty*>(gen->GetAuxiliary());
	if (state == nullptr || !state->sink) {
		return;
	}
	void* object = gen->GetObject();
	if (object == nullptr) {
		return;
	}

	auto* as_fn = static_cast<asIScriptFunction*>(gen->GetArgObject(0));
	if (as_fn == nullptr) {
		state->sink(object, nullptr); // cleared: obj.prop = null
		return;
	}

	// The handle holds the backend's single reference for the duration of the call; the sink
	// AddRef()s it if it retains the callback, and we drop our reference immediately afterwards.
	auto* handle = new AngelScriptFunctionHandle(state->engine, as_fn, state->params);
	state->sink(object, handle);
	handle->Release();
}

void AngelScriptBackend::CallbackPropertyInvokeGeneric(asIScriptGeneric* gen) {
	// Invoke the stored callback on the receiver view, marshalling the script arguments through
	// the neutral context. Works regardless of whether the callback was registered from script or
	// C++, because both are invoked through the object's std::function.
	auto* state = static_cast<CallbackViewProperty*>(gen->GetAuxiliary());
	if (state == nullptr || !state->invoke) {
		return;
	}
	void* object = gen->GetObject();
	if (object == nullptr) {
		return;
	}
	AngelScriptCallContext ctx{gen};
	state->invoke(object, ctx);
}

void AngelScriptBackend::RegisterCallbackViewType(
	const flecs::entity type_entity,
	const std::vector<ScriptTypeField>& fields,
	std::vector<ScriptCallbackProperty> callback_props
) {
	if (!engine_ || !world_ || !type_entity.is_valid()) {
		return;
	}
	const char* type_name = type_entity.name();
	if (!type_name || *type_name == '\0') {
		return;
	}

	// Non-owning reference view — instances are handles into C++-owned storage, so no factory or
	// reference counting is registered (same shape as component accessors).
	if (engine_->GetTypeIdByDecl(type_name) < 0) {
		engine_->RegisterObjectType(type_name, 0, asOBJ_REF | asOBJ_NOCOUNT);
	}

	for (const auto& [decl, offset] : fields) {
		if (engine_->RegisterObjectProperty(type_name, decl.c_str(), offset) < 0) {
			spdlog::error("[AngelScript] Failed to register property '{}' on '{}'", decl, type_name);
		}
	}

	for (auto& prop : callback_props) {
		const std::string fdname =
			prop.funcdef_name.empty() ? std::string{type_name} + "_" + prop.name + "Fn" : prop.funcdef_name;

		// Register the callback funcdef (deduplicated by name).
		if (engine_->GetTypeInfoByName(fdname.c_str()) == nullptr) {
			const std::string fdecl = BuildFuncdefDeclaration(*world_, fdname, prop.params);
			if (fdecl.empty()) {
				spdlog::error("[AngelScript] Unresolved callback param type for funcdef '{}'", fdname);
				return;
			}
			if (engine_->RegisterFuncdef(fdecl.c_str()) < 0) {
				spdlog::error("[AngelScript] Failed to register funcdef '{}'", fdecl);
				return;
			}
		}

		auto state = std::make_unique<CallbackViewProperty>();
		state->sink = std::move(prop.sink);
		state->invoke = std::move(prop.invoke);
		state->params = prop.params;
		state->engine = engine_;

		// Callback setter method: `view.Set<Name>(@fn)`. AngelScript property-assignment syntax
		// (`view.name = @fn`) is intentionally NOT used: property setters coerce their value
		// parameter to `const T &in`, which is invalid for funcdef handles, so a plain method is
		// the portable way to accept a script function.
		const std::string setter = "void Set" + prop.name + "(" + fdname + "@ fn)";
		if (engine_->RegisterObjectMethod(
				type_name,
				setter.c_str(),
				asFUNCTION(AngelScriptBackend::CallbackPropertySetGeneric),
				asCALL_GENERIC,
				state.get()
			)
			< 0) {
			spdlog::error("[AngelScript] Failed to register callback setter '{}' on '{}'", prop.name, type_name);
			return;
		}

		// Optional invoker method: `view.<Name>(<params>)` fires the stored callback directly,
		// whether it was registered from script or C++ (both go through the object's std::function).
		if (state->invoke) {
			std::string invoke_decl = "void " + prop.name + "(";
			for (size_t i = 0; i < prop.params.size(); ++i) {
				const std::string frag = FormatParam(*world_, prop.params[i]);
				if (frag.empty()) {
					spdlog::error("[AngelScript] Unresolved invoke param type for '{}.{}'", type_name, prop.name);
					return;
				}
				invoke_decl += (i == 0 ? "" : ", ") + frag;
			}
			invoke_decl += ")";
			if (engine_->RegisterObjectMethod(
					type_name,
					invoke_decl.c_str(),
					asFUNCTION(AngelScriptBackend::CallbackPropertyInvokeGeneric),
					asCALL_GENERIC,
					state.get()
				)
				< 0) {
				spdlog::error("[AngelScript] Failed to register callback invoker '{}' on '{}'", prop.name, type_name);
				return;
			}
		}
		callback_view_props_.push_back(std::move(state));
	}
}

asIScriptModule* AngelScriptBackend::CompileSource(std::string_view module_name, const std::string_view source) const {
	// Module identity must be unique per instance. AngelScript keys modules by name, and
	// asGM_ALWAYS_CREATE destroys any existing module of the same name — releasing its
	// asIScriptFunctions. Two live instances compiled from the same source path (e.g. two level
	// entities referencing the same script) would otherwise collide, leaving the first instance's
	// cached function pointers dangling. Append a monotonic id so every module is independent; the
	// human-readable source name is kept as the script-section label for diagnostics.
	const std::string unique_name = std::string{module_name} + "#" + std::to_string(next_module_id_++);
	asIScriptModule* module = engine_->GetModule(unique_name.c_str(), asGM_ALWAYS_CREATE);

	// Inject the accumulated constants preamble (key codes, tuning constants, etc.) so every
	// script sees them without explicit includes.
	if (!constants_preamble_.empty()) {
		module->AddScriptSection("__constants__", constants_preamble_.c_str(), constants_preamble_.size());
	}

	module->AddScriptSection(std::string{module_name}.c_str(), source.data(), source.size());
	if (const int result = module->Build(); result < 0) {
		spdlog::error("[AngelScript] Compile failed for module '{}' (error {})", module_name, result);
		engine_->DiscardModule(unique_name.c_str());
		return nullptr;
	}
	return module;
}

// -------------------------------------------------------------------------
// AngelScriptInstance
// -------------------------------------------------------------------------

AngelScriptInstance::AngelScriptInstance(asIScriptEngine* engine, asIScriptModule* module, const flecs::entity entity) :
		engine_(engine), module_(module), ctx_(engine->CreateContext()), entity_ref_(entity) {
	fn_on_init_ = module_->GetFunctionByDecl("void OnInit(Entity self)");
	fn_pre_tick_ = module_->GetFunctionByDecl("void PreTick(Entity self, float dt)");
	fn_on_tick_ = module_->GetFunctionByDecl("void Tick(Entity self, float dt)");
	fn_post_tick_ = module_->GetFunctionByDecl("void PostTick(Entity self, float dt)");
	fn_on_destroy_ = module_->GetFunctionByDecl("void OnDestroy(Entity self)");
}

AngelScriptInstance::~AngelScriptInstance() {
	if (ctx_) {
		// Explicitly unprepare before releasing: this releases the reference held by the
		// last ctx_->Prepare(fn) call (m_initialFunction in asCContext) so the function's
		// refcount reaches 1 (module's reference) before the engine discards modules.
		// Without this, a pooled or in-FINISHED context may retain the reference until
		// after module discard, triggering "external reference" shutdown warnings.
		ctx_->Unprepare();
		ctx_->Release();
		ctx_ = nullptr;
	}
	// Discard this instance's (uniquely-named) module so its bytecode and functions are reclaimed
	// instead of leaking for the engine's lifetime. Script functions still referenced elsewhere
	// (e.g. a callback that AddRef'd a handle) survive the discard per AngelScript semantics. This is
	// safe at final teardown too because the backend deletes all instances before ShutDownAndRelease.
	if (engine_ && module_) {
		engine_->DiscardModule(module_->GetName());
		module_ = nullptr;
	}
}

void AngelScriptInstance::OnInit(flecs::entity /*entity*/) { Call(fn_on_init_, entity_ref_.GetEntity()); }

void AngelScriptInstance::Tick(flecs::entity /*entity*/, const float dt, const ScriptTickPhase phase) {
	switch (phase) {
	case ScriptTickPhase::Pre: Call(fn_pre_tick_, entity_ref_.GetEntity(), dt, true); break;
	case ScriptTickPhase::On: Call(fn_on_tick_, entity_ref_.GetEntity(), dt, true); break;
	case ScriptTickPhase::Post: Call(fn_post_tick_, entity_ref_.GetEntity(), dt, true); break;
	}
}

void AngelScriptInstance::OnDestroy(flecs::entity /*entity*/) { Call(fn_on_destroy_, entity_ref_.GetEntity()); }

void AngelScriptInstance::Call(asIScriptFunction* fn, flecs::entity /*entity*/, const float dt, const bool pass_dt) {
	if (!fn || !ctx_) {
		return;
	}

	if (const int prep = ctx_->Prepare(fn); prep < 0) {
		spdlog::error("[AngelScript] Prepare failed for '{}' (error {})", fn->GetName(), prep);
		return;
	}

	// Entity is a value type passed by value — SetArgObject copies it into the context's arg slot.
	ctx_->SetArgObject(0, &entity_ref_);
	if (pass_dt) {
		ctx_->SetArgFloat(1, dt);
	}

	if (const int result = ctx_->Execute(); result == asEXECUTION_EXCEPTION) {
		spdlog::error("[AngelScript] Exception in '{}': {}", fn->GetName(), ctx_->GetExceptionString());
	}
	else if (result < 0) {
		spdlog::error("[AngelScript] Execute failed for '{}' (error {})", fn->GetName(), result);
	}
	// Unprepare after each call so the context doesn't retain a reference to fn via
	// m_initialFunction between ticks, avoiding shutdown warnings.
	ctx_->Unprepare();
}

} // namespace engine::scripting::angelscript
