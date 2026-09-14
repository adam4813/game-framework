# Level System (data-driven scene loading)

Builds entity trees from JSON so scene content lives in data, not hardcoded C++. Component names, relationships, and
singletons in the JSON map to loader callbacks in a registry (Strategy/Registry pattern), so adding a loadable component
is a registration — never an edit to a central `if/else`.

## Architecture: Decentralized Loaders

The `LevelModule` is now a **generic registry** that each engine subsystem populates. On import:

1. **LevelModule** imports first and creates the `LevelRegistry` singleton with only the core "transform" loader.
2. **Each engine module** (render, physics, audio, scripting, particles) registers its own component, singleton, link,
   and ref loaders in its constructor.
3. **The game layer** registers game-specific components and relationships.

```cpp
// In render_module.cpp constructor:
engine::level::RegisterComponentLoader(world, "material", [](flecs::entity e, const nlohmann::json& j) {
    e.set<render::Material>({.color = /* ... */});
});
engine::level::RegisterLink(world, "render_with", [](flecs::entity parent, flecs::entity child) {
    parent.add<render::RenderWith>(child);
});
engine::level::RegisterRef(world, "look_at", [](flecs::entity self, flecs::entity target) {
    self.add<render::LookAt>(target);
});
```

## JSON schema

```json
{
  "singletons": {
    "ambient_light": {"color": [90, 105, 130, 255], "intensity": 0.35}
  },
  "entities": [
    {
      "name": "Floor",
      "components": {
        "transform": {"position": [0, 0, 0], "scale": [10, 0.5, 10]},
        "cube": {"size": [1, 1, 1]},
        "rigid_body": {"motion_type": "static", "friction": 0.5},
        "collision_shape": {"type": "box", "box_half_extents": [5, 0.25, 5]}
      },
      "children": [
        {
          "link": "render_with",
          "components": {
            "material": {"color": [235, 240, 235, 255], "cast_shadow": false},
            "albedo": {"path": "textures/checker.png"}
          }
        }
      ]
    },
    {
      "name": "Camera",
      "components": {
        "transform": {"position": [0.0, 5.0, 5.0]},
        "camera": {"fov": 60.0, "aspect_ratio": 1.7777778}
      },
      "refs": {
        "look_at": "Target"
      }
    },
    {
      "name": "Target",
      "components": {
        "transform": {"position": [0, 0, 0]}
      }
    }
  ]
}
```

### Entity structure

- **`components`** — component data to load onto the entity.
- **`children`** — nested child entities.
- **`link`** (on children) — a named relationship to wire to the parent (e.g., `"render_with"` wires
  `RenderWith(parent → child)`). Resolved eagerly during entity building.
- **`refs`** (on entities) — named-entity relationships, resolved after the whole tree is built (e.g.,
  `"look_at": "Player"` wires `LookAt(self → Player)`).

Asset paths (`albedo`, `mesh`, `sound_effect`, `script`, `shader`) are resolved against the platform asset directory
automatically.

`transform` automatically seeds the entity's `WorldTransform` once, so authors never duplicate the values.

## Built-in component loaders

**Core**: `transform`
**Render**: `camera`, `cube`, `sphere`, `quad`, `capsule`, `mesh`, `material`, `albedo`, `shader`, `directional_light`
**Physics**: `rigid_body`, `collision_shape`, `physics_velocity`
**Audio**: `sound_effect`, `music`
**Scripting**: `script`
**Particles**: `particle_emitter`
**Tilemap**: `tilemap`, `grid_position`
**Tags**: Generic `RegisterTag<T>` for any component-like tag (e.g., `"tilemap_follow_target": {}` registers the tag on
the entity).

## Built-in singleton loaders

`ambient_light`, `physics_world`

## Built-in link loaders

`render_with` — wires `RenderWith(parent → child)` so the parent renders with the child's material.

## Built-in ref loaders

`look_at` — wires `LookAt(self → target)` so a camera aims at the target entity.

## Usage

```cpp
world.import<engine::level::LevelModule>();  // Registers core loaders and the registry

// Add a game-specific component loader (e.g., in GameModule constructor)
engine::level::RegisterComponentLoader(world, "my_component", [](flecs::entity e, const nlohmann::json& j) {
    e.set<game::MyComponent>({ .value = j.value("value", 0) });
});

// Instantiate a level file into the world (optionally under a parent entity)
auto level_root = engine::level::LoadLevel(world, platform->AssetDirectory() + "/levels/demo.level.json");
// or:
auto under_scene = engine::level::LoadLevel(world, path, parent_entity);
```

## Registered systems

None — this is a data/service module. It only owns the `LevelRegistry` singleton; all access is through the free
functions above.
