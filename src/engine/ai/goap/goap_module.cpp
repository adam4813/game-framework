#include "goap_module.hpp"

#include <algorithm>
#include <queue>
#include <set>
#include <string>

#include <flecs.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "engine/core/core.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/level/level.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scripting/scripting.hpp"

#include "goap_components.hpp"

namespace engine::ai {

namespace {

bool GOAPSatisfies(const std::unordered_map<std::string, bool>& state, const std::vector<GOAPFact>& goal) {
	return std::ranges::all_of(goal, [&](const GOAPFact& f) {
		const auto it = state.find(f.key);
		return it != state.end() && it->second == f.value;
	});
}

bool GOAPCanApply(const std::unordered_map<std::string, bool>& state, const std::vector<GOAPFact>& pre) {
	return std::ranges::all_of(pre, [&](const GOAPFact& f) {
		const auto it = state.find(f.key);
		return it != state.end() && it->second == f.value;
	});
}

std::unordered_map<std::string, bool>
GOAPApplyEffects(std::unordered_map<std::string, bool> state, const std::vector<GOAPFact>& effects) {
	for (const auto& f : effects) state[f.key] = f.value;
	return state;
}

/// Serialize a world-state map to a canonical string for visited-set membership testing.
/// Keys are sorted so identical states with different insertion order compare equal.
std::string SerializeState(const std::unordered_map<std::string, bool>& state) {
	std::vector<std::pair<std::string, bool>> sorted{state.begin(), state.end()};
	std::ranges::sort(sorted, {}, &std::pair<std::string, bool>::first);
	std::string out;
	out.reserve(sorted.size() * 8);
	for (const auto& [k, v] : sorted) {
		out += k;
		out += v ? '1' : '0';
		out += ';';
	}
	return out;
}

/// Forward-search uniform-cost GOAP planner (Dijkstra / greedy by action cost).
/// A closed/visited set prevents re-expanding identical world states, bounding the
/// search to at most O(2^|facts|) unique states rather than O(kMaxIter) duplicates.
std::vector<std::string> BuildGOAPPlan(
	const std::unordered_map<std::string, bool>& initial_state,
	const GOAPGoal& goal,
	const std::vector<GOAPActionDef>& actions
) {
	struct Node {
		std::unordered_map<std::string, bool> state;
		std::vector<std::string> plan;
		float cost{0.0F};
		bool operator>(const Node& o) const { return cost > o.cost; }
	};
	std::priority_queue<Node, std::vector<Node>, std::greater<>> open;
	std::set<std::string> closed;
	open.push({.state = initial_state, .plan = {}, .cost = 0.0F});
	constexpr int kMaxIter = 256;
	constexpr int kMaxDepth = 16;
	for (int i = 0; i < kMaxIter && !open.empty(); ++i) {
		auto [state, plan, cost] = open.top();
		open.pop();
		if (GOAPSatisfies(state, goal.desired_state)) return plan;
		if (const std::string state_key = SerializeState(state); !closed.insert(state_key).second) {
			continue; // already expanded this world state
		}
		if (static_cast<int>(plan.size()) >= kMaxDepth) continue;
		for (const auto& a : actions) {
			if (!GOAPCanApply(state, a.preconditions)) continue;
			Node next;
			next.state = GOAPApplyEffects(state, a.effects);
			next.plan = plan;
			next.plan.push_back(a.name);
			next.cost = cost + a.cost;
			open.push(std::move(next));
		}
	}
	return {};
}

// Parse a fact array: [{"key": "has_weapon", "value": false}, ...]
std::vector<GOAPFact> ParseFacts(const nlohmann::json& arr) {
	std::vector<GOAPFact> facts;
	if (!arr.is_array()) return facts;
	for (const auto& f : arr) {
		GOAPFact fact;
		fact.key = f.value("key", std::string{});
		fact.value = f.value("value", true);
		if (!fact.key.empty()) facts.push_back(std::move(fact));
	}
	return facts;
}

std::vector<GOAPActionDef> ParseActions(const nlohmann::json& j) {
	std::vector<GOAPActionDef> actions;
	if (!j.contains("actions") || !j.at("actions").is_array()) return actions;
	for (const auto& a : j.at("actions")) {
		GOAPActionDef def;
		def.name = a.value("name", std::string{});
		def.cost = a.value("cost", 1.0F);
		def.duration = a.value("duration", 0.0F);
		if (a.contains("preconditions")) def.preconditions = ParseFacts(a.at("preconditions"));
		if (a.contains("effects")) def.effects = ParseFacts(a.at("effects"));
		if (!def.name.empty()) actions.push_back(std::move(def));
	}
	return actions;
}

} // namespace

GOAPModule::GOAPModule(const flecs::world& world) {
	// === Singletons ===
	world.set<GOAPActionRegistry>({});

	// === Reflection ===
	world.component<GOAPAgent>()
		.member("needs_replan", &GOAPAgent::needs_replan)
		.member("plan_failed", &GOAPAgent::plan_failed);

	// === Scripting ===
	scripting::RegisterComponentForScripts(world, world.component<GOAPAgent>());

	// === Level loaders ===
	level::RegisterComponentLoader(world, "goap_agent", [](const flecs::entity e, const nlohmann::json& j) {
		GOAPAgent agent;

		// World state — inline key/value object: { "has_weapon": false, ... }
		if (j.contains("world_state") && j.at("world_state").is_object()) {
			for (const auto& [k, v] : j.at("world_state").items()) {
				if (v.is_boolean()) {
					agent.world_state[k] = v.get<bool>();
				}
			}
		}

		// Actions — loaded from a separate file to keep entities tidy.
		if (j.contains("actions_file")) {
			if (const auto doc = core::LoadJsonFile(j.at("actions_file").get<std::string>(), "GOAP")) {
				agent.available_actions = ParseActions(*doc);
			}
		}

		// Goals — inline array.
		if (j.contains("goals") && j.at("goals").is_array()) {
			for (const auto& g : j.at("goals")) {
				GOAPGoal goal;
				goal.name = g.value("name", std::string{});
				goal.priority = g.value("priority", 1.0F);
				if (g.contains("desired_state")) goal.desired_state = ParseFacts(g.at("desired_state"));
				if (!goal.name.empty()) agent.goals.push_back(std::move(goal));
			}
		}

		agent.needs_replan = true;
		e.set<GOAPAgent>(std::move(agent));
	});

	// === Systems ===
	std::ignore = world.system<GOAPAgent>("GOAPPlanner")
					  .kind(flecs::OnUpdate)
					  .each([](GOAPAgent& agent) {
						  if (!agent.needs_replan) return;
						  agent.needs_replan = false;
						  agent.plan_failed = false;
						  agent.current_action_index = 0;
						  agent.action_elapsed = 0.0F;

						  const GOAPGoal* best = nullptr;
						  for (const auto& g : agent.goals) {
							  if (!best || g.priority > best->priority) best = &g;
						  }
						  if (!best) {
							  agent.plan_failed = true;
							  return;
						  }

						  agent.current_plan = BuildGOAPPlan(agent.world_state, *best, agent.available_actions);
						  if (agent.current_plan.empty()) {
							  agent.plan_failed = true;
							  spdlog::debug("[GOAP] No plan found for goal '{}'", best->name);
						  }
					  })
					  .add<ecs::Pausable>()
					  .add<scene::GameScene>();

	std::ignore = world.system<GOAPAgent>("GOAPExecutor")
					  .kind(flecs::OnUpdate)
					  .each([](const flecs::iter& it, const size_t i, GOAPAgent& agent) {
						  if (agent.needs_replan || agent.plan_failed) return;
						  if (agent.current_action_index >= agent.current_plan.size()) {
							  agent.needs_replan = true;
							  return;
						  }
						  const auto& action_name = agent.current_plan[agent.current_action_index];
						  const auto& [handlers] = it.world().get<GOAPActionRegistry>();
						  const auto handler_it = handlers.find(action_name);
						  if (handler_it == handlers.end()) {
							  spdlog::warn("[GOAP] No handler for action '{}'", action_name);
							  agent.plan_failed = true;
							  return;
						  }
						  const flecs::entity e = it.entity(i);
						  if (agent.action_elapsed == 0.0F) {
							  if (handler_it->second.on_start && !handler_it->second.on_start(e, agent)) {
								  agent.plan_failed = true;
								  return;
							  }
						  }
						  agent.action_elapsed += it.delta_time();

						  const GOAPActionDef* def = nullptr;
						  for (const auto& a : agent.available_actions) {
							  if (a.name == action_name) {
								  def = &a;
								  break;
							  }
						  }
						  const bool done = handler_it->second.on_tick
												? handler_it->second.on_tick(e, agent, it.delta_time())
												: (def && agent.action_elapsed >= def->duration);
						  if (done) {
							  if (def) {
								  for (const auto& f : def->effects) agent.world_state[f.key] = f.value;
							  }
							  ++agent.current_action_index;
							  agent.action_elapsed = 0.0F;
						  }
					  })
					  .add<ecs::Pausable>()
					  .add<scene::GameScene>();

	spdlog::info("[GOAPModule] Registered GOAP planner and executor systems");
}

} // namespace engine::ai
