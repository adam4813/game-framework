#include "scripting_module.hpp"

#include "script_component.hpp"

#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>

#include "engine/assets/assets.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/level/level.hpp"

namespace engine::scripting {

namespace {

IScriptBackend* GetBackend(const flecs::world& world) {
	if (!world.c_ptr() || ecs_is_fini(world.c_ptr()) || !world.has<ScriptBackendSingleton>()) {
		return nullptr;
	}
	return world.get_mut<ScriptBackendSingleton>().backend.get();
}

} // namespace

ScriptingModule::ScriptingModule(flecs::world& world) {
	{
		IScriptBackend* backend = GetBackend(world);
		if (!backend) {
			spdlog::error(
				"[ScriptingModule] No ScriptBackendSingleton found. Use ScriptingModule::Import<TBackend>()."
			);
			return;
		}

		if (!backend->Init(world)) {
			spdlog::error("[ScriptingModule] Backend Init() failed");
			return;
		}
	}

	// IMPORTANT: ScriptComponent and ScriptHost are intentionally NOT registered with
	// RegisterComponentForScripts. Script systems run with defer_suspend() (immediate mode),
	// so if a script could call RemoveScriptComponent() on itself it would create a use-after-free
	// hazard inside the ScriptPreTick/ScriptOnTick iteration. Script lifecycle is managed
	// exclusively by the C++ scripting module — scripts must not add, remove, or inspect these.
	world.component<ScriptComponent>()
		.on_set([](const flecs::entity entity, ScriptComponent& sc) {
			const flecs::world w = entity.world();
			IScriptBackend* backend = GetBackend(w);
			if (!backend) {
				return;
			}

			const flecs::entity e = entity.parent(); // script component is on a child of the host entity

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
					sc.error_message =
						"Failed to compile inline script on entity '" + std::string(e.name() ? e.name() : "") + "'";
					spdlog::error("[ScriptingModule] {}", sc.error_message);
				}
			}
			else {
				spdlog::warn(
					"[ScriptingModule] ScriptComponent set on '{}' with no source_path or inline_source",
					e.name() ? e.name() : "(unnamed)"
				);
			}
		})
		.on_remove([](const flecs::entity entity, ScriptComponent& sc) {
			if (!sc.instance) {
				return;
			}
			IScriptBackend* backend = GetBackend(entity.world());
			if (!backend) {
				return;
			}

			const flecs::entity script_e = entity.parent();
			if (sc.initialized) {
				sc.instance->OnDestroy(script_e);
			}
			backend->DestroyInstance(sc.instance);
			sc.instance = nullptr;
			sc.initialized = false;
		});
	world.component<ScriptHost>();
	world.component<ScriptTraversal>();

	// === Level loader ===
	level::RegisterComponentLoader(world, "script", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<ScriptComponent>({.source_path = assets::ResolveAsset(e.world(), j.value("source", std::string{}))});
	});

	// RegisterObservers(world);
	RegisterSystems(world);

	spdlog::info("[ScriptingModule] Initialized");
}

void ScriptingModule::RegisterSystems(const flecs::world& world) {
	std::ignore = world.system<ScriptComponent>("ScriptPreTick")
					  .kind(flecs::PreUpdate)
					  .immediate()
					  .each([](const flecs::iter& it, const size_t i, ScriptComponent& sc) {
						  if (!sc.instance || sc.error != ScriptError::None) {
							  return;
						  }
						  const flecs::entity script_e =
							  it.entity(i).parent(); // script component is on a child of the host entity
						  if (!sc.initialized) {
							  it.world().defer_suspend();
							  sc.instance->OnInit(script_e);
							  sc.initialized = true;
							  it.world().defer_resume();
						  }
						  it.world().defer_suspend();
						  sc.instance->Tick(script_e, it.delta_time(), ScriptTickPhase::Pre);
						  it.world().defer_resume();
					  })
					  .add<ecs::Pausable>();

	std::ignore = world.system<ScriptComponent>("ScriptOnTick")
					  .kind(flecs::OnUpdate)
					  .immediate()
					  .each([](const flecs::iter& it, const size_t i, const ScriptComponent& sc) {
						  if (!sc.instance || sc.error != ScriptError::None || !sc.initialized) {
							  return;
						  }
						  const flecs::entity script_e =
							  it.entity(i).parent(); // script component is on a child of the host entity
						  it.world().defer_suspend();
						  sc.instance->Tick(script_e, it.delta_time(), ScriptTickPhase::On);
						  it.world().defer_resume();
					  })
					  .add<ecs::Pausable>();

	std::ignore = world.system<ScriptComponent>("ScriptPostTick")
					  .kind(flecs::PostUpdate)
					  .each([](const flecs::iter& it, const size_t i, const ScriptComponent& sc) {
						  if (!sc.instance || sc.error != ScriptError::None || !sc.initialized) {
							  return;
						  }
						  const flecs::entity script_e =
							  it.entity(i).parent(); // script component is on a child of the host entity
						  sc.instance->Tick(script_e, it.delta_time(), ScriptTickPhase::Post);
					  })
					  .add<ecs::Pausable>();
}

} // namespace engine::scripting
