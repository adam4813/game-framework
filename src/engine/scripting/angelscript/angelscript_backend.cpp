#include "angelscript_backend.hpp"
#include "angelscript_helpers.hpp"

#include <angelscript/autowrapper/aswrappedcall.h>
#include <angelscript/scriptarray/scriptarray.h>
#include <angelscript/scriptstdstring/scriptstdstring.h>
#include <glm/glm.hpp>

#include <spdlog/spdlog.h>
#include "engine/ecs/ecs.hpp"

#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>

namespace engine::scripting::angelscript {

// Thread-local pointer to the AngelScriptInstance whose script is currently executing, so that
// callback generics (SetOnClick, SetOnEnter etc.) can associate newly created handles with the
// right owning instance. Set through CurrentInstanceScope only.
thread_local AngelScriptInstance* t_current_instance = nullptr;

// -------------------------------------------------------------------------
// Helpers (local to core runtime)
// -------------------------------------------------------------------------

namespace {

// Publishes `instance` as t_current_instance for one script execution and restores the previous
// value afterwards, so a call nested inside another script's call (a callback a C++ binding
// invokes synchronously) hands the outer script's owner back when it returns instead of clearing it.
class CurrentInstanceScope {
public:
	explicit CurrentInstanceScope(AngelScriptInstance* instance) : previous_(t_current_instance) {
		t_current_instance = instance;
	}
	~CurrentInstanceScope() { t_current_instance = previous_; }
	CurrentInstanceScope(const CurrentInstanceScope&) = delete;
	CurrentInstanceScope& operator=(const CurrentInstanceScope&) = delete;

private:
	AngelScriptInstance* previous_;
};

// asIScriptEngine user-data slots: the flecs::world* the generics act on, and the
// AngelScriptBackend* for state the static generics keep.
constexpr asPWORD kWorldUserDataSlot = 1;
constexpr asPWORD kBackendUserDataSlot = 2;

// The logger of the world `engine` serves (spdlog's default before Init sets the world).
spdlog::logger& EngineLog(asIScriptEngine* engine) {
	const auto* world = static_cast<flecs::world*>(engine->GetUserData(kWorldUserDataSlot));
	return world ? ecs::WorldLog(*world) : *spdlog::default_logger();
}

// Executes the prepared `ctx` as `owner`'s script (see CurrentInstanceScope) and logs an
// exception, with its line, or a failure to run. `what` names the call in the log. Returns the
// asIScriptContext::Execute result.
int ExecuteAs(asIScriptContext* ctx, AngelScriptInstance* owner, const std::string_view what) {
	int result = 0;
	{
		const CurrentInstanceScope scope{owner};
		result = ctx->Execute();
	}
	if (result == asEXECUTION_EXCEPTION) {
		EngineLog(ctx->GetEngine())
			.error(
				"[AngelScript] Exception in {} (line {}): {}",
				what,
				ctx->GetExceptionLineNumber(),
				ctx->GetExceptionString()
			);
	}
	else if (result < 0) {
		EngineLog(ctx->GetEngine()).error("[AngelScript] Execute failed for {} (error {})", what, result);
	}
	return result;
}

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

std::optional<std::pair<flecs::entity, uintptr_t>> ResolveEntityAndComponent(asIScriptGeneric* gen) {
	const auto component_id = reinterpret_cast<uintptr_t>(gen->GetAuxiliary());
	const auto* entity_ref = static_cast<ScriptEntityRef*>(gen->GetObject());
	if (!entity_ref || component_id == 0) {
		return {};
	}

	return {{entity_ref->GetEntity(), component_id}};
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

	void SetReturnArray(const flecs::entity_t element_type, const void* elements, const size_t count) override {
		asIScriptEngine* engine = gen_->GetEngine();
		const auto* world = static_cast<flecs::world*>(engine->GetUserData(kWorldUserDataSlot));
		const char* element_name = world ? world->entity(element_type).name().c_str() : nullptr;
		const ecs_type_info_t* type_info = world ? ecs_get_type_info(*world, element_type) : nullptr;
		asITypeInfo* array_type =
			element_name ? engine->GetTypeInfoByDecl(("array<" + std::string{element_name} + ">").c_str()) : nullptr;
		if (!array_type || !type_info) {
			EngineLog(engine).error("[AngelScript] SetReturnArray: no array type for element type id {}", element_type);
			return;
		}
		CScriptArray* array = CScriptArray::Create(array_type, static_cast<asUINT>(count));
		const auto* bytes = static_cast<const std::byte*>(elements);
		for (size_t i = 0; i < count; ++i) {
			array->SetValue(
				static_cast<asUINT>(i),
				const_cast<std::byte*>(bytes + i * static_cast<size_t>(type_info->size))
			);
		}
		// SetReturnObject takes its own reference to a handle return; drop the one Create gave us.
		gen_->SetReturnObject(array);
		array->Release();
	}

private:
	asIScriptGeneric* gen_;
};

} // namespace

// Backend-neutral ScriptFunctionHandle over an asIScriptFunction*. Created when a script passes a
// function handle to a callback-binding function (see RegisterCallbackFunction); the sink wraps it
// (typically in a std::function on a component or descriptor) and fires it via Invoke(). The
// recorded parameter list drives argument marshalling back into AngelScript at invoke time.
class AngelScriptFunctionHandle final : public ScriptFunctionHandle {
public:
	AngelScriptFunctionHandle(
		asIScriptEngine* engine,
		asIScriptFunction* fn,
		std::vector<ScriptMethodParam> params,
		AngelScriptInstance* owner = nullptr
	) : engine_(engine), fn_(fn), params_(std::move(params)), owner_(owner) {
		// Do NOT call fn_->AddRef() here.
		//
		// AS pre-increments the reference count before passing a handle parameter to any generic
		// function (it transfers ownership to the callee). Calling AddRef() again inflates the count
		// by one too many, leaving the function permanently referenced and preventing clean module
		// discard (AS will warn "external reference preventing deletion" at shutdown).
		//
		// Ownership contract: fn_ arrives with externalRefCount already incremented by AS.
		// We are responsible for exactly one matching Release(), either via InvalidateFn() (for
		// tracked handles, called before DiscardModule) or via the destructor (for handles whose
		// C++ owner drops before the script instance is destroyed).
	}

