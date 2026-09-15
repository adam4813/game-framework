#include "fsm_module.hpp"

#include <algorithm>
#include <ranges>

#include <flecs.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "engine/core/core.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/level/level.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scripting/scripting.hpp"

#include "fsm_components.hpp"

namespace engine::ai {

namespace {

FSMStateDef ParseState(const nlohmann::json& j) {
	FSMStateDef state;
	state.name = j.value("name", std::string{});
	state.max_duration = j.value("max_duration", -1.0F);
	state.timeout_target = j.value("timeout_target", std::string{});
	if (j.contains("transitions") && j.at("transitions").is_array()) {
		for (const auto& t : j.at("transitions")) {
			FSMTransition tr;
			tr.condition = t.value("condition", std::string{});
			tr.expected = t.value("expected", true);
			tr.target = t.value("target", std::string{});
			tr.min_time = t.value("min_time", 0.0F);
			if (!tr.condition.empty() && !tr.target.empty()) {
				state.transitions.push_back(std::move(tr));
			}
		}
	}
	return state;
}

} // namespace

FSMModule::FSMModule(const flecs::world& world) {
	// === Reflection ===
	world.component<FSMAgent>()
		.member("current_state", &FSMAgent::current_state)
		.member("previous_state", &FSMAgent::previous_state)
		.member("state_elapsed", &FSMAgent::state_elapsed)
		.member("state_changed", &FSMAgent::state_changed);

	// === Scripting ===
	scripting::RegisterComponentForScripts(world, world.component<FSMAgent>());

	// === Level loaders ===
	level::RegisterComponentLoader(world, "fsm_agent", [](const flecs::entity e, const nlohmann::json& j) {
		FSMAgent agent;
		agent.current_state = j.value("initial_state", std::string{});

		// States may be defined inline or loaded from a file.
		if (j.contains("file")) {
			if (const auto doc = core::LoadJsonFile(j.at("file").get<std::string>(), "FSM");
				doc && doc->contains("states") && doc->at("states").is_array()) {
				for (const auto& s : doc->at("states")) {
					agent.states.push_back(ParseState(s));
				}
			}
		}
		else if (j.contains("states") && j.at("states").is_array()) {
			for (const auto& s : j.at("states")) {
				agent.states.push_back(ParseState(s));
			}
		}

		e.set<FSMAgent>(std::move(agent));
	});

	// === System ===
	std::ignore = world.system<FSMAgent>("FSMUpdate")
					  .kind(flecs::OnUpdate)
					  .each([](const flecs::iter& it, size_t, FSMAgent& agent) {
						  agent.state_changed = false;
						  agent.state_elapsed += it.delta_time();
						  const auto state_it = std::ranges::find_if(agent.states, [&](const FSMStateDef& s) {
							  return s.name == agent.current_state;
						  });
						  if (state_it == agent.states.end()) return;

						  // Timeout transition.
						  if (state_it->max_duration >= 0.0F
							  && agent.state_elapsed >= state_it->max_duration
							  && !state_it->timeout_target.empty()) {
							  agent.previous_state = agent.current_state;
							  agent.current_state = state_it->timeout_target;
							  agent.state_elapsed = 0.0F;
							  agent.state_changed = true;
							  return;
						  }

						  // Condition transitions (first match wins).
						  for (const auto& [condition, expected, target, min_time] : state_it->transitions) {
							  if (agent.state_elapsed < min_time) continue;
							  if (const auto cit = agent.conditions.find(condition);
								  cit == agent.conditions.end() || cit->second != expected) {
								  continue;
							  }
							  agent.previous_state = agent.current_state;
							  agent.current_state = target;
							  agent.state_elapsed = 0.0F;
							  agent.state_changed = true;
							  break;
						  }
					  })
					  .add<ecs::Pausable>()
					  .add<scene::GameScene>();

	spdlog::info("[FSMModule] Registered FSM update system");
}

} // namespace engine::ai
