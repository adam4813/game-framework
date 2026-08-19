#pragma once

#include <flecs.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace engine::scripting {

// Backend-agnostic description of a component method for scripting.
//
// Flecs meta reflection only models data members, not behaviour. To expose "normal" methods
// (no statics, overloads, or operators) on a reflected component value type, each method is
// bound through a compile-time-generated thunk that marshals arguments via a neutral
// ScriptCallContext (see below). The backend receives a neutral signature plus that thunk and
// wires it into its own generic dispatch (AngelScript: a single asCALL_GENERIC function that
// wraps asIScriptGeneric; a future Lua backend would wrap lua_State). Delegating to the backend
// this way keeps the binding portable — native calling conventions are unavailable on some
// targets (e.g. WebAssembly, where AngelScript only supports asCALL_GENERIC).
//
// See script_method_factory.hpp for the templated `RegisterComponentMethodForScripts<&C::M>()`
// entry point that deduces all of this from a member function pointer.

// The set of value kinds a method signature can reference.
//   Object — a flecs-meta-reflected value type (identified by its component/type id).
//   String — std::string.
enum class ScriptValueKind : uint8_t {
	Void,
	Bool,
	Int,   // 32-bit signed integer
	Float, // 32-bit float
	String,
	Object,
};

// A single type slot in a method signature (return type or a parameter type).
struct ScriptValueType {
	ScriptValueKind kind{ScriptValueKind::Void};
	flecs::entity_t object_type{0}; // flecs component/type id, only used when kind == Object
	bool is_handle{false};          // Object only: exposed as a reference handle (`T@`) rather than a value

	[[nodiscard]] static ScriptValueType MakeVoid() { return {ScriptValueKind::Void, 0}; }
	[[nodiscard]] static ScriptValueType MakeBool() { return {ScriptValueKind::Bool, 0}; }
	[[nodiscard]] static ScriptValueType MakeInt() { return {ScriptValueKind::Int, 0}; }
	[[nodiscard]] static ScriptValueType MakeFloat() { return {ScriptValueKind::Float, 0}; }
	[[nodiscard]] static ScriptValueType MakeString() { return {ScriptValueKind::String, 0}; }
	[[nodiscard]] static ScriptValueType MakeObject(const flecs::entity_t type_id) {
		return {ScriptValueKind::Object, type_id};
	}
	// A reference-type handle (`T@`) — used to return/pass a non-owning view into C++ storage.
	[[nodiscard]] static ScriptValueType MakeObjectHandle(const flecs::entity_t type_id) {
		return {ScriptValueKind::Object, type_id, true};
	}
};

// A parameter in a method signature. by_reference mirrors the C++ parameter: a reference
// parameter (e.g. `const vec3&`) is exposed to scripts as `const T &in`, a by-value parameter
// as `T`. The distinction matters because the native ABI must agree with the generated thunk.
struct ScriptMethodParam {
	ScriptValueType type;
	bool by_reference{false};
	std::string name; // optional; only for readability of the generated declaration
};

// Neutral per-call marshalling frame. A backend implements this over its own generic-call
// context (AngelScript: asIScriptGeneric; Lua: lua_State) and passes it to a generated thunk,
// which reads arguments and writes the return value through these methods without knowing the
// backend. Strings get dedicated accessors so each backend can convert to/from its own string
// representation. Object accessors deal in raw storage pointers: GetArgObject returns the
// argument's address (a reflected value type*), and GetReturnObjectLocation returns the buffer
// where an object return value must be constructed in place.
class ScriptCallContext {
public:
	virtual ~ScriptCallContext() = default;

	// The method receiver ("self") the method was called on.
	[[nodiscard]] virtual void* GetObject() const = 0;

	// Argument access by zero-based index.
	[[nodiscard]] virtual bool GetArgBool(int index) const = 0;
	[[nodiscard]] virtual int GetArgInt(int index) const = 0;
	[[nodiscard]] virtual float GetArgFloat(int index) const = 0;
	[[nodiscard]] virtual std::string GetArgString(int index) const = 0;
	[[nodiscard]] virtual void* GetArgObject(int index) const = 0;

	// Return-value writers for primitive and string kinds.
	virtual void SetReturnBool(bool value) = 0;
	virtual void SetReturnInt(int value) = 0;
	virtual void SetReturnFloat(float value) = 0;
	virtual void SetReturnString(const std::string& value) = 0;

	// Buffer for an object return value; the thunk placement-constructs the result here.
	[[nodiscard]] virtual void* GetReturnObjectLocation() = 0;

	// Return a non-owning reference-type handle (`T@`) — e.g. a view into C++ storage the script
	// then mutates in place. The pointed-to object must outlive the script's use of the handle.
	virtual void SetReturnObjectHandle(void* ptr) = 0;
};

// A compile-time-generated thunk that marshals one component method through a ScriptCallContext.
// The backend invokes it from its generic dispatcher; it is never called through a native ABI.
using ScriptGenericThunk = void (*)(ScriptCallContext& ctx);

// Full description of one component method to expose to scripts.
struct ScriptMethodSignature {
	std::string name;                      // method name as seen in scripts
	ScriptValueType return_type;           // ScriptValueType::MakeVoid() for no return
	std::vector<ScriptMethodParam> params; // ordered parameter list
	bool is_const{false};                  // true when the C++ method is const-qualified
};