	~AngelScriptFunctionHandle() override {
		if (owner_) {
			owner_->UntrackHandle(this);
		}
		// Balance the AS-provided AddRef. Skipped when InvalidateFn() already released and nulled fn_.
		if (fn_) {
			fn_->Release();
		}
	}

	// Called by the owning instance before module discard: balances the AS-provided AddRef and
	// nulls fn_ so the destructor skips its release and Invoke becomes a safe no-op.
	void InvalidateFn() {
		owner_ = nullptr;
		if (fn_) {
			fn_->Release();
			fn_ = nullptr;
		}
	}

	void AddRef() override { ++ref_count_; }

	void Release() override {
		if (--ref_count_ <= 0) {
			delete this;
		}
	}

	void Invoke(const void* const* args, const int arg_count) override {
		if (!fn_) return; // handle was invalidated (source module discarded)
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
				EngineLog(engine_).warn(
					"[AngelScript] Unsupported callback arg kind {}",
					static_cast<int>(params_[i].type.kind)
				);
				break;
			}
		}
		// The callback runs as its own script: a handle it creates (e.g. a click handler that binds
		// another handler) belongs to the owner's module, not to whatever called Invoke. A callback
		// that throws is logged only; its script keeps running.
		std::ignore = ExecuteAs(ctx, owner_, "callback '" + std::string{fn_->GetName()} + "'");
		ctx->Unprepare();
		ctx->Release();
	}

private:
	asIScriptEngine* engine_;
	asIScriptFunction* fn_; // null after InvalidateFn(); destructor and Invoke check this
	std::vector<ScriptMethodParam> params_;
	AngelScriptInstance* owner_; // non-owning; cleared by InvalidateFn() or UntrackHandle
	int ref_count_{1};
};

// -------------------------------------------------------------------------
// AngelScriptBackend — core lifecycle and entity/global registration
// (Type registration lives in angelscript_type_registration.cpp)
// -------------------------------------------------------------------------

AngelScriptBackend::~AngelScriptBackend() { Shutdown(); }

