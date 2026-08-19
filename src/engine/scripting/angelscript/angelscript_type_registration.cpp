#include "angelscript_backend.hpp"
#include "angelscript_helpers.hpp"

#include <spdlog/spdlog.h>

#include <string>

namespace engine::scripting::angelscript {

// -------------------------------------------------------------------------
// Helpers for reflected POD type registration (local to this TU)
// -------------------------------------------------------------------------

namespace {

const char*
ResolveMemberTypeName(const asIScriptEngine* engine, const flecs::world& world, const ecs_entity_t member_type_id) {
	if (member_type_id == ecs_id(ecs_f32_t)) return "float";
	if (member_type_id == ecs_id(ecs_i32_t)) return "int";
	if (member_type_id == ecs_id(ecs_u32_t)) return "uint";
	if (member_type_id == ecs_id(ecs_bool_t)) return "bool";
	if (member_type_id == ecs_id(ecs_u8_t) || member_type_id == ecs_id(ecs_byte_t)) return "uint8";

	const char* type_name = world.entity(member_type_id).name();
	if (!type_name || *type_name == '\0') return nullptr;

	return engine->GetTypeIdByDecl(type_name) >= 0 ? type_name : nullptr;
}

// Register each reflected (flecs::Member) child of `type_entity` as an AngelScript object property on
// the already-registered `type_name`. Shared by the component and value-type registration paths so
// member-property registration lives in one place. Unsupported member types are skipped with a warning.
void RegisterReflectedMembers(
	asIScriptEngine* engine,
	const flecs::world& world,
	const char* type_name,
	const flecs::entity type_entity
) {
	type_entity.children([&](const flecs::entity member_entity) {
		const auto* member = member_entity.try_get<flecs::Member>();
		if (!member) return;
		const char* member_name = member_entity.name();
		if (!member_name || *member_name == '\0') return;
		const char* member_type_name = ResolveMemberTypeName(engine, world, member->type);
		if (!member_type_name) {
			spdlog::warn("[AngelScript] Skipping unsupported member '{}.{}'", type_name, member_name);
			return;
		}
		engine->RegisterObjectProperty(
			type_name,
			(std::string{member_type_name} + " " + member_name).c_str(),
			member->offset
		);
	});
}

// Default constructor for reflected POD value types. The flecs type-info pointer is passed
// through the behaviour's auxiliary so the C++ constructor (or zero-fill) is invoked.
void ReflectedValueConstruct(asIScriptGeneric* gen) {
	void* self = gen->GetObject();
	const auto* type_info = static_cast<const ecs_type_info_t*>(gen->GetAuxiliary());
	if (!self || !type_info) return;
	if (type_info->hooks.ctor != nullptr) {
		type_info->hooks.ctor(self, 1, type_info);
	}
	else {
		std::memset(self, 0, static_cast<size_t>(type_info->size));
	}
}

// Layout classification for generating AngelScript ABI hints (ALLINTS/ALLFLOATS/ALIGN8).
struct TypeLayout {
	bool any{false};
	bool all_ints{true};
	bool all_floats{true};
	bool align8{false};
};

void ClassifyReflectedLayout(const flecs::world& world, const ecs_entity_t type_id, TypeLayout& out) {
	if (type_id == ecs_id(ecs_f32_t)) {
		out.any = true;
		out.all_ints = false;
		return;
	}
	if (type_id == ecs_id(ecs_f64_t)) {
		out.any = true;
		out.all_ints = false;
		out.align8 = true;
		return;
	}
	if (type_id == ecs_id(ecs_i64_t)
		|| type_id == ecs_id(ecs_u64_t)
		|| type_id == ecs_id(ecs_uptr_t)
		|| type_id == ecs_id(ecs_iptr_t)) {
		out.any = true;
		out.all_floats = false;
		out.align8 = true;
		return;
	}
	if (type_id == ecs_id(ecs_bool_t)
		|| type_id == ecs_id(ecs_char_t)
		|| type_id == ecs_id(ecs_byte_t)
		|| type_id == ecs_id(ecs_u8_t)
		|| type_id == ecs_id(ecs_u16_t)
		|| type_id == ecs_id(ecs_u32_t)
		|| type_id == ecs_id(ecs_i8_t)
		|| type_id == ecs_id(ecs_i16_t)
		|| type_id == ecs_id(ecs_i32_t)) {
		out.any = true;
		out.all_floats = false;
		return;
	}
	world.entity(type_id).children([&](const flecs::entity member_entity) {
		if (const auto* member = member_entity.try_get<flecs::Member>()) {
			ClassifyReflectedLayout(world, member->type, out);
		}
	});
}

asQWORD ReflectedLayoutFlags(const flecs::world& world, const ecs_entity_t type_id) {
	TypeLayout layout;
	ClassifyReflectedLayout(world, type_id, layout);
	if (!layout.any) return 0;
	asQWORD flags = asOBJ_APP_CLASS;
	if (layout.all_ints) flags |= asOBJ_APP_CLASS_ALLINTS;
	else if (layout.all_floats) flags |= asOBJ_APP_CLASS_ALLFLOATS;
	if (layout.align8) flags |= asOBJ_APP_CLASS_ALIGN8;
	return flags;
}

} // namespace

// -------------------------------------------------------------------------
// Type registration methods
// -------------------------------------------------------------------------

void AngelScriptBackend::RegisterValueType(const flecs::entity type_entity) { RegisterValueTypeFromMeta(type_entity); }

void AngelScriptBackend::RegisterComponentType(const flecs::entity component_entity) {
	const char* component_name = component_entity.name();
	if (!component_name || *component_name == '\0') return;

	const asITypeInfo* entity_type = engine_->GetTypeInfoByName("Entity");
	if (!entity_type) {
		spdlog::error("[AngelScript] Entity type not registered before component '{}'", component_name);
		return;
	}

	const std::string getter_sig = std::string{component_name} + "@ Get" + component_name + "()";
	if (entity_type->GetMethodByDecl(getter_sig.c_str()) != nullptr) return; // already registered

	if (engine_->GetTypeIdByDecl(component_name) < 0) {
		engine_->RegisterObjectType(component_name, 0, asOBJ_REF | asOBJ_NOCOUNT);

		const flecs::world world = component_entity.world();
		RegisterReflectedMembers(engine_, world, component_name, component_entity);
	}

	const std::string adder_sig = std::string{component_name} + "@ Add" + component_name + "()";
	const std::string setter_sig = "void Set" + std::string{component_name} + "(" + component_name + "@ v)";
	const auto auxiliary = reinterpret_cast<void*>(component_entity.id());
	engine_->RegisterObjectMethod(
		"Entity",
		getter_sig.c_str(),
		asFUNCTION(ComponentGetRefGeneric),
		asCALL_GENERIC,
		auxiliary
	);
	engine_
		->RegisterObjectMethod("Entity", adder_sig.c_str(), asFUNCTION(ComponentAddGeneric), asCALL_GENERIC, auxiliary);
	engine_->RegisterObjectMethod(
		"Entity",
		setter_sig.c_str(),
		asFUNCTION(ComponentSetGeneric),
		asCALL_GENERIC,
		auxiliary
	);
}

void AngelScriptBackend::RegisterSingletonType(const flecs::entity component_entity) {
	RegisterValueTypeFromMeta(component_entity);

	const char* component_name = component_entity.name();
	if (!component_name || *component_name == '\0') return;

	const std::string getter_sig = std::string{component_name} + " Get" + component_name + "()";
	if (engine_->GetGlobalFunctionByDecl(getter_sig.c_str()) != nullptr) return;

	engine_->RegisterGlobalFunction(
		getter_sig.c_str(),
		asFUNCTION(AngelScriptBackend::SingletonGetGeneric),
		asCALL_GENERIC,
		reinterpret_cast<void*>(component_entity.id())
	);
}

void AngelScriptBackend::RegisterComponentMethod(
	const flecs::entity component_entity,
	const ScriptMethodSignature& sig,
	const ScriptGenericThunk thunk
) {
	if (!engine_ || !component_entity.is_valid() || !thunk) return;

	const char* type_name = component_entity.name();
	if (!type_name || *type_name == '\0') {
		spdlog::warn("[AngelScript] Cannot register method on unnamed component {}", component_entity.id());
		return;
	}

	if (engine_->GetTypeIdByDecl(type_name) < 0) {
		RegisterValueTypeFromMeta(component_entity);
		if (engine_->GetTypeIdByDecl(type_name) < 0) {
			spdlog::warn("[AngelScript] Cannot register method '{}' on unregistered type '{}'", sig.name, type_name);
			return;
		}
	}

	const std::string declaration = BuildMethodDeclaration(component_entity.world(), sig);
	if (declaration.empty()) {
		spdlog::warn("[AngelScript] Skipping method '{}::{}' — unresolved parameter/return type", type_name, sig.name);
		return;
	}
	if (engine_->RegisterObjectMethod(
			type_name,
			declaration.c_str(),
			asFUNCTION(ComponentMethodGeneric),
			asCALL_GENERIC,
			reinterpret_cast<void*>(thunk)
		)
		< 0) {
		spdlog::error("[AngelScript] Failed to register method '{}' on '{}'", sig.name, type_name);
	}
}

void AngelScriptBackend::RegisterValueTypeFromMeta(const flecs::entity type_entity) const {
	if (!engine_ || !type_entity.is_valid()) return;

	const char* type_name = type_entity.name();
	if (!type_name || *type_name == '\0') {
		spdlog::warn("[AngelScript] Skipping unnamed reflected type {}", type_entity.id());
		return;
	}
	if (engine_->GetTypeIdByDecl(type_name) >= 0) return; // already registered

	const flecs::world world = type_entity.world();
	const ecs_type_info_t* type_info = ecs_get_type_info(world, type_entity.id());
	if (!type_info) {
		spdlog::warn("[AngelScript] Reflected type '{}' has no type info", type_name);
		return;
	}

	const ecs_type_hooks_t& hooks = type_info->hooks;
	const bool trivially_copyable = !hooks.copy && !hooks.copy_ctor && !hooks.move && !hooks.move_ctor && !hooks.dtor;
	if (!trivially_copyable) {
		spdlog::warn("[AngelScript] Type '{}' is not trivially copyable and cannot be exposed to scripts", type_name);
		return;
	}

	const asQWORD layout_flags = ReflectedLayoutFlags(world, type_entity.id());
	engine_->RegisterObjectType(type_name, type_info->size, asOBJ_VALUE | asOBJ_POD | layout_flags);
	if (engine_->RegisterObjectBehaviour(
			type_name,
			asBEHAVE_CONSTRUCT,
			"void f()",
			asFUNCTION(ReflectedValueConstruct),
			asCALL_GENERIC,
			const_cast<ecs_type_info_t*>(type_info)
		)
		< 0) {
		spdlog::error("[AngelScript] Failed to register default constructor for '{}'", type_name);
	}

	RegisterReflectedMembers(engine_, world, type_name, type_entity);

	spdlog::debug("[AngelScript] Registered reflected type '{}' ({} bytes)", type_name, type_info->size);
}

void AngelScriptBackend::RegisterShutdownCallback(const std::function<void(flecs::world&)>& callback) {
	shutdown_callbacks_.push_back(callback);
}

void AngelScriptBackend::RegisterObjectConstructor(
	const flecs::entity type_entity,
	const std::string_view params_signature,
	const ScriptGenericThunk ctor_thunk,
	const ScriptGenericThunk dtor_thunk
) {
	if (!engine_ || !type_entity.is_valid() || !ctor_thunk) return;

	const char* type_name = type_entity.name();
	if (!type_name || engine_->GetTypeIdByDecl(type_name) < 0) {
		spdlog::warn("[AngelScript] RegisterObjectConstructor: type '{}' not registered", type_name ? type_name : "?");
		return;
	}

	const std::string ctor_sig = "void f(" + std::string(params_signature) + ")";
	if (engine_->RegisterObjectBehaviour(
			type_name,
			asBEHAVE_CONSTRUCT,
			ctor_sig.c_str(),
			asFUNCTION(ComponentMethodGeneric),
			asCALL_GENERIC,
			reinterpret_cast<void*>(ctor_thunk)
		)
		< 0) {
		spdlog::error("[AngelScript] Failed to register constructor '{}::{}'", type_name, ctor_sig);
	}

	if (dtor_thunk) {
		if (engine_->RegisterObjectBehaviour(
				type_name,
				asBEHAVE_DESTRUCT,
				"void f()",
				asFUNCTION(ComponentMethodGeneric),
				asCALL_GENERIC,
				reinterpret_cast<void*>(dtor_thunk)
			)
			< 0) {
			spdlog::error("[AngelScript] Failed to register destructor for '{}'", type_name);
		}
	}
	else {
		const int type_id = engine_->GetTypeIdByDecl(type_name);
		if (const asITypeInfo* ti = engine_->GetTypeInfoById(type_id); ti && !(ti->GetFlags() & asOBJ_POD)) {
			spdlog::warn(
				"[AngelScript] '{}' is non-POD but no dtor thunk provided — resource leak possible",
				type_name
			);
		}
	}
}

void AngelScriptBackend::RegisterGlobalConstants(std::vector<ScriptConstant> constants) {
	for (const ScriptConstant& c : constants) {
		std::string decl;
		switch (c.type.kind) {
		case ScriptValueKind::Int: decl = "const int " + c.name + " = " + c.value_str + ";"; break;
		case ScriptValueKind::Float: decl = "const float " + c.name + " = " + c.value_str + "f;"; break;
		case ScriptValueKind::Bool: decl = "const bool " + c.name + " = " + c.value_str + ";"; break;
		case ScriptValueKind::String: decl = "const string " + c.name + " = \"" + c.value_str + "\";"; break;
		case ScriptValueKind::Object:
			if (world_ && c.type.object_type != 0) {
				const char* type_name = world_->entity(c.type.object_type).name();
				if (type_name && *type_name != '\0') {
					decl = "const " + std::string{type_name} + " " + c.name + " = " + c.value_str + ";";
				}
			}
			if (decl.empty()) {
				spdlog::warn("[AngelScript] RegisterGlobalConstants: could not resolve Object type for '{}'", c.name);
				continue;
			}
			break;
		default: spdlog::warn("[AngelScript] RegisterGlobalConstants: unsupported kind for '{}'", c.name); continue;
		}
		constants_preamble_ += decl + "\n";
	}
}

} // namespace engine::scripting::angelscript
