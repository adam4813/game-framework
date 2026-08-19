#include "script_context.hpp"

#include <flecs.h>

#include <spdlog/spdlog.h>

namespace engine::scripting {

uint64_t ScriptEntityRef::GetId() const { return entity_.id(); }

std::string ScriptEntityRef::GetName() const {
	const char* name = entity_.name();
	return name ? name : "";
}

bool ScriptEntityRef::HasComponent(const std::string& component_name) const {
	const flecs::entity comp = entity_.world().lookup(component_name.c_str());
	if (!comp.is_valid()) {
		return false;
	}
	return entity_.has(comp.id());
}

ScriptEntityRef ScriptEntityRef::GetParent() const { return ScriptEntityRef(entity_.parent()); }

// -------------------------------------------------------------------------
// Helpers — resolve component pointer via Flecs meta
// -------------------------------------------------------------------------

namespace {

// Returns {comp_id, ptr} if the component exists on entity and has meta. Nullptr otherwise.
std::pair<flecs::id_t, void*> ResolveMutableComponent(const flecs::entity entity, const std::string& component_name) {
	const flecs::entity comp = entity.world().lookup(component_name.c_str());
	if (!comp.is_valid()) {
		spdlog::warn("[Script] Unknown component '{}'", component_name);
		return {0, nullptr};
	}

	void* ptr = entity.try_get_mut(comp.id());
	if (!ptr) {
		spdlog::warn("[Script] Entity '{}' does not have component '{}'", entity.name(), component_name);
		return {0, nullptr};
	}
	return {comp.id(), ptr};
}

std::pair<flecs::id_t, const void*>
ResolveConstComponent(const flecs::entity entity, const std::string& component_name) {
	const flecs::entity comp = entity.world().lookup(component_name.c_str());
	if (!comp.is_valid()) {
		spdlog::warn("[Script] Unknown component '{}'", component_name);
		return {0, nullptr};
	}

	const void* ptr = entity.try_get(comp.id());
	if (!ptr) {
		return {0, nullptr};
	}
	return {comp.id(), ptr};
}

} // namespace

// -------------------------------------------------------------------------
// Float
// -------------------------------------------------------------------------

float ScriptEntityRef::GetFloat(const std::string& component, const std::string& field) const {
	auto [comp_id, ptr] = ResolveConstComponent(entity_, component);
	if (!ptr) {
		return 0.0f;
	}

	ecs_meta_cursor_t cur = ecs_meta_cursor(entity_.world(), comp_id, const_cast<void*>(ptr));
	if (ecs_meta_member(&cur, field.c_str()) != 0) {
		spdlog::warn("[Script] Unknown field '{}.{}'", component, field);
		return 0.0f;
	}
	return ecs_meta_get_float(&cur);
}

void ScriptEntityRef::SetFloat(const std::string& component, const std::string& field, const float value) {
	auto [comp_id, ptr] = ResolveMutableComponent(entity_, component);
	if (!ptr) {
		return;
	}

	ecs_meta_cursor_t cur = ecs_meta_cursor(entity_.world(), comp_id, ptr);
	if (ecs_meta_member(&cur, field.c_str()) != 0) {
		spdlog::warn("[Script] Unknown field '{}.{}'", component, field);
		return;
	}
	ecs_meta_set_float(&cur, value);
}

// -------------------------------------------------------------------------
// Int
// -------------------------------------------------------------------------

int ScriptEntityRef::GetInt(const std::string& component, const std::string& field) const {
	auto [comp_id, ptr] = ResolveConstComponent(entity_, component);
	if (!ptr) {
		return 0;
	}

	ecs_meta_cursor_t cur = ecs_meta_cursor(entity_.world(), comp_id, const_cast<void*>(ptr));
	if (ecs_meta_member(&cur, field.c_str()) != 0) {
		spdlog::warn("[Script] Unknown field '{}.{}'", component, field);
		return 0;
	}
	return static_cast<int>(ecs_meta_get_int(&cur));
}

void ScriptEntityRef::SetInt(const std::string& component, const std::string& field, const int value) {
	auto [comp_id, ptr] = ResolveMutableComponent(entity_, component);
	if (!ptr) {
		return;
	}

	ecs_meta_cursor_t cur = ecs_meta_cursor(entity_.world(), comp_id, ptr);
	if (ecs_meta_member(&cur, field.c_str()) != 0) {
		spdlog::warn("[Script] Unknown field '{}.{}'", component, field);
		return;
	}
	ecs_meta_set_int(&cur, value);
}

// -------------------------------------------------------------------------
// Bool
// -------------------------------------------------------------------------

bool ScriptEntityRef::GetBool(const std::string& component, const std::string& field) const {
	auto [comp_id, ptr] = ResolveConstComponent(entity_, component);
	if (!ptr) {
		return false;
	}

	ecs_meta_cursor_t cur = ecs_meta_cursor(entity_.world(), comp_id, const_cast<void*>(ptr));
	if (ecs_meta_member(&cur, field.c_str()) != 0) {
		spdlog::warn("[Script] Unknown field '{}.{}'", component, field);
		return false;
	}
	return ecs_meta_get_bool(&cur);
}

void ScriptEntityRef::SetBool(const std::string& component, const std::string& field, const bool value) {
	auto [comp_id, ptr] = ResolveMutableComponent(entity_, component);
	if (!ptr) {
		return;
	}

	ecs_meta_cursor_t cur = ecs_meta_cursor(entity_.world(), comp_id, ptr);
	if (ecs_meta_member(&cur, field.c_str()) != 0) {
		spdlog::warn("[Script] Unknown field '{}.{}'", component, field);
		return;
	}
	ecs_meta_set_bool(&cur, value);
}

// -------------------------------------------------------------------------
// Observer factory — backend must override this or the module patches it.
// Default implementation logs a warning; backends are expected to bind a
// delegate during Init() that provides the actual script-side dispatch.
// -------------------------------------------------------------------------

void ScriptEntityRef::RegisterObserver(const std::string& event_component, const std::string& callback_fn_name) {
	// Intentionally no-op at this layer. The backend replaces this behavior
	// by registering observer_factory on the ScriptingModule singleton and
	// calling it from within the language-specific type binding.
	spdlog::warn(
		"[Script] RegisterObserver('{}', '{}') called but no backend factory is bound.",
		event_component,
		callback_fn_name
	);
}

} // namespace engine::scripting