bool AngelScriptBackend::Init(flecs::world& world) {
	engine_ = asCreateScriptEngine();
	if (!engine_) {
		ecs::Log(world).error("[AngelScript] asCreateScriptEngine() failed");
		return false;
	}

	world_ = &world;
	engine_->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL);
	engine_->SetUserData(world_, kWorldUserDataSlot);
	engine_->SetUserData(this, kBackendUserDataSlot);
	RegisterStdString(engine_);
	// Default array: scripts can write `T[]` for `array<T>`. Query results (RaycastAll) come back as arrays.
	RegisterScriptArray(engine_, true);
	RegisterEntityType();
	RegisterGlobalFunctions();

	// Register ScriptEntityRef with Flecs meta under the "Entity" name so that callback
	// parameter types can reference it via ScriptValueType::MakeObject(world.component<ScriptEntityRef>())
	// and ScriptTypeName() resolves it to the "Entity" string AngelScript expects. As an opaque type
	// (serialized as its entity) it can also be a member of a reflected value type (RaycastHit.entity).
	world.component<ScriptEntityRef>("Entity")
		.opaque(flecs::Entity)
		.serialize([](const flecs::serializer* s, const ScriptEntityRef* data) {
			const flecs::entity_t id = data->GetEntity().id();
			return s->value(flecs::Entity, &id);
		});

	ecs::Log(world).info("[AngelScript] Engine initialized");
	return true;
}

void AngelScriptBackend::Shutdown() {
	// Run a full GC pass before deleting instances. The GC may hold transient references to
	// script functions (e.g. from the last Execute cycle). Flushing those references first
	// ensures that our InvalidateFn calls in ~AngelScriptInstance are the last external refs,
	// so DiscardModule can complete without "external reference" warnings.
	if (engine_) {
		engine_->GarbageCollect(asGC_FULL_CYCLE);
	}

	spdlog::debug("[AngelScript] Shutdown: {} script instances remaining", instances_.size());
	for (const AngelScriptInstance* inst : instances_) {
		delete inst;
	}
	instances_.clear();

	if (engine_) {
		engine_->ShutDownAndRelease();
		engine_ = nullptr;
	}

	// Clear stable storage after the engine is gone so a future Init() starts from a clean slate.
	global_thunks_.clear();
	callback_bindings_.clear();
	callback_view_props_.clear();
	registered_funcdefs_.clear();
	warned_missing_components_.clear();

	world_ = nullptr;
}

