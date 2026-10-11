# Script API Reference

Complete catalogue of types, globals, component accessors, methods, and constants exposed to
AngelScript by the engine. All registrations are backend-agnostic; the AngelScript backend maps
them to its own type system automatically.

See [scripting-api.md](scripting-api.md) for the registration conventions and lifecycle.

---

## Math / Core (`core_module.cpp`)

### Value types

| Type   | Fields             | Constructor                                |
|--------|--------------------|--------------------------------------------|
| `vec2` | `float x, y`       | `vec2(float x, float y)`                   |
| `vec3` | `float x, y, z`    | `vec3(float x, float y, float z)`          |
| `vec4` | `float x, y, z, w` | `vec4(float x, float y, float z, float w)` |
| `Rgba` | `uint8 r, g, b, a` | —                                          |
| `Rect` | `float x, y, w, h` | —                                          |

### Constants

| Constant       | Value            |
|----------------|------------------|
| `Vec3_Right`   | `vec3(1, 0, 0)`  |
| `Vec3_Up`      | `vec3(0, 1, 0)`  |
| `Vec3_Forward` | `vec3(0, 0, -1)` |

---

## Input (`input_module.cpp`)

### Singleton getter

| Global            | Returns      | Description                            |
|-------------------|--------------|----------------------------------------|
| `GetInputState()` | `InputState` | Snapshot of keyboard/mouse this frame. |

### Methods on `InputState`

| Method                   | Returns | Description                     |
|--------------------------|---------|---------------------------------|
| `IsKeyDown(int key)`     | `bool`  | Key held down this frame.       |
| `WasKeyPressed(int key)` | `bool`  | Key just pressed (rising edge). |
| `IsKeyUp(int key)`       | `bool`  | Key just released.              |

### Key constants (`Key_*`)

All keyboard codes are available as `Key_<Name>` int constants, e.g. `Key_Space`, `Key_Return`,
`Key_W`, `Key_F1`, `Key_LeftShift`, `Key_NumPad0`. See `input_module.cpp` for the full list.

---

## Spatial (`spatial_module.cpp`)

### Components (via `GetTransform()` / `MutTransform()` etc.)

| Component        | Fields                                        |
|------------------|-----------------------------------------------|
| `Transform`      | `vec3 position, rotation, scale`              |
| `WorldTransform` | `vec3 position, rotation, scale; mat4 matrix` |

---

## Audio (`audio_module.cpp`)

### Components

| Component     | Key fields                                    |
|---------------|-----------------------------------------------|
| `SoundEffect` | `string path`, `bool playing`                 |
| `Music`       | `string path`, `bool playing`, `bool looping` |

### Methods on `SoundEffect`

| Method   | Returns | Description                                  |
|----------|---------|----------------------------------------------|
| `Fire()` | `void`  | Play the sound once (sets `playing = true`). |

### Globals

| Global                   | Returns | Description                                       |
|--------------------------|---------|---------------------------------------------------|
| `PlaySoundHandle(int h)` | `void`  | Play a loaded sound by its asset handle directly. |

---

## Physics (`physics_module.cpp`)

### Components (via standard GetT/MutT/AddT)

`PhysicsImpulse`, `RigidBody`, `CollisionShape`, `PhysicsVelocity`, `PhysicsForce`, `PhysicsVelocityOverride`

---

## Scene (`scene_module.cpp`)

### Globals

| Global          | Returns | Description                                     |
|-----------------|---------|-------------------------------------------------|
| `PauseScene()`  | `void`  | Pause the active scene (adds `Paused` tag).     |
| `ResumeScene()` | `void`  | Resume the active scene (removes `Paused` tag). |

---

## Timer (`timer_module.cpp`)

### Components (via GetTimer/MutTimer/AddTimer etc.)

| Component  | Key fields                                                        |
|------------|-------------------------------------------------------------------|
| `Timer`    | `float duration, remaining; bool expired, repeat`                 |
| `Cooldown` | `float duration, remaining`                                       |
| `Tween`    | `float from, to, duration, elapsed, value; bool done; int easing` |

### Methods on `Cooldown`

| Method    | Returns | Description                 |
|-----------|---------|-----------------------------|
| `Ready()` | `bool`  | True when `remaining <= 0`. |

### Easing constants

| Constant               | Value |
|------------------------|-------|
| `Easing_Linear`        | 0     |
| `Easing_EaseInQuad`    | 1     |
| `Easing_EaseOutQuad`   | 2     |
| `Easing_EaseInOutQuad` | 3     |

---

## Render (`render_module.cpp`)

### Components (via GetMaterial/MutMaterial etc.)

