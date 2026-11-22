#pragma once

#include <flecs.h>

#include <string>

namespace engine::scripting {

// ScriptEntityRef — the ECS entity handle exposed inside scripts.
//
// Each scripting backend registers this C++ class as a first-class type in its
// language ("Entity" in AngelScript, a value type: two words, copied freely). Scripts
// receive and pass it like any other value; an entry point's `self` is the host entity:
//
//   // AngelScript example
//   void OnInit(Entity self) {
//       Print("attached to " + self.GetName() + " under " + self.GetParent().GetName());
//       ProgressBar@ bar = self.MutProgressBar(); // a handle into ECS storage, marked modified
//       bar.value = 0.5f;
//   }
//
// The typed accessors (Get/Mut/Add/Set/HasT) come from RegisterComponentForScripts() and need
// the component reflected with Flecs meta (.member<T>("name")). The by-name accessors below
// (GetFloat("Comp", "field"), ...) look the component up with world.lookup and reach a
// top-level member only (no "position.x" paths).
class ScriptEntityRef {
public:
	explicit ScriptEntityRef(const flecs::entity entity) : entity_(entity) {}

	// -------------------------------------------------------------------------
	// Identity
	// -------------------------------------------------------------------------

	[[nodiscard]] uint64_t GetId() const;
	[[nodiscard]] std::string GetName() const;

	// -------------------------------------------------------------------------
	// Component presence (lookup by component name via world.lookup)
	// -------------------------------------------------------------------------

	[[nodiscard]] bool HasComponent(const std::string& component_name) const;

	// -------------------------------------------------------------------------
	// Entity navigation
	// -------------------------------------------------------------------------

	// Returns the parent entity (i.e. the host entity the script is attached to).
	[[nodiscard]] ScriptEntityRef GetParent() const;

	// -------------------------------------------------------------------------
	// Meta-based field accessors (require Flecs meta registration).
	// Returns 0 / false if component or field is not found.
	// -------------------------------------------------------------------------

	[[nodiscard]] float GetFloat(const std::string& component, const std::string& field) const;
	void SetFloat(const std::string& component, const std::string& field, float value) const;

	[[nodiscard]] int GetInt(const std::string& component, const std::string& field) const;
	void SetInt(const std::string& component, const std::string& field, int value) const;

	[[nodiscard]] bool GetBool(const std::string& component, const std::string& field) const;
	void SetBool(const std::string& component, const std::string& field, bool value) const;

	[[nodiscard]] std::string GetString(const std::string& component, const std::string& field) const;
	void SetString(const std::string& component, const std::string& field, const std::string& value) const;

	// -------------------------------------------------------------------------
	// Observer registration — NOT IMPLEMENTED
	//
	// Intended to register a Flecs observer for event_component (by name) that calls
	// callback_fn_name in the calling script. No backend implements it: the call only
	// logs a warning. For script callbacks use a callback-binding function
	// (RegisterCallbackFunctionForScripts, e.g. SetOnClick) instead.
	// -------------------------------------------------------------------------

	void RegisterObserver(const std::string& event_component, const std::string& callback_fn_name);

	// -------------------------------------------------------------------------
	// Internal — not for script use
	// -------------------------------------------------------------------------

	[[nodiscard]] flecs::entity GetEntity() const { return entity_; }

private:
	flecs::entity entity_;
};

} // namespace engine::scripting
