#include "bt_module.hpp"

#include <flecs.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "engine/ecs/ecs.hpp"
#include "engine/graph/graph.hpp"
#include "engine/level/level.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scripting/scripting.hpp"

#include "../ai_components.hpp"
#include "bt_components.hpp"

namespace engine::ai {

namespace {

BTNodeType ParseBTNodeType(const std::string& s) {
	if (s == "sequence") return BTNodeType::Sequence;
	if (s == "selector") return BTNodeType::Selector;
	if (s == "parallel") return BTNodeType::Parallel;
	if (s == "inverter") return BTNodeType::Inverter;
	if (s == "repeater") return BTNodeType::Repeater;
	if (s == "succeeder") return BTNodeType::Succeeder;
	if (s == "condition") return BTNodeType::Condition;
	return BTNodeType::Action;
}

BTStatus TickBTNode(
	const graph::Tree<BTNodeDef>& tree,
	const graph::NodeId id,
	const flecs::entity e,
	Blackboard& bb,
	const BTHandlerRegistry& reg,
	const float dt,
	std::vector<BTStatus>& statuses
) {
	if (!tree.Valid(id)) return BTStatus::Failure;
	const auto& [type, handler_key, parallel_success_min, repeat_count] = tree.Get(id);
	const auto children = tree.Children(id);
	auto result = BTStatus::Failure;

	switch (type) {
	case BTNodeType::Sequence:
		result = BTStatus::Success;
		for (const auto cid : children) {
			if (const auto s = TickBTNode(tree, cid, e, bb, reg, dt, statuses); s != BTStatus::Success) {
				result = s;
				break;
			}
		}
		break;
	case BTNodeType::Selector:
		result = BTStatus::Failure;
		for (const auto cid : children) {
			if (const auto s = TickBTNode(tree, cid, e, bb, reg, dt, statuses); s != BTStatus::Failure) {
				result = s;
				break;
			}
		}
		break;
	case BTNodeType::Parallel:
	{
		int successes = 0;
		bool any_running = false;
		for (const auto cid : children) {
			if (const auto s = TickBTNode(tree, cid, e, bb, reg, dt, statuses); s == BTStatus::Success) {
				++successes;
			}
			else if (s == BTStatus::Running) {
				any_running = true;
			}
		}
		if (successes >= parallel_success_min) {
			result = BTStatus::Success;
		}
		else if (any_running) {
			result = BTStatus::Running;
		}
		break;
	}
	case BTNodeType::Inverter:
		if (!children.empty()) {
			const auto s = TickBTNode(tree, children[0], e, bb, reg, dt, statuses);
			result = (s == BTStatus::Success) ? BTStatus::Failure : (s == BTStatus::Failure) ? BTStatus::Success : s;
		}
		break;
	case BTNodeType::Repeater:
	{
		if (children.empty()) break;
		const std::string count_key = "_rpt_" + std::to_string(static_cast<int>(id));
		if (const auto s = TickBTNode(tree, children[0], e, bb, reg, dt, statuses); s == BTStatus::Running) {
			result = BTStatus::Running;
			break;
		}
		if (repeat_count < 0) {
			result = BTStatus::Running;
			break;
		} // infinite
		bb.floats[count_key] += 1.0F;
		if (static_cast<int>(bb.floats[count_key]) >= repeat_count) {
			bb.floats.erase(count_key);
			result = BTStatus::Success;
		}
		else {
			result = BTStatus::Running;
		}
		break;
	}
	case BTNodeType::Succeeder:
		if (!children.empty()) TickBTNode(tree, children[0], e, bb, reg, dt, statuses);
		result = BTStatus::Success;
		break;
	case BTNodeType::Action:
	{
		if (const auto it = reg.actions.find(handler_key); it != reg.actions.end()) {
			result = it->second(e, bb, dt);
		}
		else {
			spdlog::warn("[BT] Action handler not found: '{}'", handler_key);
		}
		break;
	}
	case BTNodeType::Condition:
	{
		if (const auto it = reg.conditions.find(handler_key); it != reg.conditions.end()) {
			result = it->second(e, bb) ? BTStatus::Success : BTStatus::Failure;
		}
		else {
			spdlog::warn("[BT] Condition handler not found: '{}'", handler_key);
		}
		break;
	}
	}

	if (id < static_cast<graph::NodeId>(statuses.size())) statuses[id] = result;
	return result;
}

} // namespace

BTModule::BTModule(flecs::world& world) {
	world.import<graph::GraphModule>();

	// === Singletons ===
	world.set<BTHandlerRegistry>({});

	// === Reflection ===
	world.component<BehaviorTree>()
		.member("tick_interval", &BehaviorTree::tick_interval)
		.member("tick_timer", &BehaviorTree::tick_timer);

	// === Scripting ===
	// BehaviorTree: expose tick_interval so scripts can tune the tick rate.
	scripting::RegisterComponentForScripts(world, world.component<BehaviorTree>());
	// BTStatus constants let scripts interpret root_status.
	scripting::RegisterGlobalConstantsForScripts(
		world,
		{
			{.name = "BTStatus_Invalid", .type = scripting::ScriptValueType::MakeInt(), .value_str = "0"},
			{.name = "BTStatus_Running", .type = scripting::ScriptValueType::MakeInt(), .value_str = "1"},
			{.name = "BTStatus_Success", .type = scripting::ScriptValueType::MakeInt(), .value_str = "2"},
			{.name = "BTStatus_Failure", .type = scripting::ScriptValueType::MakeInt(), .value_str = "3"},
		}
	);

	// === Level loaders ===
	level::RegisterComponentLoader(world, "behavior_tree", [](const flecs::entity e, const nlohmann::json& j) {
		BehaviorTree bt;
		bt.tick_interval = j.value("tick_interval", 0.0F);
		if (j.contains("file")) {
			bt.tree =
				graph::LoadTreeFromFile<BTNodeDef>(j.at("file").get<std::string>(), "BT", [](const nlohmann::json& n) {
					BTNodeDef def;
					def.type = ParseBTNodeType(n.value("type", std::string{"action"}));
					def.handler_key = n.value("handler", std::string{});
					def.parallel_success_min = n.value("parallel_success_min", 1);
					def.repeat_count = n.value("repeat_count", -1);
					return def;
				});
		}
		e.set<BehaviorTree>(std::move(bt));
		// Ensure a Blackboard exists — BT callbacks need it.
		if (!e.has<Blackboard>()) e.set<Blackboard>({});
	});

	// === System ===
	std::ignore =
		world.system<BehaviorTree, Blackboard>("BTExecution")
			.kind(flecs::OnUpdate)
			.each([](const flecs::iter& it, const size_t i, BehaviorTree& bt, Blackboard& bb) {
				bt.tick_timer -= it.delta_time();
				if (bt.tick_timer > 0.0F) return;
				bt.tick_timer = bt.tick_interval;
				if (!bt.tree.Valid(bt.tree.root)) return;
				if (const size_t required = bt.tree.NodeCount() + 1; bt.node_statuses.size() != required) {
					bt.node_statuses.assign(required, BTStatus::Invalid);
				}
				const auto& reg = it.world().get<BTHandlerRegistry>();
				bt.root_status =
					TickBTNode(bt.tree, bt.tree.root, it.entity(i), bb, reg, it.delta_time(), bt.node_statuses);
			})
			.add<ecs::Pausable>()
			.add<scene::GameScene>();

	spdlog::info("[BTModule] Registered Behavior Tree systems");
}

} // namespace engine::ai
