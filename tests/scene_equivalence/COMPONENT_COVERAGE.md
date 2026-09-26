# Scene Equivalence Test — Component Coverage

This file catalogs every ECS component in the engine, its meta-registered fields,
and its coverage status in `scene_equivalence_test`. Update it whenever a module adds
or changes components.

Legend:

- ✅ **Tested** — present in all 3 paths (JSON / AngelScript / C++) with 0 mismatches
- ⚠️ **Partial** — tested in JSON + C++ but not AS (e.g. string-only fields, transient state)
- 🚫 **Skip** — runtime/transient/internal; not meaningful to author-compare
- 🔧 **TODO** — testable but not yet added to the fixture
- 🐛 **Bug** — known meta-registration issue (incorrect offsets or wrong field names)

---

## spatial

| Component        | Meta fields                                 | Status | Notes                                      |
|------------------|---------------------------------------------|--------|--------------------------------------------|
| `Transform`      | position(vec3), rotation(vec3), scale(vec3) | ✅     | Authored transform                         |
| `WorldTransform` | position(vec3), rotation(vec3), scale(vec3) | 🚫     | Derived from Transform; excluded from diff |

---

## render

| Component          | Meta fields                                                                                                               | Status | Notes                                                                                |
|--------------------|---------------------------------------------------------------------------------------------------------------------------|--------|--------------------------------------------------------------------------------------|
| `Camera`           | up(vec3), fov, aspect_ratio, near_plane, far_plane                                                                        | ✅     |                                                                                      |
| `CubePrimitive`    | size(vec3)                                                                                                                | ✅     |                                                                                      |
| `SpherePrimitive`  | radius                                                                                                                    | ✅     |                                                                                      |
| `QuadPrimitive`    | size(vec2)                                                                                                                | ✅     |                                                                                      |
| `CapsulePrimitive` | radius, height                                                                                                            | ✅     |                                                                                      |
| `DirectionalLight` | direction(vec3), color(Rgba), intensity, specular_strength, shininess, casts_shadows, shadow_ground_y, shadow_color(Rgba) | ✅     |                                                                                      |
| `Material`         | color(Rgba), wireframe, cast_shadow                                                                                       | ✅     | Tested standalone; RenderWith relationship not compared                              |
| `MeshPrimitive`    | path(string), handle(int)                                                                                                 | ✅     | handle skipped (runtime); path compared via direct property assignment               |
| `AlbedoMap`        | path(string), handle(int)                                                                                                 | ✅     | handle skipped (runtime); path compared; tested standalone (not as RenderWith child) |
| `SpriteAtlas`      | texture_path(string), image_width_px(int), image_height_px(int)                                                           | ✅     | Atlas entity carries SpriteAtlas, AlbedoMap, Material, and ShaderMap                 |
| `ShaderMap`        | handle(int)                                                                                                               | 🚫     | Only meta field is runtime handle                                                    |
| `DynamicMesh`      | handle(int)                                                                                                               | 🚫     | GPU-uploaded mesh; runtime handle only                                               |
| `AmbientLight`     | color(Rgba), intensity                                                                                                    | 🔧     | **World singleton**, not per-entity — needs `SerializeSingletons()` helper           |
| `RenderWith`       | (relationship)                                                                                                            | 🚫     | Relationship, not a data component                                                   |
| `UseSpriteAtlas`   | (relationship)                                                                                                            | 🚫     | Relationship, not a data component                                                   |
| `LookAt`           | (relationship)                                                                                                            | 🚫     | Relationship, not a data component                                                   |

---

## physics