IScriptInstance* AngelScriptBackend::CreateInstanceFromFile(std::string_view path, const flecs::entity entity) {
	const std::ifstream file{std::string{path}};
	if (!file.is_open()) {
		EngineLog(engine_).error("[AngelScript] Cannot open script file '{}'", path);
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

	engine_->RegisterObjectMethod(
		"Entity",
		"string GetString(const string &in, const string &in) const",
		WRAP_MFN(ScriptEntityRef, GetString),
		asCALL_GENERIC
	);
	engine_->RegisterObjectMethod(
		"Entity",
		"void SetString(const string &in, const string &in, const string &in)",
		WRAP_MFN(ScriptEntityRef, SetString),
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
	// Returns a mutable T@ handle into Flecs component storage WITHOUT calling modified.
	// This is the default `GetT()` getter — safe for both reads and silent in-place mutations.
	// Assigning to `const T@` in script incurs no Flecs overhead (no observer wake-up, no
	// change-detection invalidation). When you need Flecs to detect a mutation, use MutT()
	// instead, or call SetT() after you are done modifying.
	//
	// Pointer stability: OnInit, PreTick and Tick run undeferred (the script systems suspend
	// deferral), so an AddT()/SetT() that adds a component moves the entity to another table and
	// leaves handles fetched before it dangling; fetch handles after structural changes. If the
	// component is absent a null handle is returned, with a warning once per (entity, component).
	//
	// **ScriptTraversal fallback**: when the component entity carries a (ScriptTraversal, Rel)
	// pair and the component is not found on the host, the lookup retries on the entity targeted
	// via Rel.

	const auto resolved = ResolveEntityAndComponent(gen);
	if (!resolved.has_value()) {
		gen->SetReturnAddress(nullptr);
		return;
	};
	const auto [host, component_id] = resolved.value();

	void* ptr = host.try_get_mut(component_id);
	if (ptr == nullptr) {
		if (const flecs::entity traversal_rel = host.world().entity(component_id).target<ScriptTraversal>();
			traversal_rel.is_valid()) {
			if (const auto target = host.target(traversal_rel)) {
				if (void* target_ptr = target.try_get_mut(component_id); target_ptr != nullptr) {
					gen->SetReturnAddress(target_ptr);
					return;
				}
			}
		}
	}

	if (ptr == nullptr) {
		WarnMissingComponentOnce(gen, host, component_id, "Get");
	}
	gen->SetReturnAddress(ptr);
}

void AngelScriptBackend::ComponentGetMutRefGeneric(asIScriptGeneric* gen) {
	// Mutable getter registered as `T@ MutT()`. Identical to GetT() but calls modified
	// after the lookup so Flecs OnSet observers and change-detection queries fire. Use this when
	// you will mutate through the returned handle and need Flecs to see the change (e.g. a
	// Transform that drives physics propagation, or a component with an OnSet hook). For pure
	// reads or mutations that don't require observer notification, use GetT().

	const auto resolved = ResolveEntityAndComponent(gen);
	if (!resolved.has_value()) {
		gen->SetReturnAddress(nullptr);
		return;
	};
	const auto [host, component_id] = resolved.value();

	void* ptr = host.try_get_mut(component_id);

	if (ptr == nullptr) {
		if (const flecs::entity traversal_rel = host.world().entity(component_id).target<ScriptTraversal>();
			traversal_rel.is_valid()) {
			if (const auto target = host.target(traversal_rel)) {
				if (void* target_ptr = target.try_get_mut(component_id); target_ptr != nullptr) {
					target.modified(component_id);
					gen->SetReturnAddress(target_ptr);
					return;
				}
			}
		}
	}

	if (ptr == nullptr) {
		WarnMissingComponentOnce(gen, host, component_id, "Mut");
	}
	else {
		host.modified(component_id);
	}
	gen->SetReturnAddress(ptr);
}

void AngelScriptBackend::ComponentGetConstRefGeneric(asIScriptGeneric* gen) {
	// Read-only companion to ComponentGetRefGeneric. Registered as `const T@ GetT() const` on
	// Entity so scripts (or C++ callers with a const Entity) can read a component without
	// triggering modified. Uses try_get (const pointer) — no mutation tracking, no
	// waking of change-detection queries or OnSet observers. Use this whenever the intent is
	// purely to read, e.g. when checking a value inside an OnValidate or a read-heavy loop.

	const auto resolved = ResolveEntityAndComponent(gen);
	if (!resolved.has_value()) {
		gen->SetReturnAddress(nullptr);
		return;
	};
	const auto [host, component_id] = resolved.value();

	const void* ptr = host.try_get(component_id);

	if (ptr == nullptr) {
		if (const flecs::entity traversal_rel = host.world().entity(component_id).target<ScriptTraversal>();
			traversal_rel.is_valid()) {
			if (const auto target = host.target(traversal_rel)) {
				if (const void* target_ptr = target.try_get(component_id); target_ptr != nullptr) {
					gen->SetReturnAddress(const_cast<void*>(target_ptr));
					return;
				}
			}
		}
	}

	if (ptr == nullptr) {
		WarnMissingComponentOnce(gen, host, component_id, "Get");
	}
	gen->SetReturnAddress(const_cast<void*>(ptr));
}

void AngelScriptBackend::WarnMissingComponentOnce(
	asIScriptGeneric* gen,
	const flecs::entity host,
	const uint64_t component_id,
	const char* accessor
) {
	// The handle stays null: a script that dereferences it throws "Null pointer access" and is
	// disabled with that line, while one that checks `is null` (or HasT() first) keeps running.
	auto* backend = static_cast<AngelScriptBackend*>(gen->GetEngine()->GetUserData(kBackendUserDataSlot));
	if (backend == nullptr || !backend->warned_missing_components_.emplace(host.id(), component_id).second) {
		return;
	}
	const char* component_name = host.world().entity(component_id).name().c_str();
	ecs::Log(host.world())
		.warn(
			"[AngelScript] {}{}(): entity '{}' has no {}; returning a null handle (call Add{}() first, or check "
			"Has{}()). Warned once per entity and component.",
			accessor,
			component_name,
			host.name().c_str(),
			component_name,
			component_name,
			component_name
		);
}

void AngelScriptBackend::ComponentAddGeneric(asIScriptGeneric* gen) {
	// Ensures the component exists on the entity (adds zero-initialised if absent), then returns
	// a mutable handle (T@) into the ECS storage. This replaces the old "create local value, call
	// SetT()" pattern: scripts call AddT() once, modify fields via the handle, and the ECS
	// component is immediately updated without a separate write-back step.
	// NOTE: ensure fires OnAdd (if newly added) but NOT OnSet. Call SetT() explicitly
	// when an OnSet observer must trigger (e.g. AudioSource which resolves handles on OnSet).

	const auto resolved = ResolveEntityAndComponent(gen);
	if (!resolved.has_value()) {
		gen->SetReturnAddress(nullptr);
		return;
	};
	const auto [host, component_id] = resolved.value();

	void* ptr = host.ensure(component_id);
	gen->SetReturnAddress(ptr);
}

void AngelScriptBackend::ComponentSetGeneric(asIScriptGeneric* gen) {
	// The method receiver is the Entity; the argument is a T@ handle (ref type).
	// GetArgObject returns the raw pointer for a handle argument (the T* itself).
	const void* value = gen->GetArgObject(0);
	const auto resolved = ResolveEntityAndComponent(gen);
	if (!resolved.has_value() || !value) {
		return; // void method: nothing to return
	}
	const auto [host, component_id] = resolved.value();

	const auto* component_info = GetComponentInfo(host.world(), component_id);
	if (!component_info) {
		ecs::Log(host.world())
			.warn("[AngelScript] ComponentSetGeneric: component_id {} has no type info", component_id);
		return;
	}

	// set_ptr adds the component if the entity doesn't already have it.
	host.set_ptr(component_id, static_cast<size_t>(component_info->size), value);
}

void AngelScriptBackend::ComponentHasGeneric(asIScriptGeneric* gen) {
	// Returns true if the entity (or its script host) has the component.
	const auto resolved = ResolveEntityAndComponent(gen);
	if (!resolved.has_value()) {
		gen->SetReturnByte(0);
		return;
	}
	const auto [host, component_id] = resolved.value();
	gen->SetReturnByte(host.has(component_id) ? 1 : 0);
}

void AngelScriptBackend::SingletonGetGeneric(asIScriptGeneric* gen) {
	const auto component_id = reinterpret_cast<uintptr_t>(gen->GetAuxiliary());
	void* out = gen->GetAddressOfReturnLocation();
	const asIScriptContext* ctx = asGetActiveContext();
	if (!out || component_id == 0 || ctx == nullptr) {
		return;
	}

	const auto* world = static_cast<flecs::world*>(ctx->GetEngine()->GetUserData(kWorldUserDataSlot));
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
	auto* world = static_cast<flecs::world*>(gen->GetEngine()->GetUserData(kWorldUserDataSlot));
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
		ecs::Log(*world_).warn(
			"[AngelScript] Skipping global function '{}' — unresolved parameter/return type",
			sig.name
		);
		return;
	}

	// On re-registration (e.g. scene reload), update the thunk in-place so the AS engine's
	// auxiliary pointer continues to point to the same address with the new behaviour.
	if (const auto it = global_thunks_.find(declaration); it != global_thunks_.end()) {
		*it->second = std::move(thunk);
		return;
	}

	auto thunk_ptr = std::make_unique<GlobalFunctionThunk>(std::move(thunk));
	if (engine_->RegisterGlobalFunction(
			declaration.c_str(),
			asFUNCTION(AngelScriptBackend::GlobalFunctionGeneric),
			asCALL_GENERIC,
			thunk_ptr.get()
		)
		< 0) {
		ecs::Log(*world_).error("[AngelScript] Failed to register global function '{}'", sig.name);
		return;
	}
	global_thunks_[declaration] = std::move(thunk_ptr);
}

void AngelScriptBackend::CallbackFunctionGeneric(asIScriptGeneric* gen) {
	// The binding (sink + callback signature) is carried as the auxiliary; the world comes from
	// engine user data (slot 1). The trailing argument is always the script function handle.
	const auto* binding = static_cast<CallbackBinding*>(gen->GetAuxiliary());
	auto* world = static_cast<flecs::world*>(gen->GetEngine()->GetUserData(kWorldUserDataSlot));
	if (binding == nullptr || !binding->sink || world == nullptr) {
		return;
	}

	auto* as_fn = static_cast<asIScriptFunction*>(gen->GetArgObject(static_cast<asUINT>(binding->callback_arg_index)));
	if (as_fn == nullptr) {
		return; // null callback passed; nothing to bind
	}

	// Associate the handle with the currently executing instance for cleanup on module discard.
	AngelScriptInstance* owner = t_current_instance;

	// The handle holds the backend's single reference for the duration of the call. The sink
	// AddRef()s it if it retains the callback; we drop our reference immediately afterwards.
	auto* handle = new AngelScriptFunctionHandle(gen->GetEngine(), as_fn, binding->callback_params, owner);
	if (owner) owner->TrackHandle(handle);
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
		ecs::Log(*world_).warn("[AngelScript] Callback function '{}' has no funcdef name", desc.name);
		return;
	}

	// Format one parameter as its AngelScript declaration fragment, honoring handle/reference
	// (delegates to the shared helper so all declaration builders stay consistent).
	const auto format_param = [this](const ScriptMethodParam& p) -> std::string { return FormatParam(*world_, p); };

	// Register the callback funcdef (deduplicated by name across bindings that share a signature).
	if (!registered_funcdefs_.contains(fdname)) {
		const std::string fdecl = BuildFuncdefDeclaration(*world_, fdname, desc.callback.params);
		if (fdecl.empty()) {
			ecs::Log(*world_).error("[AngelScript] Unresolved callback param type for funcdef '{}'", fdname);
			return;
		}
		if (engine_->RegisterFuncdef(fdecl.c_str()) < 0) {
			ecs::Log(*world_).error("[AngelScript] Failed to register funcdef '{}'", fdecl);
			return;
		}
		registered_funcdefs_.insert(fdname);
	}

	// Build `void <name>(<fixed params...>, <funcdef>@ <callback name>)`.
	std::string decl = "void " + desc.name + "(";
	for (size_t i = 0; i < desc.fixed_params.size(); ++i) {
		const std::string frag = format_param(desc.fixed_params[i]);
		if (frag.empty()) {
			ecs::Log(*world_).warn(
				"[AngelScript] Skipping callback function '{}' — unresolved parameter type",
				desc.name
			);
			return;
		}
		decl += (i == 0 ? "" : ", ") + frag;
	}
	if (!desc.fixed_params.empty()) {
		decl += ", ";
	}
	decl += fdname + "@ " + (desc.callback.name.empty() ? "callback" : desc.callback.name);
	decl += ")";

	// On re-registration (e.g. scene reload), update the sink in-place.
	if (const auto it = callback_bindings_.find(decl); it != callback_bindings_.end()) {
		it->second->sink = std::move(sink);
		return;
	}

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
		ecs::Log(*world_).error("[AngelScript] Failed to register callback function '{}'", desc.name);
		return;
	}
	callback_bindings_[decl] = std::move(binding);
}