`Camera`, `CubePrimitive`, `SpherePrimitive`, `QuadPrimitive`, `CapsulePrimitive`, `MeshPrimitive`,
`AmbientLight`, `DirectionalLight`, `Material`, `SpriteAtlas`, `AlbedoMap`

---

## UI (`ui_module.cpp`)

### Components (via GetButton/MutButton etc.)

`UIRect`, `UIElement`, `Panel`, `Button`, `ProgressBar`, `Spinner`, `Stack`, `ScrollRect`, `Modal`

### Callback globals

| Global                        | Description                                             |
|-------------------------------|---------------------------------------------------------|
| `SetOnClick(Entity btn, @fn)` | Bind a script callback to a `Button`'s onClick handler. |

Callback signature: `void MyClick(Entity button)`

---

## AI (`ai/*_module.cpp`)

### Components

`Blackboard`, `BehaviorTree`, `FSMAgent`, `GOAPAgent`, `SteeringAgent`, `SteeringTarget`, `UtilityAgent`

### Methods on `Blackboard`

| Method                          | Returns  |
|---------------------------------|----------|
| `GetFloat(string key)`          | `float`  |
| `SetFloat(string key, float)`   | `void`   |
| `GetBool(string key)`           | `bool`   |
| `SetBool(string key, bool)`     | `void`   |
| `GetString(string key)`         | `string` |
| `SetString(string key, string)` | `void`   |

### `BehaviorTree` status constants

| Constant           | Value |
|--------------------|-------|
| `BTStatus_Invalid` | 0     |
| `BTStatus_Running` | 1     |
| `BTStatus_Success` | 2     |
| `BTStatus_Failure` | 3     |

---

## Graph (`graph_module.cpp`)

### Components

`NodeId`, `GraphNode`, `GraphEdgeWeight`

---

## Particles (`particles_module.cpp`)

### Components

`ParticleEmitter`, `Particle`

---

## Noise (`noise_module.cpp`)

### Value types

| Type                 | Constructor                                         | Fields                                       |
|----------------------|-----------------------------------------------------|----------------------------------------------|
| `FractalNoiseParams` | `FractalNoiseParams(int octaves, float p, float l)` | `int octaves; float persistence, lacunarity` |

### Reference handle: `NoiseSampler@`

Obtained via `GetNoiseSampler()`. Owns an isolated generator instance seeded from `NoiseConfig`.

| Method                                                         | Returns | Description      |
|----------------------------------------------------------------|---------|------------------|
| `Sample2D(const vec2 &in pos)`                                 | `float` | 2D noise sample. |
| `Sample3D(const vec3 &in pos)`                                 | `float` | 3D noise sample. |
| `FractalNoise2D(const vec2 &in, const FractalNoiseParams &in)` | `float` | FBM 2D.          |
| `FractalNoise3D(const vec3 &in, const FractalNoiseParams &in)` | `float` | FBM 3D.          |

### Globals

| Global                                                         | Returns         | Description                                                                     |
|----------------------------------------------------------------|-----------------|---------------------------------------------------------------------------------|
| `GetNoiseSampler()`                                            | `NoiseSampler@` | Fresh handle for active algorithm + seed.                                       |
| `Noise2D(const vec2 &in pos)`                                  | `float`         | Quick 2D sample via active algorithm.                                           |
| `Noise3D(const vec3 &in pos)`                                  | `float`         | Quick 3D sample.                                                                |
| `FractalNoise2D(const vec2 &in, const FractalNoiseParams &in)` | `float`         | FBM 2D shorthand.                                                               |
| `FractalNoise3D(const vec3 &in, const FractalNoiseParams &in)` | `float`         | FBM 3D shorthand.                                                               |
| `Noise_SetAlgorithm(string name)`                              | `void`          | Switch active algorithm (`"perlin"`, `"simplex"`, `"value"`, `"worley_f1"`, …). |
| `Noise_SetSeed(int seed)`                                      | `void`          | Change active seed (0–9999).                                                    |

> **Note:** All free globals and `GetNoiseSampler()` resolve through `NoiseConfig` (the active
> algorithm name + seed stored as a world singleton). `NoiseScene::Tick` also polls `NoiseConfig`
> to rebuild the visualisation texture when scripts change the algorithm or seed.

> **TODO:** Once the scripting backend gains namespace support (`SetDefaultNamespace`), these will
> move into a `noise` namespace. See [noise/README.md](../src/engine/noise/README.md).

---

## Tilemap (`tilemap_module.cpp`)

### Components

`TileSet`, `Tilemap`, `TilemapSpec`, `TileCallbackState`, `TilemapViewport`, `GridPosition`

### Methods on `TileSet`

| Method               | Returns           | Description                             |
|----------------------|-------------------|-----------------------------------------|
| `CreateTile(int id)` | `TileDescriptor@` | Get or create a tile definition handle. |
