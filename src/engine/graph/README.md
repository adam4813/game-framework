# Graph Module

Generic directed-graph and rooted-tree data structures for Flecs ECS.
Provides both **C++ value types** (for embedded use inside other components, e.g. BehaviorTree)
and **ECS entity components** (for composable, query-able graph topology visible in the Explorer).

---

## Structure

| File                    | Purpose                                                                          |
|-------------------------|----------------------------------------------------------------------------------|
| `graph_types.hpp`       | `NodeId`, `Graph<T>`, `Tree<T>` — C++ value types                                |
| `graph_components.hpp`  | `GraphNode`, `GraphRoot`, `GraphEdge`, `GraphEdgeWeight` — ECS components        |
| `graph_json.hpp`        | `LoadTreeFromJson`, `LoadTreeFromFile` — helpers for loading `Tree<T>` from JSON |
| `graph_module.hpp/.cpp` | Flecs module: registers all components, scripting, and level loaders             |
| `graph.hpp`             | Umbrella header                                                                  |

---

## C++ value types

For embedded data inside another component (e.g. `BehaviorTree` owns a `Tree<BTNodeDef>`).

```cpp
engine::graph::Tree<MyNodeData> tree;
const auto root  = tree.AddNode({/* data */});
const auto child = tree.AddNode({/* data */}, root);
for (const auto cid : tree.Children(root)) { auto& data = tree.Get(cid); }
```

---

## ECS entity components

For general-purpose query-able graphs where each node is a Flecs entity.
Use for dialog trees, skill graphs, mission DAGs, etc.

```cpp
// Build
e.set<GraphNode>({.id = 1, .label = "Hello", .order = 0});
parent.add<GraphEdge>(child);
child.set<GraphEdgeWeight>({.tag = "yes"});

// Traverse
parent.each<GraphEdge>([](flecs::entity child) { /* ... */ });
```

JSON — inline entity hierarchy (game-specific components like `dialog_line` registered separately):

```json
{
  "name": "DialogRoot",
  "components": {"graph_root": {}, "dialog_line": {"text": "Hello!"}},
  "children": [
    {
      "name": "Choice1", "link": "graph_edge",
      "components": {
        "graph_node": {"id": 2, "label": "Ask quest", "order": 0},
        "graph_edge_weight": {"tag": "choice_0"},
        "dialog_line": {"text": "What quests are available?"}
      }
    }
  ]
}
```

---

## Registered loaders

| Key                   | Type      | Description                                   |
|-----------------------|-----------|-----------------------------------------------|
| `"graph_root"`        | Component | Adds `GraphRoot` tag                          |
| `"graph_node"`        | Component | Sets `GraphNode` (id, label, order)           |
| `"graph_edge_weight"` | Component | Sets `GraphEdgeWeight` (weight, tag)          |
| `"graph_node_id"`     | Component | Sets bare `NodeId`                            |
| `"graph_edge"`        | Link      | Adds `GraphEdge(parent → child)` relationship |

---

## Consumers

- `engine/ai/bt/` — uses `Tree<BTNodeDef>` inside `BehaviorTree` (value-type pattern)
- Game code — dialog trees, skill graphs, mission DAGs (entity-component pattern)
