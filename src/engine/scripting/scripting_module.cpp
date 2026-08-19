#include "scripting_module.hpp"

#include "script_component.hpp"

#include <spdlog/spdlog.h>

#include "engine/ecs/ecs.hpp"

namespace engine::scripting {

namespace {

IScriptBackend* GetBackend(const flecs::world& world) {
	if (!world.has<ScriptBackendSingleton>()) {
		return nullptr;
	}
	return world.get_mut<ScriptBackendSingleton>().backend.get();
}

} // namespace

ScriptingModule::ScriptingModule(flecs::world& world) {
	IScriptBackend* backend = GetBackend(world);
	if (!backend) {
		spdlog::error("[ScriptingModule] No ScriptBackendSingleton found. Use ScriptingModule::Import<TBackend>().");
		return;
	}

	if (!backend->Init(world)) {
		spdlog::error("[ScriptingModule] Backend Init() failed");
		return;
	}

	world.component<ScriptComponent>();
	world.component<ScriptHost>();

	RegisterObservers(world);
	RegisterSystems(world);

	spdlog::info("[ScriptingModule] Initialized");
}

// -------------------------------------------------------------------------
// Observers — backend retrieved from world to stay in sync with the singleton
// -------------------------------------------------------------------------

void ScriptingModule::RegisterObservers(const flecs::world& world) {
	// OnSet: fires when ScriptComponent data is written.
	// Asks the backend to compile the script and stores the instance for direct dispatch.
	// Errors during compilation are captured in the component's error field.
	world.observer<ScriptComponent>("ScriptComponent.OnSet")
		.event(flecs::OnSet)
		.each([](const flecs::iter& it, const size_t i, ScriptComponent& sc) {
			if (sc.instance) {
				return; // already loaded
			}

			IScriptBackend* backend = GetBackend(it.world());
			if (!backend) {
				return;
			}

			const flecs::entity e = it.entity(i);

			if (!sc.source_path.empty()) {
				sc.instance = backend->CreateInstanceFromFile(sc.source_path, e);
				if (!sc.instance) {
					sc.error = ScriptError::CompileFailed;
					sc.error_message = "Failed to compile script from file '" + sc.source_path + "'";
					spdlog::error("[ScriptingModule] {}", sc.error_message);
				}
			}
			else if (!sc.inline_source.empty()) {
				const std::string module_name = e.name() ? std::string{e.name()} : std::to_string(e.id());
				sc.instance = backend->CreateInstanceFromSource(module_name, sc.inline_source, e);
				if (!sc.instance) {
					sc.error = ScriptError::CompileFailed;
					sc.error_message = "Failed to compile inline script on entity '" + std::string(e.name() ? e.name() : "") + "'";
					spdlog::error("[ScriptingModule] {}", sc.error_message);
				}
			}
			else {
				spdlog::warn(
					"[ScriptingModule] ScriptComponent set on '{}' with no source_path or inline_source",
					e.name() ? e.name() : "(unnamed)"
				);
			}
		});

	// OnRemove: fires just before the component is removed.
	// Calls OnDestroy on the instance, then asks the backend to release it.
	world.observer<ScriptComponent>("ScriptComponent.OnRemove")
		.event(flecs::OnRemove)
		.each([](const flecs::iter& it, const size_t i, ScriptComponent& sc) {
			if (!sc.instance) {
				return;
			}

			IScriptBackend* backend = GetBackend(it.world());
			if (!backend) {
				return;
			}

			const flecs::entity script_e = it.entity(i);
			if (sc.initialized) {
				sc.instance->OnDestroy(script_e);
			}
			backend->DestroyInstance(sc.instance);
			sc.instance = nullptr;
			sc.initialized = false;
		});
}

// -------------------------------------------------------------------------
// Tick systems — dispatch directly through IScriptInstance, no backend needed
// -------------------------------------------------------------------------

void ScriptingModule::RegisterSystems(const flecs::world& world) {
	// PreUpdate: call OnInit on first tick, then dispatch Pre.
	// Skip initialization and tick if compilation failed.
	world.system<ScriptComponent>("ScriptPreTick")
		.kind(flecs::PreUpdate)
		.each([](const flecs::iter& it, const size_t i, ScriptComponent& sc) {
			if (!sc.instance || sc.error != ScriptError::None) {
				return;
			}
			const flecs::entity script_e = it.entity(i);
			if (!sc.initialized) {
				sc.instance->OnInit(script_e);
				sc.initialized = true;
			}
			sc.instance->Tick(script_e, it.delta_time(), ScriptTickPhase::Pre);
		})
		.add<ecs::Pausable>();

	// OnUpdate: dispatch On.
	// Skip tick if compilation failed.
	world.system<ScriptComponent>("ScriptOnTick")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, const ScriptComponent& sc) {
			if (!sc.instance || sc.error != ScriptError::None || !sc.initialized) {
				return;
			}
			sc.instance->Tick(it.entity(i), it.delta_time(), ScriptTickPhase::On);
		})
		.add<ecs::Pausable>();

	// PostUpdate: dispatch Post.
	// Skip tick if compilation failed.
	world.system<ScriptComponent>("ScriptPostTick")
		.kind(flecs::PostUpdate)
		.each([](const flecs::iter& it, const size_t i, const ScriptComponent& sc) {
			if (!sc.instance || sc.error != ScriptError::None || !sc.initialized) {
				return;
			}
			sc.instance->Tick(it.entity(i), it.delta_time(), ScriptTickPhase::Post);
		})
		.add<ecs::Pausable>();
}

} // namespace engine::scripting
