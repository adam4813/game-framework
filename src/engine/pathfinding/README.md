# Pathfinding Module

Navigation graph abstraction and pluggable pathfinding algorithms for 2D/3D worlds.
Decouples algorithm logic from world representation via an `INavGraph` interface so systems
never know about tiles, navmeshes, or geometry.

## Architecture

```
INavGraph               — abstract nav graph (Position, Locate, Neighbors, Heuristic, Valid)
INavAlgorithm           — abstract algorithm (stateless FindPath → NavPath)
PathfinderRegistry      — Flecs singleton: named algorithm instances (like NoiseRegistry)
NavEnvironment          — Flecs singleton: active INavGraph for the current scene
GridNavGraph            — concrete INavGraph for flat/heightmap tile grids (X/Z plane)
BuildNavGraphFromTilemap— tilemap adapter: Tilemap + TileSet → GridNavGraph
PathRequest             — ECS component: one-shot "find path to this goal" trigger. Enable the component to trigger a pathfinding request. The system will disable the component after processing.
PathAgent               — ECS component: computed path + follow state
PathFollow              — ECS component: movement output mode (SteeringTarget/Kinematic/Direct)
PathAutoReplan          — ECS component: periodic replan (set goal each frame for chasing)
PathfindingModule       — Flecs module; registers all of the above
BuiltinPathfindingModule— registers "astar" and "dijkstra" into PathfinderRegistry
```

## Available Algorithms

| Name         | Class             | Notes                                       |
|--------------|-------------------|---------------------------------------------|
| `"astar"`    | AStarAlgorithm    | Heuristic search; optimal + fast on grids   |
| `"dijkstra"` | DijkstraAlgorithm | Uniform-cost; optimal for arbitrary weights |

## C++ Usage

### Register a navigation environment from a tilemap

```cpp
#include "engine/tilemap/tilemap_nav_adapter.hpp"

// In a scene load observer or system:
const auto& tilemap = tilemap_entity.get<engine::tilemap::Tilemap>();
const auto& tileset = world.get<engine::tilemap::TileSet>();
const glm::vec3 origin = tilemap_entity.get<engine::spatial::WorldTransform>().position;

world.get_mut<engine::pathfinding::NavEnvironment>().graph =
    engine::tilemap::BuildNavGraphFromTilemap(tilemap, tileset, origin);
```

### Request a one-shot path

```cpp
// Entity must have a WorldTransform so the system knows where it starts.
enemy_entity.set<engine::pathfinding::PathRequest>({.goal = player_pos});
enemy_entity.set<engine::pathfinding::PathFollow>({.mode = PathFollowMode::SteeringTarget});
```

### Chase a moving target (periodic replan)

```cpp
enemy_entity.set<engine::pathfinding::PathAutoReplan>({.interval = 0.5f});
enemy_entity.set<engine::pathfinding::PathFollow>({.mode = PathFollowMode::Kinematic, .speed = 4.0f});

// In a per-frame system, update the replan goal:
enemy_entity.get_mut<engine::pathfinding::PathAutoReplan>().goal =
    player_entity.get<engine::spatial::WorldTransform>().position;
```

### Register a custom navigation graph

```cpp
struct MyNavGraph : engine::pathfinding::INavGraph {
    glm::vec3 Position(graph::NodeId id) const override { ... }
    graph::NodeId Locate(glm::vec3 pos) const override { ... }
    std::vector<NavEdge> Neighbors(graph::NodeId from) const override { ... }
    float Heuristic(graph::NodeId from, graph::NodeId goal) const override { ... }
    bool Valid(graph::NodeId id) const override { ... }
    size_t NodeCount() const override { ... }
};

world.get_mut<engine::pathfinding::NavEnvironment>().graph = std::make_unique<MyNavGraph>(...);
```

### Register a custom algorithm

```cpp
struct MyAlgorithm : engine::pathfinding::INavAlgorithm {
    engine::pathfinding::NavPath FindPath(
        const INavGraph& g, graph::NodeId start, graph::NodeId goal) const override { ... }
};

world.get_mut<engine::pathfinding::PathfinderRegistry>()
    .Register("my_algo", std::make_unique<MyAlgorithm>());
```

## JSON scene authoring

```json
{
  "name": "Enemy",
  "components": {
    "path_follow": {"mode": "kinematic", "speed": 3.0},
    "path_auto_replan": {"goal": [10.0, 0.0, 5.0], "interval": 0.5}
  }
}
```

## GridNavGraph coordinate system

- Grid lives on the X/Z plane (Y up).
- Cell (x, z) → `NodeId = z * width + x + 1` (1-based).
- World position of cell centre: `origin + (x+0.5, height_at(x,z), z+0.5) * cell_size`.
- Octile-distance heuristic gives optimal A* behaviour on 8-neighbour grids.
- Diagonal moves are blocked when either cardinal neighbour is unwalkable (no corner-cutting).

## Systems registered

| System               | Kind     | Query                                                  | Notes                             |
|----------------------|----------|--------------------------------------------------------|-----------------------------------|
| PathAutoReplanSystem | OnUpdate | PathAutoReplan                                         | Issues PathRequest on timer       |
| PathRequestSystem    | OnUpdate | const PathRequest, const WorldTransform                | Writes PathAgent, removes request |
| PathFollowSystem     | OnUpdate | PathAgent, PathFollow, const WorldTransform, Transform | Produces movement output          |

All systems carry `ecs::Pausable`.
