#pragma once

#include <string>

#include "graph_types.hpp"

namespace engine::graph {

/// Component attached to any entity that acts as a graph/tree node.
/// The entity itself IS the node — topology is expressed via GraphEdge relationships.
struct GraphNode {
	NodeId id{NullNode}; ///< Stable numeric id within its graph (set by loader or code)
	std::string label;   ///< Human-readable label for tooling and debug display
	int order{0};        ///< Traversal sort index among siblings
};

/// Tag: marks the root entity of a graph or tree.
struct GraphRoot {};

/// Zero-size relationship component: GraphEdge(parent → child).
/// Presence of the relationship encodes a directed edge.
/// For weighted or labelled edges, also attach GraphEdgeWeight to the child entity.
///
/// Usage:
///   parent.add<GraphEdge>(child);        // directed edge parent → child
///   parent.each<GraphEdge>([](flecs::entity child) { ... }); // traverse children
struct GraphEdge {};

/// Optional per-edge data attached to the edge target (child entity).
/// Useful for weighted graphs, dialog choices, skill-tree branches, etc.
struct GraphEdgeWeight {
	float weight{1.0F}; ///< Edge cost or priority (lower = preferred in pathfinding)
	std::string tag;    ///< Semantic label, e.g. "yes"/"no", "attack"/"retreat"
};

} // namespace engine::graph
