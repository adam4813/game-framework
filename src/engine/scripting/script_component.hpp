#pragma once

#include <cstdint>
#include <string>

namespace engine::scripting {

// Maps to the three built-in Flecs phases used for script dispatch:
//   Pre  → flecs::PreUpdate
//   On   → flecs::OnUpdate
//   Post → flecs::PostUpdate
enum class ScriptTickPhase : uint8_t {
	Pre,
	On,
	Post,
};

// Forward declare so ScriptComponent can hold a pointer without pulling in backend headers.
class IScriptInstance;

// Script compilation/execution error state.
enum class ScriptError : uint8_t {
	None,           // No error; script is valid
	LoadFailed,     // File read error
	CompileFailed,  // Syntax or compile-time error
	RuntimeError,   // Initialization or execution error
};

// Attached as a component on a *child* entity of the host entity.
// Multiple scripts per entity attach by creating multiple child entities, each with one ScriptComponent.
//
//   auto s = world.entity("FooScript").child_of(host);
//   s.set<ScriptComponent>({ .source_path = "scripts/foo.as" });
//
// source_path and inline_source are mutually exclusive:
//   - source_path:   backend reads and compiles the file.
//   - inline_source: raw script text for dynamically generated scripts.
//
// instance is filled by the backend's OnSet observer. Tick systems dispatch
// directly through it — no backend involvement in the hot path. error tracks
// failures during compilation or initialization so callers can detect issues.
struct ScriptComponent {
	std::string source_path;   // path on disk (empty when using inline source)
	std::string inline_source; // raw script text (empty when using a file path)

	IScriptInstance* instance{nullptr}; // backend-owned; direct dispatch target
	bool initialized{false};            // true after IScriptInstance::OnInit has been called
	ScriptError error{ScriptError::None}; // tracks compilation/init failures
	std::string error_message;          // human-readable error detail
};

// Tag placed on the parent ("host") entity when at least one script child exists.
struct ScriptHost {};

} // namespace engine::scripting