| Component                 | Meta fields                                             | Status | Notes                                                                 |
|---------------------------|---------------------------------------------------------|--------|-----------------------------------------------------------------------|
| `RigidBody`               | mass, friction, restitution, use_gravity, gravity_scale | ✅     | Partial — motion_type, layer, etc. not in meta                        |
| `CollisionShape`          | offset(vec3)                                            | ✅     | Partial — type, box_half_extents etc. not in meta                     |
| `PhysicsVelocity`         | linear(vec3), angular(vec3)                             | ✅     |                                                                       |
| `PhysicsForce`            | force(vec3), torque(vec3), clear_after_apply            | 🚫     | One-frame transient; consumed and removed each tick                   |
| `PhysicsImpulse`          | impulse(vec3), point(vec3)                              | 🚫     | Transient; consumed and removed each tick                             |
| `PhysicsVelocityOverride` | linear(vec3), angular(vec3)                             | 🚫     | Transient; consumed and removed each tick                             |
| `CollisionEvents`         | (none registered)                                       | 🚫     | Runtime per-entity collision list; populated by JoltModule each frame |
| `IsTrigger`               | (tag)                                                   | 🚫     | Gameplay flag applied by game code; no data to author                 |
| `IsSleeping`              | (tag)                                                   | 🚫     | Runtime Jolt state; set/cleared by physics backend                    |

---

## timer

| Component  | Meta fields                                           | Status | Notes                     |
|------------|-------------------------------------------------------|--------|---------------------------|
| `Timer`    | remaining, duration, repeat, expired                  | ✅     |                           |
| `Cooldown` | remaining, duration                                   | ✅     |                           |
| `Tween`    | elapsed, duration, from, to, easing(int), value, done | ✅     | EasingType exposed as int |

---

## particles

| Component         | Meta fields                                                                                                                | Status | Notes                      |
|-------------------|----------------------------------------------------------------------------------------------------------------------------|--------|----------------------------|
| `ParticleEmitter` | rate, accumulator, particleLifetime, speed, startSize, spread, direction(vec3), colorStart(Rgba), colorEnd(Rgba), emitting | ✅     |                            |
| `Particle`        | velocity(vec3), age, lifetime, startSize, colorStart(Rgba), colorEnd(Rgba)                                                 | 🚫     | Runtime per-particle state |

---

## audio

| Component     | Meta fields                              | Status | Notes                                   |
|---------------|------------------------------------------|--------|-----------------------------------------|
| `SoundEffect` | path(string), handle(int), playing(bool) | ✅     | handle skipped (runtime); path compared |
| `Music`       | path(string), handle(int), loop(bool)    | ✅     | handle skipped (runtime); path compared |

---

## graph

| Component         | Meta fields                | Status | Notes                                                                |
|-------------------|----------------------------|--------|----------------------------------------------------------------------|
| `GraphNode`       | label(string), order(int)  | ✅     | Fixed from sequential → pointer-to-member offset in graph_module.cpp |
| `GraphEdgeWeight` | weight(float), tag(string) | ✅     | Fixed from sequential → pointer-to-member offset in graph_module.cpp |
| `NodeId`          | (value type)               | 🚫     | Scalar wrapper; not used as a standalone entity component in tests   |

---

## ai

| Component        | Meta fields                                                                 | Status | Notes                                                                        |
|------------------|-----------------------------------------------------------------------------|--------|------------------------------------------------------------------------------|
| `Blackboard`     | (none registered)                                                           | 🚫     | `floats`/`bools`/`strings` maps are not meta-reflected; serializes as `null` |
| `BehaviorTree`   | tick_interval, tick_timer                                                   | ✅     | Fixed from sequential → pointer-to-member offset in bt_module.cpp            |
| `FSMAgent`       | current_state(string), previous_state(string), state_elapsed, state_changed | ✅     | Fixed from sequential → pointer-to-member offset in fsm_module.cpp           |
| `GOAPAgent`      | needs_replan, plan_failed                                                   | ✅     | Fixed from sequential → pointer-to-member offset in goap_module.cpp          |
| `SteeringAgent`  | max_speed, max_force, mass                                                  | ✅     | Fixed from sequential → pointer-to-member offset in steering_module.cpp      |
| `SteeringTarget` | position(vec3)                                                              | ✅     | Fixed from sequential → pointer-to-member offset in steering_module.cpp      |
| `UtilityAgent`   | current_action(string)                                                      | ✅     | Fixed from sequential → pointer-to-member offset in utility_module.cpp       |

---

## tilemap