// Thunk for a global (free) script function. The backend creates a ScriptCallContext for the
// active call frame and passes the world alongside it, so thunks can access world singletons
// or platform pointers as needed (the common case: capture the platform pointer directly in the
// lambda to avoid re-fetching it on every call).
using GlobalFunctionThunk = std::function<void(ScriptCallContext&, flecs::world&)>;

// A language-agnostic named constant to inject into every script's global namespace.
// The value is stored as its textual representation so backends can emit it using their own
// syntax (e.g. AngelScript: "const int Key_Space = 32;", Lua: "Key_Space = 32").
// Only the primitive kinds (Int, Float, Bool, String) are supported.
struct ScriptConstant {
	std::string name;
	ScriptValueType type;
	std::string value_str; // textual representation of the value, e.g. "32", "3.14", "true"
};

// -------------------------------------------------------------------------
// Callback binding support
// -------------------------------------------------------------------------

// Backend-agnostic callable wrapping a script function. Created by the backend when a script
// passes a function handle to a callback-binding function (see ScriptCallbackFunctionDesc), and
// handed to that function's C++ sink. Lifetime is reference-counted (initial count is 1 on
// construction); the backend releases its own reference after the sink returns, so a sink that
// wants to retain the callback must AddRef() and Release() it later (e.g. from a std::function
// stored on a component or descriptor).
//
// Invoke() receives one void* per declared parameter, each pointing to the C++ value:
//   Entity/Object → pointer to the C++ value (e.g. ScriptEntityRef*)
//   int            → pointer to int
//   float          → pointer to float
//   bool           → pointer to bool
class ScriptFunctionHandle {
public:
	virtual ~ScriptFunctionHandle() = default;
	virtual void AddRef() = 0;
	virtual void Release() = 0;
	virtual void Invoke(const void* const* args, int arg_count) = 0;
};

// The trailing script-callback (function handle) parameter of a callback-binding function.
// The backend registers a funcdef for `params` and appends a handle parameter of that funcdef
// type to the function's declaration. `params` should be entity-first: script functions have no
// implicit receiver, so the subject entity is passed explicitly as the first parameter.
struct ScriptCallbackParam {
	std::string funcdef_name;              // funcdef type name (e.g. "TileEnterCallback")
	std::vector<ScriptMethodParam> params; // callback signature (entity-first)
	std::string name;                      // parameter name in the declaration (optional)
};

// A free function that binds a script callback to a C++ sink. It is declared to scripts as
// `void <name>(<fixed_params...>, <callback funcdef>@ <callback.name>)`, so the last argument is
// always a function handle. When called, the backend marshals the fixed arguments into a
// ScriptCallContext and hands the sink a ScriptFunctionHandle wrapping the passed function.
//
// This is the primitive behind callback-as-argument bindings (e.g. `SetOnClick(button, @fn)`):
// no per-type value machinery — the sink decides how to retain and invoke the callback (typically
// by wrapping it in a std::function stored on a component).
struct ScriptCallbackFunctionDesc {
	std::string name;                            // function name as seen in scripts
	std::vector<ScriptMethodParam> fixed_params; // leading value/object parameters (entity-first)
	ScriptCallbackParam callback;                // the trailing function-handle parameter
};

// Sink invoked when a callback-binding function is called from a script. Read the fixed
// arguments from `ctx` (indices match `fixed_params`); `handle` wraps the passed script function
// (AddRef it to retain beyond the call). `world` is provided for singleton/entity access.
using ScriptCallbackSink =
	std::function<void(ScriptCallContext& ctx, flecs::world& world, ScriptFunctionHandle* handle)>;

// A plain (non-callback) member field of a script-registered type.
// decl:   backend type + name declaration, e.g. "bool walkable" or "float r".
// offset: byte offset of the field within the C++ struct.
struct ScriptTypeField {
	std::string decl;
	int offset{0};
};

// One assignable callback slot on a script reference-view type (see RegisterCallbackViewType).
// Exposed to scripts as two methods:
//   * a setter `view.Set<name>(@fn)` — the backend wraps the script function in a
//     ScriptFunctionHandle and calls `sink(object, handle)`; a null handle clears it. The sink
//     typically stores the callback as a std::function on the object (AddRef to retain).
//   * an invoker `view.<name>(<params>)` — the backend calls `invoke(object, ctx)` so scripts can
//     fire the stored callback directly, whether it was registered from script or C++ (both are
//     invoked through the object's std::function). Omit `invoke` to expose only the setter.
// `params` is the callback signature (entity-first), shared by the funcdef and the invoker.
// AngelScript property-assignment syntax (`view.name = @fn`) is not used: property setters coerce
// their value parameter to `const T &in`, which is invalid for funcdef handles.
struct ScriptCallbackProperty {
	std::string name;
	std::string funcdef_name;
	std::vector<ScriptMethodParam> params;
	std::function<void(void* object, ScriptFunctionHandle* handle)> sink;
	std::function<void(void* object, ScriptCallContext& ctx)> invoke;
};

} // namespace engine::scripting
