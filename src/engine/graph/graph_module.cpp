#include "graph_module.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "engine/level/level.hpp"
#include "engine/scripting/scripting.hpp"

#include "graph_components.hpp"
#include "graph_types.hpp"

namespace engine::graph {

GraphModule::GraphModule(const flecs::world& world) {
	// === Reflection ===
	world.component<NodeId>();
	world.component<GraphNode>().member("label", &GraphNode::label).member("order", &GraphNode::order);
	world.component<GraphRoot>();
	world.component<GraphEdge>();
	world.component<GraphEdgeWeight>().member("weight", &GraphEdgeWeight::weight).member("tag", &GraphEdgeWeight::tag);

	// === Scripting ===
	scripting::RegisterValueTypeForScripts(world, world.component<NodeId>());
	scripting::RegisterComponentForScripts(world, world.component<GraphNode>());
	scripting::RegisterComponentForScripts(world, world.component<GraphEdgeWeight>());

	// === Level loaders ===
	level::RegisterTag<GraphRoot>(world, "graph_root");

	level::RegisterComponentLoader(world, "graph_node", [](const flecs::entity e, const nlohmann::json& j) {
		GraphNode n;
		n.id = j.value("id", NullNode);
		n.label = j.value("label", std::string{});
		n.order = j.value("order", 0);
		e.set<GraphNode>(n);
	});

	level::RegisterComponentLoader(world, "graph_edge_weight", [](const flecs::entity e, const nlohmann::json& j) {
		GraphEdgeWeight w;
		w.weight = j.value("weight", 1.0F);
		w.tag = j.value("tag", std::string{});
		e.set<GraphEdgeWeight>(w);
	});

	level::RegisterComponentLoader(world, "graph_node_id", [](const flecs::entity e, const nlohmann::json& j) {
		const NodeId id = j.is_number_unsigned() ? j.get<NodeId>() : j.value("id", NullNode);
		if (id != NullNode) e.set<NodeId>(id);
	});

	level::RegisterLink(world, "graph_edge", [](const flecs::entity parent, const flecs::entity child) {
		parent.add<GraphEdge>(child);
	});

	spdlog::info("[GraphModule] Registered graph node/edge components and loaders");
}

} // namespace engine::graph
