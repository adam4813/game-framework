#include "utility_module.hpp"

#include <algorithm>
#include <cmath>

#include <flecs.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "engine/core/core.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/level/level.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scripting/scripting.hpp"

#include "utility_components.hpp"

namespace engine::ai {

namespace {

float ScoreAction(const UtilityActionDef& action, const std::unordered_map<std::string, float>& ctx) {
	if (action.considerations.empty()) return 0.0F;
	float score = 1.0F;
	for (const auto& [context_key, min_value, max_value, weight] : action.considerations) {
		const auto it = ctx.find(context_key);
		const float raw = it != ctx.end() ? it->second : 0.0F;
		const float norm =
			max_value > min_value ? std::clamp((raw - min_value) / (max_value - min_value), 0.0F, 1.0F) : 0.0F;
		score *= std::pow(norm, std::max(0.001F, weight));
	}
	return score;
}

std::vector<UtilityActionDef> ParseUtilityActions(const nlohmann::json& j) {
	std::vector<UtilityActionDef> actions;
	if (!j.contains("actions") || !j.at("actions").is_array()) return actions;
	for (const auto& a : j.at("actions")) {
		UtilityActionDef def;
		def.name = a.value("name", std::string{});
		def.cooldown = a.value("cooldown", 0.0F);
		if (a.contains("considerations") && a.at("considerations").is_array()) {
			for (const auto& c : a.at("considerations")) {
				UtilityConsideration con;
				con.context_key = c.value("context_key", std::string{});
				con.min_value = c.value("min", 0.0F);
				con.max_value = c.value("max", 1.0F);
				con.weight = c.value("weight", 1.0F);
				if (!con.context_key.empty()) def.considerations.push_back(std::move(con));
			}
		}
		if (!def.name.empty()) actions.push_back(std::move(def));
	}
	return actions;
}

} // namespace

UtilityModule::UtilityModule(const flecs::world& world) {
	// === Singletons ===
	world.set<UtilityActionRegistry>({});

	// === Reflection ===
	world.component<UtilityAgent>().member("current_action", &UtilityAgent::current_action);

	// === Scripting ===
	scripting::RegisterComponentForScripts(world, world.component<UtilityAgent>());

	// === Level loaders ===
	level::RegisterComponentLoader(world, "utility_agent", [](const flecs::entity e, const nlohmann::json& j) {
		UtilityAgent agent;
		agent.eval_interval = j.value("eval_interval", 0.25F);

		if (j.contains("file")) {
			if (const auto doc = core::LoadJsonFile(j.at("file").get<std::string>(), "Utility")) {
				agent.actions = ParseUtilityActions(*doc);
			}
		}
		else if (j.contains("actions")) {
			agent.actions = ParseUtilityActions(j);
		}

		e.set<UtilityAgent>(std::move(agent));
	});

	// === Systems ===
	std::ignore = world.system<UtilityAgent>("UtilityEval")
					  .kind(flecs::OnUpdate)
					  .each([](const flecs::iter& it, size_t, UtilityAgent& agent) {
						  const float dt = it.delta_time();
						  for (auto& cooldown : agent.cooldowns | std::views::values) {
							  cooldown = std::max(0.0F, cooldown - dt);
						  }
						  agent.eval_timer -= dt;
						  if (agent.eval_timer > 0.0F) return;
						  agent.eval_timer = agent.eval_interval;

						  float best = -1.0F;
						  std::string best_name;
						  for (const auto& a : agent.actions) {
							  if (const auto cd_it = agent.cooldowns.find(a.name);
								  cd_it != agent.cooldowns.end() && cd_it->second > 0.0F) {
								  continue;
							  }
							  if (const float score = ScoreAction(a, agent.context); score > best) {
								  best = score;
								  best_name = a.name;
							  }
						  }
						  if (best_name == agent.current_action) return;
						  if (!agent.current_action.empty()) {
							  for (const auto& a : agent.actions) {
								  if (a.name == agent.current_action) {
									  agent.cooldowns[a.name] = a.cooldown;
									  break;
								  }
							  }
						  }
						  agent.current_action = best_name;
					  })
					  .add<ecs::Pausable>()
					  .add<scene::GameScene>();

	std::ignore =
		world.system<UtilityAgent>("UtilityExecute")
			.kind(flecs::OnUpdate)
			.each([](const flecs::iter& it, const size_t i, UtilityAgent& agent) {
				if (agent.current_action.empty()) return;
				const auto& [handlers] = it.world().get<UtilityActionRegistry>();
				if (const auto handler_it = handlers.find(agent.current_action); handler_it != handlers.end()) {
					handler_it->second(it.entity(i), agent, it.delta_time());
				}
			})
			.add<ecs::Pausable>()
			.add<scene::GameScene>();

	spdlog::info("[UtilityModule] Registered Utility AI evaluation and execution systems");
}

} // namespace engine::ai
