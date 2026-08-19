#pragma once

#include "script_method.hpp"

#include <flecs.h>

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

// Templated construction of a component-method binding from a plain C++ member function
// pointer. The member pointer is a template value parameter, which lets us generate a
// non-capturing thunk that marshals one call through a neutral ScriptCallContext:
//
//   RegisterComponentMethodForScripts<&InputState::WasKeyPressed>(world, "WasKeyPressed");
//
// The class, return type, parameter types (and their ref-ness / const-ness) are all deduced.
// Supported value types map 1:1 to ScriptValueKind: bool, int, float, std::string, and any
// flecs-meta-reflected value type (resolved to its component id via world.component<T>()).
// The backend supplies the ScriptCallContext at call time and drives dispatch through its own
// generic convention, so no native calling convention is required.

namespace engine::scripting::detail {

// Map a bare C++ type to its ScriptValueType. Primary template covers reflected object value
// types; primitives and std::string are specialised below.
template<typename T>
[[nodiscard]] ScriptValueType ToScriptValueType(const flecs::world& world) {
	static_assert(
		!std::is_arithmetic_v<T>,
		"Unsupported arithmetic type in script method parameter/return. Add explicit specialization for this type "
		"(e.g., uint32_t, int64_t, double, size_t, etc.)."
	);
	return ScriptValueType::MakeObject(world.component<T>());
}

template<>
[[nodiscard]] inline ScriptValueType ToScriptValueType<bool>(const flecs::world&) {
	return ScriptValueType::MakeBool();
}
template<>
[[nodiscard]] inline ScriptValueType ToScriptValueType<int>(const flecs::world&) {
	return ScriptValueType::MakeInt();
}
template<>
[[nodiscard]] inline ScriptValueType ToScriptValueType<float>(const flecs::world&) {
	return ScriptValueType::MakeFloat();
}
template<>
[[nodiscard]] inline ScriptValueType ToScriptValueType<std::string>(const flecs::world&) {
	return ScriptValueType::MakeString();
}

// Read a single argument of bare type T from the call context. Primary template covers
// reflected value-type objects (returned by reference into the arg storage, so a `const T&`
// parameter binds without a copy); primitives and std::string are specialised below.
template<typename T>
struct ArgReader {
	static_assert(
		!std::is_arithmetic_v<T>,
		"Unsupported arithmetic type in script method parameter. Add explicit specialization for this type "
		"(e.g., uint32_t, int64_t, double, size_t, etc.)."
	);
	static T& Read(ScriptCallContext& ctx, const int index) { return *static_cast<T*>(ctx.GetArgObject(index)); }
};
template<>
struct ArgReader<bool> {
	static bool Read(ScriptCallContext& ctx, const int index) { return ctx.GetArgBool(index); }
};
template<>
struct ArgReader<int> {
	static int Read(ScriptCallContext& ctx, const int index) { return ctx.GetArgInt(index); }
};
template<>
struct ArgReader<float> {
	static float Read(ScriptCallContext& ctx, const int index) { return ctx.GetArgFloat(index); }
};
template<>
struct ArgReader<std::string> {
	static std::string Read(ScriptCallContext& ctx, const int index) { return ctx.GetArgString(index); }
};

// Write the method's return value of bare type R into the call context. Primary template covers
// reflected value-type objects (placement-constructed into the return buffer); primitives and
// std::string are specialised below.
template<typename R>
struct ReturnWriter {
	static_assert(
		!std::is_arithmetic_v<R>,
		"Unsupported arithmetic type in script method return value. Add explicit specialization for this type "
		"(e.g., uint32_t, int64_t, double, size_t, etc.)."
	);
	static void Write(ScriptCallContext& ctx, R&& value) {
		new (ctx.GetReturnObjectLocation()) std::remove_cvref_t<R>(std::forward<R>(value));
	}
};
template<>
struct ReturnWriter<bool> {
	static void Write(ScriptCallContext& ctx, const bool value) { ctx.SetReturnBool(value); }
};
template<>
struct ReturnWriter<int> {
	static void Write(ScriptCallContext& ctx, const int value) { ctx.SetReturnInt(value); }
};
template<>
struct ReturnWriter<float> {
	static void Write(ScriptCallContext& ctx, const float value) { ctx.SetReturnFloat(value); }
};
template<>
struct ReturnWriter<std::string> {
	static void Write(ScriptCallContext& ctx, const std::string& value) { ctx.SetReturnString(value); }
};

// Shared signature-building logic for both const and non-const member pointers.
template<typename C, typename R, typename... Args>
struct MemberFnCore {
	using Class = C;

	[[nodiscard]] static ScriptMethodSignature
	Signature(const flecs::world& world, std::string name, const bool is_const) {
		ScriptMethodSignature sig;
		sig.name = std::move(name);
		sig.is_const = is_const;
		if constexpr (std::is_void_v<R>) {
			sig.return_type = ScriptValueType::MakeVoid();
		}
		else {
			sig.return_type = ToScriptValueType<std::remove_cvref_t<R>>(world);
		}
		sig.params = {
			ScriptMethodParam{ToScriptValueType<std::remove_cvref_t<Args>>(world), std::is_reference_v<Args>, {}}...
		};
		return sig;
	}

	// Marshal a call to `self->*Fn(args...)` through the context: read each argument by its
	// position, invoke, then write the return value (or nothing for void). The argument packs
	// are read positionally; evaluation order is irrelevant because each read touches a distinct
	// index with no observable side effects.
	template<typename Self, auto Fn, std::size_t... I>
	static void Dispatch(ScriptCallContext& ctx, std::index_sequence<I...>) {
		auto* self = static_cast<Self*>(ctx.GetObject());
		if constexpr (std::is_void_v<R>) {
			(self->*Fn)(ArgReader<std::remove_cvref_t<Args>>::Read(ctx, static_cast<int>(I))...);
		}
		else {
			ReturnWriter<std::remove_cvref_t<R>>::Write(
				ctx,
				(self->*Fn)(ArgReader<std::remove_cvref_t<Args>>::Read(ctx, static_cast<int>(I))...)
			);
		}
	}
};

// Deduce class / return / parameters from a member function pointer type, and expose a thunk
// generator keyed on the member pointer value. The generated thunk marshals through a neutral
// ScriptCallContext, so it is portable across backends and calling conventions.
template<typename T>
struct MemberFnTraits;

template<typename C, typename R, typename... Args>
struct MemberFnTraits<R (C::*)(Args...)> : MemberFnCore<C, R, Args...> {
	static constexpr bool kConst = false;
	template<R (C::*Fn)(Args...)>
	static void Thunk(ScriptCallContext& ctx) {
		MemberFnCore<C, R, Args...>::template Dispatch<C, Fn>(ctx, std::index_sequence_for<Args...>{});
	}
};

template<typename C, typename R, typename... Args>
struct MemberFnTraits<R (C::*)(Args...) const> : MemberFnCore<C, R, Args...> {
	static constexpr bool kConst = true;
	template<R (C::*Fn)(Args...) const>
	static void Thunk(ScriptCallContext& ctx) {
		MemberFnCore<C, R, Args...>::template Dispatch<const C, Fn>(ctx, std::index_sequence_for<Args...>{});
	}
};

} // namespace engine::scripting::detail
