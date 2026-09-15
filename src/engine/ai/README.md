# AI Module

General-purpose AI subsystems for Flecs ECS. Imported via `world.import<ai::AIModule>()`.

---

## Structure

| File                      | Purpose                                                                  |
|---------------------------|--------------------------------------------------------------------------|
| `ai_module.hpp/.cpp`      | Module class; registers all singletons and systems                       |
| `ai_components.hpp`       | Shared `Blackboard` component (floats, bools, strings key-value store)   |
| `bt_components.hpp`       | Behavior Tree — `BehaviorTree`, `BTHandlerRegistry`                      |
| `goap_components.hpp`     | GOAP — `GOAPAgent`, `GOAPActionDef`, `GOAPGoal`, `GOAPActionRegistry`    |
| `fsm_components.hpp`      | Finite State Machine — `FSMAgent`, `FSMStateDef`, `FSMTransition`        |
| `utility_components.hpp`  | Utility AI — `UtilityAgent`, `UtilityActionDef`, `UtilityActionRegistry` |
| `steering_components.hpp` | Steering — `SteeringAgent`, `SteeringBehavior`, `SteeringTarget`         |
| `ai.hpp`                  | Umbrella header                                                          |

Depends on the **graph module** (`engine/graph/`) for the tree data structure used by Behavior Trees.

---

## Quick-start per pattern

### Behavior Tree

```cpp
// 1. Register leaf handlers once (e.g. in your scene's Load())
world.get_mut<ai::BTHandlerRegistry>().actions["MoveToTarget"] =
    [](flecs::entity e, ai::Blackboard& bb, float dt) {
        // ... move logic ...
        return ai::BTStatus::Running;
    };
world.get_mut<ai::BTHandlerRegistry>().conditions["CanSeePlayer"] =
    [](flecs::entity e, const ai::Blackboard& bb) { return /* ... */; };

// 2. Build tree and attach components
ai::BehaviorTree bt;
const auto root = bt.tree.AddNode(ai::BTNodeDef{.type = ai::BTNodeType::Sequence});
bt.tree.AddNode(ai::BTNodeDef{.type = ai::BTNodeType::Condition, .handler_key = "CanSeePlayer"}, root);
bt.tree.AddNode(ai::BTNodeDef{.type = ai::BTNodeType::Action, .handler_key = "MoveToTarget"}, root);

flecs::entity enemy = world.entity("Enemy");
enemy.set<ai::BehaviorTree>(bt);
enemy.set<ai::Blackboard>({});
```

### GOAP

```cpp
// 1. Describe actions, goals, and initial world state
ai::GOAPAgent agent;
agent.available_actions.push_back({
    .name = "PickUpWeapon",
    .preconditions = {{"has_weapon", false}},
    .effects       = {{"has_weapon", true}},
    .cost = 1.0F,
});
agent.goals.push_back({.name = "BeArmed", .desired_state = {{"has_weapon", true}}, .priority = 1.0F});
agent.world_state["has_weapon"] = false;
agent.needs_replan = true;
e.set<ai::GOAPAgent>(agent);

// 2. Register action executors
world.get_mut<ai::GOAPActionRegistry>().handlers["PickUpWeapon"] = {
    .on_start = [](flecs::entity e, ai::GOAPAgent&) { return true; },
    .on_tick  = [](flecs::entity e, ai::GOAPAgent&, float) { return true; }, // instant
};
```

### FSM

```cpp
ai::FSMAgent fsm;
fsm.states.push_back({.name = "Idle", .transitions = {
    {.condition = "enemy_nearby", .expected = true, .target = "Chase"},
}});
fsm.states.push_back({.name = "Chase", .max_duration = 10.0F, .timeout_target = "Idle"});
fsm.current_state = "Idle";
e.set<ai::FSMAgent>(fsm);

// Each frame, set conditions:
e.get_mut<ai::FSMAgent>().conditions["enemy_nearby"] = DetectEnemy(e);
```

### Utility AI

```cpp
ai::UtilityAgent ua;
ua.actions.push_back({
    .name = "AttackPlayer",
    .considerations = {{.context_key = "health", .min_value = 0, .max_value = 100, .weight = 1.0F}},
});
e.set<ai::UtilityAgent>(ua);

world.get_mut<ai::UtilityActionRegistry>().handlers["AttackPlayer"] =
    [](flecs::entity e, ai::UtilityAgent& ua, float dt) { /* ... */ };

// Each frame, update context:
e.get_mut<ai::UtilityAgent>().context["health"] = GetHealth(e);
```

