#pragma once

// Internal helpers shared by angelscript_backend.cpp and angelscript_type_registration.cpp.
// Not part of the public scripting API.

#include "engine/scripting/script_method.hpp"

#include <angelscript.h>
#include <flecs.h>

#include <string>
#include <vector>

namespace engine::scripting::angelscript {

// Resolve the AngelScript type name for a value slot.
// Returns an empty string when the object type has no registered name.
inline std::string ScriptTypeName(const flecs::world& world, const ScriptValueType& type) {
	switch (type.kind) {
	case ScriptValueKind::Void: return "void";
	case ScriptValueKind::Bool: return "bool";
	case ScriptValueKind::Int: return "int";
	case ScriptValueKind::Float: return "float";
	case ScriptValueKind::String: return "string";
	case ScriptValueKind::Object:
	{
		const char* name = world.entity(type.object_type).name();
		if (!name || *name == '\0') {
			return {};
		}
		return type.is_handle ? std::string{name} + "@" : std::string{name};
	}
	}
	return {};
}

// Format a single parameter as its AngelScript declaration fragment, honoring handle/reference:
//   handle → "T@";  by-reference → "const T &in";  by-value → "T".
// Appends " name" when the param is named and include_name is true. Returns empty if unresolved.
inline std::string FormatParam(const flecs::world& world, const ScriptMethodParam& param, const bool include_name = true) {
	const std::string type_name = ScriptTypeName(world, param.type);
	if (type_name.empty()) {
		return {};
	}
	std::string frag;
	if (param.type.is_handle) {
		frag = type_name;
	}
	else {
		frag = param.by_reference ? "const " + type_name + " &in" : type_name;
	}
	if (include_name && !param.name.empty()) {
		frag += " " + param.name;
	}
	return frag;
}

// Format a parameter list as comma-separated AngelScript declarations.
// Returns empty if any parameter type is unresolved.
inline std::string
FormatParamList(const flecs::world& world, const std::vector<ScriptMethodParam>& params) {
	std::string result;
	for (size_t i = 0; i < params.size(); ++i) {
		const std::string frag = FormatParam(world, params[i]);
		if (frag.empty()) {
			return {};
		}
		if (i != 0) {
			result += ", ";
		}
		result += frag;
	}
	return result;
}

// Build a callback funcdef declaration "void <funcdef_name>(<params...>)".
// Returns empty if any parameter type is unresolved.
inline std::string
BuildFuncdefDeclaration(const flecs::world& world, const std::string& funcdef_name, const std::vector<ScriptMethodParam>& params) {
	const std::string param_list = FormatParamList(world, params);
	if (param_list.empty() && !params.empty()) {
		return {};
	}
	return "void " + funcdef_name + "(" + param_list + ")";
}

// Build an AngelScript method declaration from a neutral signature.
// e.g. bool-returning const method taking int → "bool WasKeyPressed(int) const"
// A reference parameter becomes `const T &in`.
// Returns an empty string if any referenced type is unresolved.
inline std::string BuildMethodDeclaration(const flecs::world& world, const ScriptMethodSignature& sig) {
	const std::string return_name = ScriptTypeName(world, sig.return_type);
	if (return_name.empty()) {
		return {};
	}

	const std::string param_list = FormatParamList(world, sig.params);
	if (param_list.empty() && !sig.params.empty()) {
		return {};
	}

	std::string decl;
	decl.reserve(return_name.size() + 1 + sig.name.size() + 2 + param_list.size() + 1);
	decl += return_name;
	decl += " ";
	decl += sig.name;
	decl += "(";
	decl += param_list;
	decl += ")";
	if (sig.is_const) {
		decl += " const";
	}
	return decl;
}

} // namespace engine::scripting::angelscript
