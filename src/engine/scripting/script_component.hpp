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
	None,          // No error; script is valid
	LoadFailed,    // File read error
	CompileFailed, // Syntax or compile-time error
	RuntimeError,  // The script threw in OnInit or a Tick; it is not called again
};

// Attached as a component on a *child* entity of the host entity.
// Multiple scripts per entity attach by creating multiple child entities, each with one ScriptComponent.
//
//   auto s = world.entity("FooScript").child_of(host);
//   s.set<ScriptComponent>({ .source_path = "scripts/foo.as" });
//
// The script itself is the component, so its `self` is the host (the child's parent): the entity
// whose components it reads and writes. The child only carries the ScriptComponent.
//
// source_path and inline_source are mutually exclusive:
//   - source_path:   backend reads and compiles the file.
//   - inline_source: raw script text for dynamically generated scripts.
//
// instance is filled by the backend's OnSet observer. Tick systems dispatch
// directly through it — no backend involvement in the hot path. error tracks
// failures during compilation or execution so callers can detect issues; a script with an
// error is never called again.
struct ScriptComponent {
	std::string source_path;   // path on disk (empty when using inline source)
	std::string inline_source; // raw script text (empty when using a file path)

	IScriptInstance* instance{nullptr};   // backend-owned; direct dispatch target
	bool initialized{false};              // true after IScriptInstance::OnInit has been called
	ScriptError error{ScriptError::None}; // compilation or runtime failure; None while the script runs
	std::string error_message;            // human-readable error detail
};

// Relationship tag for component traversal in script Get calls.
//
// When component C has the pair (ScriptTraversal, Rel) added to its component entity, the
// scripting backend's GetC() accessor will also try finding C on the entity that `this` targets
// via Rel whenever C is not found directly on the host entity.
//
// Usage (typically in a module constructor):
//   world.component<render::Material>()
//       .add<scripting::ScriptTraversal>(world.component<render::RenderWith>());
//
// This lets scripts call self.GetMaterial() on entities that declare their material via a
// RenderWith relationship, rather than having Material set directly on them.
struct ScriptTraversal {};

} // namespace engine::scripting