### Steering

```cpp
ai::SteeringAgent sa;
sa.behaviors.push_back({.type = ai::SteeringBehaviorType::Arrive, .slow_radius = 3.0F});
sa.max_speed = 5.0F;
e.set<ai::SteeringAgent>(sa);
e.set<ai::SteeringTarget>({.position = target_position}); // update each frame
```

---

## Registered systems

| System           | Phase      | Tag                     | Description                                                       |
|------------------|------------|-------------------------|-------------------------------------------------------------------|
| `BTExecution`    | `OnUpdate` | `Pausable`, `GameScene` | Ticks all `BehaviorTree + Blackboard` entities                    |
| `GOAPPlanner`    | `OnUpdate` | `Pausable`, `GameScene` | Builds a plan when `needs_replan` is set                          |
| `GOAPExecutor`   | `OnUpdate` | `Pausable`, `GameScene` | Ticks the current GOAP action and advances the plan               |
| `FSMUpdate`      | `OnUpdate` | `Pausable`, `GameScene` | Evaluates FSM transitions and handles timeouts                    |
| `UtilityEval`    | `OnUpdate` | `Pausable`, `GameScene` | Scores utility actions and selects the best                       |
| `UtilityExecute` | `OnUpdate` | `Pausable`, `GameScene` | Calls the registered executor for the selected action             |
| `SteeringSystem` | `OnUpdate` | `Pausable`, `GameScene` | Computes steering forces and integrates velocity into `Transform` |

---

## Known limitations

### Steering flocking is O (n²)

`Separation`, `Cohesion`, and `Alignment` behaviors each run a full neighbor query over all
`SteeringAgent + WorldTransform` entities per agent per frame. For small NPC counts (< ~50) this is fine.
For larger crowds, mitigate by:

1. **Lower-frequency update**: cache per-agent neighbor results in `SteeringAgent` and refresh only every
   N frames (e.g. `eval_interval` pattern from UtilityAgent).
2. **Spatial hash/grid**: bucket agents into coarse grid cells so the neighbor query is O (1) per bucket
   rather than O (n). The `engine/spatial` module's grid is a candidate host.
3. **Crowd simulation / RVO** (see below): replaces the flocking model entirely for large agent counts.

### Wander is deterministic

The `Wander` behavior applies a **fixed** angular rotation (`wander_jitter` rad/s) to `wander_target`
each frame — agents with the same initial state trace identical arcs. If you need divergent wandering,
draw a random angle from `world.get_mut<ecs::RngState>()` and add it inside the `Wander()` call.

---

## Notes on genre-specific / niche AI patterns

The following patterns are **not implemented** in this module but are noted here for reference.
They would be appropriate additions for specific game types:

| Pattern                              | Genre fit                        | Notes                                                                                            |
|--------------------------------------|----------------------------------|--------------------------------------------------------------------------------------------------|
| **A\* / Navmesh pathfinding**        | Any game with navigation         | Belongs in a dedicated spatial/navigation module; `SteeringAgent` can consume the resulting path |
| **Influence maps**                   | RTS, tower defense               | Spatial heatmaps for threat/resources; efficient with the spatial module's grid                  |
| **HTN (Hierarchical Task Networks)** | Complex military/strategy sims   | More expressive than GOAP; higher authoring cost                                                 |
| **MCTS (Monte Carlo Tree Search)**   | Turn-based strategy, board games | Sampling-based; uses `engine/graph` trees at runtime                                             |
| **Blackboard architecture**          | Large teams with many AI agents  | Shared read/write store for inter-agent communication; could extend `Blackboard`                 |
| **Neural network / ML inference**    | Racing, adaptive difficulty      | Requires an inference runtime (e.g. ONNX); not suitable for a game-framework module              |
| **Crowd simulation / RVO**           | Open-world, city builders        | Reciprocal Velocity Obstacles for large agent counts; replaces the simple O(n²) flocking here    |
| **Emotion / personality models**     | RPG, narrative                   | Layered on top of Utility AI or GOAP world-state facts                                           |
