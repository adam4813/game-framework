#pragma once

#include <flecs.h>

namespace engine::graph {

/// Graph and tree data-structure module.
/// Provides NodeId, Graph<T>, and Tree<T> as C++ value types for use by other
/// modules (e.g. the AI module's behavior trees).
/// Registers:
///   - NodeId type with Flecs meta and scripting
///   - "graph_node_id" level component loader
class GraphModule {
public:
	explicit GraphModule(const flecs::world& world);
};

} // namespace engine::graph
