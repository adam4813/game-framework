#pragma once

#include <flecs.h>

#include <string>

namespace engine::scripting {

// ScriptEntityRef — the ECS entity handle exposed inside scripts.
//
// Each scripting backend registers this C++ class as a first-class type in its
// language (e.g. "Entity" in AngelScript). Scripts receive and pass instances of
// this type just like any other object:
//
//   // AngelScript example
//   void OnInit(Entity@ self) {
//       Entity@ other = FindEntity("Paddle");
//       float x = other.GetFloat("Transform", "x");
//       self.RegisterObserver("BallHit", "OnBallHit");
//   }
//
// Component field access requires the component to be registered with Flecs meta
// (.member<T>("name")). See ScriptingModule::RegisterComponentForScripts().
//
// Observer registration is delegated to the backend: the backend holds a factory
// callback (set during Init) that creates a Flecs observer and routes its
// trigger back into the appropriate script function.
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
	void SetFloat(const std::string& component, const std::string& field, float value);

	[[nodiscard]] int GetInt(const std::string& component, const std::string& field) const;
	void SetInt(const std::string& component, const std::string& field, int value);

	[[nodiscard]] bool GetBool(const std::string& component, const std::string& field) const;
	void SetBool(const std::string& component, const std::string& field, bool value);

	// -------------------------------------------------------------------------
	// Observer factory
	//
	// Registers a Flecs observer for event_component (by name) and routes
	// its trigger into callback_fn_name — a function in the calling script
	// module with signature: void Fn(Entity@ emitter).
	//
	// The observer is unregistered when OnEntityDestroy fires for this entity.
	// Actual implementation is provided by the active scripting backend.
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
