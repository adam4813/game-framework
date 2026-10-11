#pragma once

#include <fstream>
#include <functional>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "graph_types.hpp"

namespace engine::graph {

/// Parse a JSON tree document into a Tree<T>.
///
/// Expected JSON shape:
/// ```json
/// {
///   "nodes": [
///     { "id": 1, "data": { ... } },          // root (no "parent")
///     { "id": 2, "parent": 1, "data": { ... } }
///   ]
/// }
/// ```
/// Nodes must be listed in topological order (parent before child).
/// `converter` maps the node's "data" sub-object to T; if "data" is absent the
/// whole node JSON object is passed instead. Returns an empty Tree on any parse error.
template<typename T>
[[nodiscard]] Tree<T>
LoadTreeFromJson(const nlohmann::json& doc, const std::function<T(const nlohmann::json&)>& converter) {
	Tree<T> tree;
	if (!doc.contains("nodes") || !doc.at("nodes").is_array()) {
		spdlog::error("[Graph] LoadTreeFromJson: missing or non-array 'nodes' key");
		return tree;
	}
	for (const auto& jnode : doc.at("nodes")) {
		const NodeId id = jnode.value("id", NullNode);
		if (id == NullNode) {
			spdlog::warn("[Graph] LoadTreeFromJson: node missing 'id', skipped");
			continue;
		}
		const NodeId parent = jnode.value("parent", NullNode);
		const nlohmann::json& data = jnode.contains("data") ? jnode.at("data") : jnode;
		tree.AddNode(converter(data), parent);
	}
	return tree;
}

/// Convenience overload: parse a tree document loaded from a file path.
template<typename T>
[[nodiscard]] Tree<T> LoadTreeFromFile(
	const std::string_view path,
	const std::string_view log_tag,
	const std::function<T(const nlohmann::json&)>& converter
) {
	std::ifstream file{std::string{path}};
	if (!file) {
		spdlog::error("[{}] Cannot open tree file '{}'", log_tag, path);
		return {};
	}
	try {
		nlohmann::json doc;
		file >> doc;
		return LoadTreeFromJson<T>(doc, converter);
	}
	catch (const std::exception& ex) {
		spdlog::error("[{}] Failed to parse tree file '{}': {}", log_tag, path, ex.what());
		return {};
	}
}

} // namespace engine::graph
