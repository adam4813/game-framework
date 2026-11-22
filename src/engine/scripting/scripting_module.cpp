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

// Records that a script threw in `entry` (the backend has logged the exception), so no system
// calls it again. A script that throws every frame would otherwise log at frame rate forever.
void DisableThrownScript(const flecs::world& world, const flecs::entity host, ScriptComponent& sc, const char* entry) {
	sc.error = ScriptError::RuntimeError;
	const std::string label = !sc.source_path.empty() ? sc.source_path : std::string{host.name().c_str()};
	sc.error_message = "Script '" + label + "' threw in " + entry + "; disabled";
	ecs::Log(world).error("[ScriptingModule] {}", sc.error_message);
}

// The script entry point a tick phase calls, as scripts spell it.
const char* EntryPointName(const ScriptTickPhase phase) {
	switch (phase) {
	case ScriptTickPhase::Pre: return "PreTick";
	case ScriptTickPhase::On: return "Tick";
	case ScriptTickPhase::Post: return "PostTick";
	}
	return "Tick";
}

// Runs one phase of a live script, initializing it first in the Pre phase. The ScriptComponent
// sits on a child of the host; the instance was bound to the host when it was created.
void TickScript(const flecs::iter& it, const size_t i, ScriptComponent& sc, const ScriptTickPhase phase) {
	if (!sc.instance || sc.error != ScriptError::None) {
		return;
	}
	const flecs::entity host = it.entity(i).parent();
	if (!sc.initialized) {
		if (phase != ScriptTickPhase::Pre) {
			return;
		}
		sc.initialized = true;
		if (!sc.instance->OnInit()) {
			DisableThrownScript(it.world(), host, sc, "OnInit");
			return;
		}
	}
	if (!sc.instance->Tick(it.delta_time(), phase)) {
		DisableThrownScript(it.world(), host, sc, EntryPointName(phase));
	}
}

} // namespace

ScriptingModule::ScriptingModule(flecs::world& world) {
	{
		IScriptBackend* backend = GetBackend(world);
		if (!backend) {
			ecs::Log(world).error(
				"[ScriptingModule] No ScriptBackendSingleton found. Use ScriptingModule::Import<TBackend>()."
			);
			return;
		}

		if (!backend->Init(world)) {
			ecs::Log(world).error("[ScriptingModule] Backend Init() failed");
			return;
		}
	}

	// IMPORTANT: ScriptComponent is intentionally NOT registered with
	// RegisterComponentForScripts. Script systems run with defer_suspend() (immediate mode),
	// so if a script could call RemoveScriptComponent() on itself it would create a use-after-free
	// hazard inside the ScriptPreTick/ScriptOnTick iteration. Script lifecycle is managed
	// exclusively by the C++ scripting module — scripts must not add, remove, or inspect it.
	world.component<ScriptComponent>()
		.on_set([](const flecs::entity entity, ScriptComponent& sc) {
			const flecs::world w = entity.world();
			IScriptBackend* backend = GetBackend(w);
			if (!backend) {
				return;
			}

			// The ScriptComponent sits on a child of the host; the script is bound to the host (its `self`).
			const flecs::entity host = entity.parent();

			if (!sc.source_path.empty()) {
				sc.instance = backend->CreateInstanceFromFile(sc.source_path, host);
				if (!sc.instance) {
					sc.error = ScriptError::CompileFailed;
					sc.error_message = "Failed to compile script from file '" + sc.source_path + "'";
					ecs::Log(w).error("[ScriptingModule] {}", sc.error_message);
				}
			}
			else if (!sc.inline_source.empty()) {
				const std::string module_name = host.name() ? std::string{host.name()} : std::to_string(host.id());
				sc.instance = backend->CreateInstanceFromSource(module_name, sc.inline_source, host);
				if (!sc.instance) {
					sc.error = ScriptError::CompileFailed;
					sc.error_message = "Failed to compile inline script on entity '"
									   + std::string(host.name() ? host.name() : "")
									   + "'";
					ecs::Log(w).error("[ScriptingModule] {}", sc.error_message);
				}
			}
			else {
				ecs::Log(w).warn(
					"[ScriptingModule] ScriptComponent set on '{}' with no source_path or inline_source",
					host.name() ? host.name() : "(unnamed)"
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

			// A script that threw is not called again, teardown included.
			if (sc.initialized && sc.error == ScriptError::None) {
				sc.instance->OnDestroy();
			}
			backend->DestroyInstance(sc.instance);
			sc.instance = nullptr;
			sc.initialized = false;
		});
	world.component<ScriptTraversal>();

	// === Level loader ===
	level::RegisterComponentLoader(world, "script", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<ScriptComponent>({.source_path = assets::ResolveAsset(e.world(), j.value("source", std::string{}))});
	});

	// RegisterObservers(world);
	RegisterSystems(world);

	ecs::Log(world).info("[ScriptingModule] Initialized");
}

void ScriptingModule::RegisterSystems(const flecs::world& world) {
	std::ignore = world.system<ScriptComponent>("ScriptPreTick")
					  .kind(flecs::PreUpdate)
					  .immediate()
					  .each([](const flecs::iter& it, const size_t i, ScriptComponent& sc) {
						  it.world().defer_suspend();
						  TickScript(it, i, sc, ScriptTickPhase::Pre);
						  it.world().defer_resume();
					  })
					  .add<ecs::Pausable>();

	std::ignore = world.system<ScriptComponent>("ScriptOnTick")
					  .kind(flecs::OnUpdate)
					  .immediate()
					  .each([](const flecs::iter& it, const size_t i, ScriptComponent& sc) {
						  it.world().defer_suspend();
						  TickScript(it, i, sc, ScriptTickPhase::On);
						  it.world().defer_resume();
					  })
					  .add<ecs::Pausable>();

	// PostTick is the reactive phase: it runs deferred, so the changes a script makes there
	// apply at the next merge rather than at once (unlike PreTick and Tick).
	std::ignore = world.system<ScriptComponent>("ScriptPostTick")
					  .kind(flecs::PostUpdate)
					  .each([](const flecs::iter& it, const size_t i, ScriptComponent& sc) {
						  TickScript(it, i, sc, ScriptTickPhase::Post);
					  })
					  .add<ecs::Pausable>();
}

} // namespace engine::scripting
