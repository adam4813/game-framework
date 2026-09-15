#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace engine::graph {

using NodeId = uint32_t;
constexpr NodeId NullNode{0};

/// Internal bookkeeping entry stored parallel to each node's data.
struct NodeEntry {
	NodeId id{NullNode};
	NodeId parent{NullNode};
	std::vector<NodeId> children;
};

/// Flat directed graph with 1-based NodeIds.
/// NodeId 0 is reserved as null; valid ids start at 1.
template<typename T>
struct Graph {
	std::vector<T> data;          ///< Indexed by NodeId-1
	std::vector<NodeEntry> nodes; ///< Structural bookkeeping, indexed by NodeId-1

	NodeId AddNode(T node_data) {
		const auto id = static_cast<NodeId>(data.size() + 1);
		data.push_back(std::move(node_data));
		nodes.push_back(NodeEntry{.id = id});
		return id;
	}

	void AddEdge(const NodeId from, const NodeId to) {
		if (from == NullNode || to == NullNode) return;
		if (from > static_cast<NodeId>(nodes.size()) || to > static_cast<NodeId>(nodes.size())) return;
		nodes[from - 1].children.push_back(to);
	}

	[[nodiscard]] T& Get(const NodeId id) { return data[id - 1]; }
	[[nodiscard]] const T& Get(const NodeId id) const { return data[id - 1]; }
	[[nodiscard]] bool Valid(const NodeId id) const { return id != NullNode && id <= static_cast<NodeId>(data.size()); }
	[[nodiscard]] std::span<const NodeId> Children(const NodeId id) const { return nodes[id - 1].children; }
	[[nodiscard]] NodeId Parent(const NodeId id) const { return nodes[id - 1].parent; }
	[[nodiscard]] size_t Size() const { return data.size(); }
};

/// Rooted tree — a directed acyclic Graph with a single root and parent-tracking.
/// Nodes are added bottom-up; each node must declare its parent.
template<typename T>
struct Tree {
	Graph<T> graph;
	NodeId root{NullNode};

	/// Add a node. Pass NullNode (default) to make it the root.
	NodeId AddNode(T node_data, const NodeId parent = NullNode) {
		const NodeId id = graph.AddNode(std::move(node_data));
		if (parent != NullNode) {
			graph.AddEdge(parent, id);
			graph.nodes[id - 1].parent = parent;
		}
		else {
			root = id;
		}
		return id;
	}

	[[nodiscard]] T& Get(const NodeId id) { return graph.Get(id); }
	[[nodiscard]] const T& Get(const NodeId id) const { return graph.Get(id); }
	[[nodiscard]] bool Valid(const NodeId id) const { return graph.Valid(id); }
	[[nodiscard]] NodeId Parent(const NodeId id) const { return graph.Parent(id); }
	[[nodiscard]] std::span<const NodeId> Children(const NodeId id) const { return graph.Children(id); }
	[[nodiscard]] bool IsLeaf(const NodeId id) const { return graph.Children(id).empty(); }
	[[nodiscard]] size_t NodeCount() const { return graph.Size(); }
};

} // namespace engine::graph