| Component           | Meta fields                                                                                                           | Status | Notes                                                                                 |
|---------------------|-----------------------------------------------------------------------------------------------------------------------|--------|---------------------------------------------------------------------------------------|
| `GridPosition`      | x(int), z(int)                                                                                                        | ✅     |                                                                                       |
| `Tilemap`           | width(int), height(int), tile_size_px(int), pixels_per_world_unit(float), tile_ids(array)                             | 🔧     | Can create entity in all 3 paths but properties not fully exposed to AngelScript      |
| `TilemapSpec`       | width(int), height(int), tile_size_px(int), pixels_per_world_unit(float), fill_id(int), fill_name(string), ops(array) | 🔧     | Instruction set for building Tilemap; needs array meta for TilemapOp                  |
| `TileSet`           | tiles(map), name_to_id(map)                                                                                           | 🔧     | Can create empty registry entity in all 3 paths but fields not exposed to AngelScript |
| `TilemapViewport`   | buffer_width(int), buffer_height(int), origin_tile_x(int), origin_tile_z(int)                                         | ✅     | Optional viewport streaming configuration; tested in JSON/C++                         |
| `TileCallbackState` | current_tile_x(int), current_tile_z(int)                                                                              | 🚫     | Runtime state; set by the tilemap system, not authored                                |
| `TileDescriptor`    | id(int), name(string), color(vec4), tex_coords(Rect), walkable(bool)                                                  | 🚫     | A registry entry, not a per-entity component; part of TileSet                         |
| `TilemapMeshData`   | (none registered)                                                                                                     | 🚫     | Internal CPU-side mesh buffer; cleared after GPU upload                               |

---

## ui

`UIModule` is imported by `EngineContext`, so UI components are registered in `TestWorld`. The sixteen
script-registered components below are authored in all three paths (`TestUIPanel`/`TestUIButton`/
`TestUIProgressBar`/`TestUISpinner`/`TestUIStack`/`TestUIScrollRect`/`TestUIModal`/`TestUILabel`/
`TestUISlider`/`TestUICheckbox`/`TestUIRadio`/`TestUITextInput`/`TestUITextArea`/`TestUIDropdown`). Runtime
fields (`hovered`/`pressed`/`clicked`/`phase`/`scroll`/`content_size`/`dragging_*`/`caret_blink`/`hover_index`) are
left at defaults — no path runs systems, so they match. Each widget's unreflected members (`std::function`
callbacks, `Dropdown.options` vector, `Stack.direction`/`Label.align` enums) are automatically skipped by the
script and JSON layers, so only the plain-data fields are compared.

| Component     | Meta fields                                                                                                                                     | Status | Notes                                                           |
|---------------|-------------------------------------------------------------------------------------------------------------------------------------------------|--------|-----------------------------------------------------------------|
| `UIRect`      | rect(Rect)                                                                                                                                      | ✅     | `core::Rect` value type; set via `.rect.x` etc. in AS           |
| `UIElement`   | z_index(int), visible(bool)                                                                                                                     | ✅     |                                                                 |
| `Panel`       | color(Rgba), border_color(Rgba), border_thickness, roundness                                                                                    | ✅     | `texture` (int handle) not meta-reflected                       |
| `Button`      | label(string), font_size, normal/hover/pressed/border/text colors(Rgba), roundness, hovered, pressed, clicked                                   | ✅     | `hovered`/`pressed`/`clicked` runtime; default-match            |
| `ProgressBar` | value, min, max, fill_color/track_color/border_color(Rgba), border_thickness, roundness                                                         | ✅     |                                                                 |
| `Spinner`     | color(Rgba), radius, dot_radius, dots(int), speed, phase                                                                                        | ✅     | `phase` runtime; default-match (no systems run)                 |
| `Stack`       | spacing, padding, stretch                                                                                                                       | ✅     | `direction` enum not meta-reflected                             |
| `ScrollRect`  | scroll(vec2), content_size(vec2), vertical, horizontal, scrollbar_thickness, wheel_speed, track_color/thumb_color(Rgba), dragging_v, dragging_h | ✅     | `scroll`/`content_size`/`dragging_*` runtime; default-match     |
| `Modal`       | open(bool), backdrop_color(Rgba), close_on_backdrop                                                                                             | ✅     |                                                                 |
| `Label`       | text(string), font_size, color(Rgba), align(TextAlign)                                                                                          | ✅     | `align` enum skipped (left default); text/font_size/color       |
| `Slider`      | value, min, max, step, hovered, dragging                                                                                                        | ✅     | `std::function on_change` skipped; `hovered`/`dragging` runtime |
| `Checkbox`    | checked, label(string), font_size, box_size, colors(Rgba), roundness, hovered                                                                   | ✅     | `std::function on_change` skipped; colors left default          |
| `Radio`       | group(string), value(string), selected, label(string), font_size, radius, colors(Rgba), hovered                                                 | ✅     | `std::function on_change` skipped; colors left default          |
| `TextInput`   | text(string), placeholder(string), max_length, font_size, colors(Rgba), roundness, hovered, caret_blink                                         | ✅     | `std::function` callbacks skipped; `caret_blink` runtime        |
| `TextArea`    | text(string), placeholder(string), max_length, font_size, line_spacing, colors(Rgba), roundness, hovered, caret_blink                           | ✅     | `std::function on_change` skipped; `caret_blink` runtime        |
| `Dropdown`    | selected(int), placeholder(string), open, font_size, row_height, colors(Rgba), roundness, hovered, hover_index                                  | ✅     | `options`(vector) + `std::function` skipped; not compared       |
| `DataBinding` | (std::function + entity handles)                                                                                                                | 🚫     | Binding wiring (callbacks/entity refs); not author-compared     |
| `UIFocus`     | (singleton; flecs::entity)                                                                                                                      | 🚫     | Runtime keyboard-focus handle; not authored                     |