void AngelScriptBackend::CallbackPropertySetGeneric(asIScriptGeneric* gen) {
	// Auxiliary carries the property's sink + callback signature. The receiver is the C++ view
	// object; the single argument is the assigned script function (null when the property is cleared).
	const auto* state = static_cast<CallbackViewProperty*>(gen->GetAuxiliary());
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

	// Associate the handle with the currently executing instance for cleanup on module discard.
	AngelScriptInstance* owner = t_current_instance;

	// The handle holds the backend's single reference for the duration of the call; the sink
	// AddRef()s it if it retains the callback, and we drop our reference immediately afterwards.
	auto* handle = new AngelScriptFunctionHandle(state->engine, as_fn, state->params, owner);
	if (owner) owner->TrackHandle(handle);
	state->sink(object, handle);
	handle->Release();
}

void AngelScriptBackend::CallbackPropertyInvokeGeneric(asIScriptGeneric* gen) {
	// Invoke the stored callback on the receiver view, marshalling the script arguments through
	// the neutral context. Works regardless of whether the callback was registered from script or
	// C++, because both are invoked through the object's std::function.
	const auto* state = static_cast<CallbackViewProperty*>(gen->GetAuxiliary());
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
	// The AS type may have been pre-registered by RegisterComponentType (without the view fields),
	// so use the callback_view_props_ map — not type existence — to detect re-registration.
	const std::string first_key =
		callback_props.empty() ? "" : std::string{type_name} + "::" + callback_props.front().name;
	const bool is_re_registration = !first_key.empty() && callback_view_props_.contains(first_key);

	if (engine_->GetTypeIdByDecl(type_name) < 0) {
		engine_->RegisterObjectType(type_name, 0, asOBJ_REF | asOBJ_NOCOUNT);
	}
	if (!is_re_registration) {
		for (const auto& [decl, offset] : fields) {
			if (engine_->RegisterObjectProperty(type_name, decl.c_str(), offset) < 0) {
				ecs::Log(*world_).error("[AngelScript] Failed to register property '{}' on '{}'", decl, type_name);
			}
		}
	}

	for (auto& prop : callback_props) {
		const std::string fdname =
			prop.funcdef_name.empty() ? std::string{type_name} + "_" + prop.name + "Fn" : prop.funcdef_name;

		// Register the callback funcdef (deduplicated by name; shared across bindings with the
		// same signature — including bindings from a separate RegisterCallbackFunction call).
		if (!registered_funcdefs_.contains(fdname)) {
			const std::string fdecl = BuildFuncdefDeclaration(*world_, fdname, prop.params);
			if (fdecl.empty()) {
				ecs::Log(*world_).error("[AngelScript] Unresolved callback param type for funcdef '{}'", fdname);
				return;
			}
			if (engine_->RegisterFuncdef(fdecl.c_str()) < 0) {
				ecs::Log(*world_).error("[AngelScript] Failed to register funcdef '{}'", fdecl);
				return;
			}
			registered_funcdefs_.insert(fdname);
		}

		const std::string key = std::string{type_name} + "::" + prop.name;

		// On re-registration (e.g. scene reload), update sink/invoke in-place.
		if (is_re_registration) {
			callback_view_props_[key]->sink = std::move(prop.sink);
			callback_view_props_[key]->invoke = std::move(prop.invoke);
			continue;
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
			ecs::Log(*world_).error(
				"[AngelScript] Failed to register callback setter '{}' on '{}'",
				prop.name,
				type_name
			);
			return;
		}

		// Optional invoker method: `view.<Name>(<params>)` fires the stored callback directly,
		// whether it was registered from script or C++ (both go through the object's std::function).
		if (state->invoke) {
			std::string invoke_decl = "void " + prop.name + "(";
			for (size_t i = 0; i < prop.params.size(); ++i) {
				const std::string frag = FormatParam(*world_, prop.params[i]);
				if (frag.empty()) {
					ecs::Log(*world_).error(
						"[AngelScript] Unresolved invoke param type for '{}.{}'",
						type_name,
						prop.name
					);
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
				ecs::Log(*world_).error(
					"[AngelScript] Failed to register callback invoker '{}' on '{}'",
					prop.name,
					type_name
				);
				return;
			}
		}
		callback_view_props_[key] = std::move(state);
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
		EngineLog(engine_).error("[AngelScript] Compile failed for module '{}' (error {})", module_name, result);
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
	// Invalidate all tracked handles before discarding the module so the module has zero
	// application-held external references when DiscardModule is called. AS's GC may still
	// hold a transient ref; a full GC pass in Shutdown() before deleting instances resolves that.
	for (AngelScriptFunctionHandle* handle : tracked_handles_) {
		handle->InvalidateFn();
	}
	tracked_handles_.clear();
	if (engine_ && module_) {
		engine_->DiscardModule(module_->GetName());
		module_ = nullptr;
	}
}

void AngelScriptInstance::TrackHandle(AngelScriptFunctionHandle* handle) { tracked_handles_.push_back(handle); }

void AngelScriptInstance::UntrackHandle(AngelScriptFunctionHandle* handle) { std::erase(tracked_handles_, handle); }

bool AngelScriptInstance::OnInit() { return Call(fn_on_init_); }

bool AngelScriptInstance::Tick(const float dt, const ScriptTickPhase phase) {
	switch (phase) {
	case ScriptTickPhase::Pre: return Call(fn_pre_tick_, dt, true);
	case ScriptTickPhase::On: return Call(fn_on_tick_, dt, true);
	case ScriptTickPhase::Post: return Call(fn_post_tick_, dt, true);
	}
	return true;
}

void AngelScriptInstance::OnDestroy() { std::ignore = Call(fn_on_destroy_); }

std::vector<std::string> AngelScriptInstance::FunctionDeclarations() const {
	std::vector<std::string> declarations;
	if (!module_) {
		return declarations;
	}
	for (asUINT i = 0; i < module_->GetFunctionCount(); ++i) {
		declarations.emplace_back(module_->GetFunctionByIndex(i)->GetDeclaration(false, false, false));
	}
	return declarations;
}

ScriptCallResult
AngelScriptInstance::CallFunction(const std::string_view declaration, const std::span<const ScriptArg> args, std::string* error) {
	asIScriptFunction* fn = module_ ? module_->GetFunctionByDecl(std::string{declaration}.c_str()) : nullptr;
	if (!fn) {
		return ScriptCallResult::NotDefined;
	}
	const auto fail = [this, error, &declaration](const std::string& message) {
		if (error) {
			*error = message;
		}
		EngineLog(engine_).error("[AngelScript] '{}': {}", declaration, message);
		return ScriptCallResult::Failed;
	};
	if (const int prep = ctx_->Prepare(fn); prep < 0) {
		return fail("Prepare failed (error " + std::to_string(prep) + ")");
	}
	for (size_t i = 0; i < args.size(); ++i) {
		const auto index = static_cast<asUINT>(i);
		if (const auto* number = std::get_if<int>(&args[i])) {
			ctx_->SetArgDWord(index, static_cast<asDWORD>(*number));
		}
		else if (const auto* real = std::get_if<float>(&args[i])) {
			ctx_->SetArgFloat(index, *real);
		}
		else {
			ctx_->SetArgObject(index, const_cast<std::string*>(&std::get<std::string>(args[i])));
		}
	}
	int result = 0;
	{
		const CurrentInstanceScope scope{this};
		result = ctx_->Execute();
	}
	ScriptCallResult outcome = ScriptCallResult::Ok;
	if (result == asEXECUTION_EXCEPTION) {
		outcome = fail(std::string{ctx_->GetExceptionString()} + " (line " + std::to_string(ctx_->GetExceptionLineNumber()) + ")");
	}
	else if (result < 0) {
		outcome = fail("Execute failed (error " + std::to_string(result) + ")");
	}
	ctx_->Unprepare();
	return outcome;
}

bool AngelScriptInstance::Call(asIScriptFunction* fn, const float dt, const bool pass_dt) {
	if (!fn) {
		return true;
	}
	if (!ctx_) {
		return false;
	}
	// Re-entry: this instance's script is already running and called C++ that calls one of its
	// entry points again. Its one context can't run two calls, so the inner one is skipped; that
	// isn't the script throwing, so it reports success.
	if (ctx_->GetState() == asEXECUTION_ACTIVE) {
		EngineLog(engine_).warn("[AngelScript] '{}' skipped: its script is already running", fn->GetName());
		return true;
	}

	if (const int prep = ctx_->Prepare(fn); prep < 0) {
		EngineLog(engine_).error("[AngelScript] Prepare failed for '{}' (error {})", fn->GetName(), prep);
		return false;
	}

	// Entity is a value type passed by value — SetArgObject copies it into the context's arg slot.
	ctx_->SetArgObject(0, &entity_ref_);
	if (pass_dt) {
		ctx_->SetArgFloat(1, dt);
	}

	// Run as this instance so callback generics (SetOnClick etc.) can track any script function
	// handles they create back to this module for cleanup on discard.
	const int result = ExecuteAs(ctx_, this, "'" + std::string{fn->GetName()} + "'");
	// Unprepare after each call so the context doesn't retain a reference to fn via
	// m_initialFunction between ticks, avoiding shutdown warnings.
	ctx_->Unprepare();
	return result == asEXECUTION_FINISHED;
}

} // namespace engine::scripting::angelscript
