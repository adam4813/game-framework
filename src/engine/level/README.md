# Level System (data-driven scene loading)

Builds entity trees from JSON so scene content lives in data, not hardcoded C++. Component names in the JSON map to
loader callbacks in a registry (Strategy/Registry pattern), so adding a loadable component is a registration — never an
edit to a central `if/else`.

> **Status:** the loader framework is complete and hand-authorable. Full migration of the runtime
> scenes (e.g. `game_scene.cpp`) is deferred until there is a scene editor to author and verify
> level files. See `plan/data-driven-levels.md` for the schema, roadmap, and editor dependency.

## Structure

- **level_components.hpp** — `ComponentLoader` / `SingletonLoader` aliases and the `LevelRegistry`
  singleton.
- **level_module.hpp/cpp** — `LevelModule` (registers built-in loaders) plus `RegisterComponentLoader`
  / `RegisterSingletonLoader` / `LoadLevel`.
- **level.hpp** — public umbrella header.

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
        "material": {"color": [235, 240, 235, 255], "cast_shadow": false},
        "albedo": {"path": "textures/checker.png"},
        "rigid_body": {"motion_type": "static", "friction": 0.5, "restitution": 0.0, "use_gravity": false},
        "collision_shape": {"type": "box", "box_half_extents": [5, 0.25, 5]}
      },
      "children": [
        {"name": "FloorScript", "components": {"script": {"source": "scripts/foo.as"}}}
      ]
    }
  ]
}
```

- Asset paths (`albedo`, `mesh`, `sound_effect`, `script`) are resolved against the platform asset directory
  automatically.
- `transform` also computes the `WorldTransform`, so authors never duplicate the values.

## Built-in component loaders

`transform`, `camera`, `cube`, `sphere`, `quad`, `capsule`, `mesh`, `material`, `albedo`,
`directional_light`, `rigid_body`, `collision_shape`, `physics_velocity`, `sound_effect`, `script`.

## Built-in singleton loaders

`ambient_light`, `physics_world`.

## Usage

```cpp
world.import<engine::level::LevelModule>();

// Add a game-specific component loader.
engine::level::RegisterComponentLoader(world, "object", [](flecs::entity e, const nlohmann::json& j) {
    e.set<game::Object>({ .score = j.value("score", 100) });
});

// Instantiate a level file.
engine::level::LoadLevel(world, platform->AssetDirectory() + "/levels/demo.level.json");
```

## Registered systems

None — this is a data/service module. It only owns the `LevelRegistry` singleton; all access is through the free
functions above.