---

## save *(not imported in TestWorld)*

| Component             | Meta fields         | Status | Notes                                           |
|-----------------------|---------------------|--------|-------------------------------------------------|
| `SaveField`           | (function pointers) | 🚫     | Contains `std::function`; not meta-serializable |
| `EntityMemberBinding` | (function pointers) | 🚫     | Contains `std::function`                        |
| `EntitySaveType`      | (function pointers) | 🚫     | Contains `std::function`                        |

---

## input *(not imported in TestWorld)*

| Component    | Meta fields                    | Status | Notes                                         |
|--------------|--------------------------------|--------|-----------------------------------------------|
| `InputState` | (complex keyboard/mouse state) | 🚫     | Runtime; populated each frame by the platform |

---

## pathfinding

| Component        | Meta fields                             | Status | Notes                                                                  |
|------------------|-----------------------------------------|--------|------------------------------------------------------------------------|
| `PathRequest`    | goal(vec3), algorithm(string)           | 🚫     | Transient; consumed and removed by PathRequestSystem each frame        |
| `PathAgent`      | arrival_radius(float)                   | ✅     | waypoints/current_waypoint/reached_goal are runtime state              |
| `PathFollow`     | speed(float)                            | ✅     | mode (enum) and desired_velocity skipped; only authored field is speed |
| `PathAutoReplan` | goal(vec3), algorithm(string), interval | ✅     | timer is runtime state                                                 |
| `PathFollowMode` | SteeringTarget/Kinematic/Direct (enum)  | 🚫     | Enum constants; no per-entity component                                |

---

## Known Bugs

*(None currently known.)*

---

## Adding a New Module to the Test

When you add a new engine module via the `create-module` skill:

1. Add `world.import<engine::your_module::YourModule>()` to `TestWorld` in `main.cpp`.
2. For each **authored** component with meta-registered fields:

- Add a `TestYourComponent` entity in `BuildCppWorld()`.
- Add the equivalent JSON entity to `kTestSceneJson` in `scene_data.hpp`.
- Add the equivalent AngelScript entity setup to `kTestSceneScript` in `scene_data.hpp`.

3. **Use pointer-to-member syntax** for all `.member()` calls when the component has any struct fields that are NOT
   registered:
   ```cpp
   // CORRECT — safe even if unregistered fields precede the registered ones
   world.component<MyComp>()
       .member("field_a", &MyComp::field_a)
       .member("field_b", &MyComp::field_b);

   // WRONG — sequential offset tracking breaks when earlier fields are skipped
   world.component<MyComp>()
       .member<float>("field_a")   // assumes field_a is at offset 0
       .member<float>("field_b");
   ```
4. Update this file.
5. Run `scene_equivalence_test` — it must exit 0 with 0 mismatches.
