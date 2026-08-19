# Data-Driven Levels — Design & Roadmap

Status: **loader framework implemented; runtime scene migration deferred (needs a scene editor).**

This document captures the plan for moving scene content out of hardcoded C++ (`game_scene.cpp`
`SetupPhysicsDemo`) and into JSON data, per the data-driven design principle in the engine instructions.

## Why deferred

The `engine::level` loader framework is complete and can instantiate a full scene from JSON (see
`assets/levels/demo.level.json`, which reproduces the current physics demo). However, **hand-editing JSON is not a
viable authoring workflow** for real levels — there is no scene editor yet to place entities, tune transforms, and
preview lighting/physics. Ripping the working C++ scene out now would trade a debuggable, IDE-assisted setup for
error-prone hand-authored JSON with no visual feedback.

So the framework lands now (unblocking future data-driven work and the save system), but the actual
`CubeScene` migration waits until the editor exists.

## What exists today (`src/engine/level/`)

- `LevelRegistry` singleton: `name -> ComponentLoader` and `name -> SingletonLoader` maps.
- Built-in loaders for the engine components used by scenes: `transform`, `camera`, `cube`, `sphere`,
  `quad`, `capsule`, `mesh`, `material`, `albedo`, `directional_light`, `rigid_body`,
  `collision_shape`, `physics_velocity`, `sound_effect`, `script`; singletons `ambient_light`,
  `physics_world`.
- `LoadLevel(world, path)` — parses JSON, applies singletons, builds the entity tree (with
  `children` and named entities). Asset paths resolve against the platform asset directory.
- `RegisterComponentLoader` / `RegisterSingletonLoader` — the game layer adds its own component loaders (e.g. `coin`,
  `spawner`) without touching the engine (Strategy/Registry pattern).

## JSON schema (v0)

```jsonc
{
  "singletons": { "ambient_light": { "color": [r,g,b,a], "intensity": 0.35 } },
  "entities": [
    {
      "name": "Floor",                       // optional; enables lookup/destruct by name
      "components": {                         // key = registered loader name
        "transform": { "position": [x,y,z], "rotation": [x,y,z], "scale": [x,y,z] },
        "cube": { "size": [x,y,z] },
        "material": { "color": [r,g,b,a], "cast_shadow": false },
        "rigid_body": { "motion_type": "static|kinematic|dynamic", ... },
        "collision_shape": { "type": "box|sphere|capsule|cylinder", ... }
      },
      "children": [ { "name": "...", "components": { ... } } ]
    }
  ]
}
```

Notes:

- `transform` also computes `WorldTransform`, so authors never duplicate values.
- Component set order within an entity is irrelevant: the physics sync observer fires once the
  `WorldTransform` + `RigidBody` + `CollisionShape` trio is present.

## Roadmap

1. **Scene editor (prerequisite).** In-game ImGui editor (imgui is already a dependency but not yet integrated — see the
   "missing modules" review): entity list, component inspectors, transform gizmos, load/save of the level JSON. This is
   the gating item.
2. **Round-trip save.** A `SaveLevel(world, path)` that serializes the live entity tree back to the same schema (pairs
   naturally with the in-progress JSON save system and Flecs meta reflection — most components are already reflected, so
   a generic serializer is feasible).
3. **Migrate `CubeScene`.** Replace `SetupPhysicsDemo` with `level::LoadLevel(world,
   assetDir + "/levels/demo.level.json")`; keep `CleanupPhysicsDemo` (name-based destruct still works) or generalize it
   to destruct everything the load created (tag loaded roots with a
   `LevelRoot` component and destruct by query on unload).
4. **Game component loaders.** Register game-specific loaders (`drain`, `spawner`, `modifier`) so state is fully
   data-authored.
5. **Variety / levels.** Multiple `*.level.json` level layouts selected by chapter/run state (ties into
   `engine::gameflow`), seeded modifiers layered on load.

## Open questions

- Rotation units in JSON: radians (engine-native) vs degrees (author-friendly). Lean degrees at the loader boundary once
  the editor exists.
- Prefabs / templates: Flecs prefabs could back reusable entity archetypes referenced by name in the JSON
  (`"prefab": "Object"`).
- Unload strategy: name-based destruct vs a `LevelRoot` tag + query destruct (preferred once scenes create many
  entities).
