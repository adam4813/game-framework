# Scripting API

Backend-agnostic ECS scripting. Scripts run as **child entities** of a host and interact with the world through a
reflection-driven API. The active backend (AngelScript today) is chosen at startup and imported first so every other
module can expose its components to scripts from its constructor.

Deep dives: [scripting/README.md](../src/engine/scripting/README.md) (the API contract) and
[angelscript/README.md](../src/engine/scripting/angelscript/README.md) (backend internals, WASM constraints).

## Attaching a script

Scripts attach as a child of the host, one `ScriptComponent` per child (multiple scripts per host = multiple children).
`source_path` and `inline_source` are mutually exclusive.

```cpp
auto s = world.entity("FooScript").child_of(host);
s.set<engine::scripting::ScriptComponent>({ .source_path = "scripts/foo.as" });
```

The backend's `ScriptComponent.OnSet` observer compiles/instantiates the script; `OnRemove` tears it down. In scripts,
`self` is the **script child entity**, but component accessors resolve to the **parent (host)** entity automatically.

## Component access API

Every component registered with `RegisterComponentForScripts(world, world.component<T>())` gains four entity methods in
scripts. The exact signature depends on whether the entity reference is mutable or const:

**On a mutable Entity&:**

| Method            | Returns        | Behaviour                                                                     |
|-------------------|----------------|-------------------------------------------------------------------------------|
| `T@ GetT()`       | mutable handle | Direct pointer into ECS storage; **does not mark component modified**.        |
| `T@ MutT()`       | mutable handle | Direct pointer into ECS storage; **marks the component modified** every call. |
| `T@ AddT()`       | mutable handle | Ensures the component exists (adds zero-initialised if absent).               |
| `void SetT(T@ v)` | —              | Upsert: add if absent, replace value if present.                              |

**On a const Entity&:**

| Method                | Returns      | Behaviour                                            |
|-----------------------|--------------|------------------------------------------------------|
| `const T@ GetT() const` | const handle | Read-only pointer into ECS storage.                  |

### When to use each method:

**`GetT()`** — Safe mutable access without notifications. Use for private mutations or reads where observer notifications would be unwanted:

```angelscript
// Read and modify without observer side effects
SoundEffect@ sfx = self.GetSoundEffect();  // no modified() call
sfx.path = "sfx/land.wav";                  // change is local to this frame
sfx.Fire();                                 // only this explicit call has side effects
```

**`MutT()`** — Notify Flecs that you modified a component. Use when you want `OnSet` observers to fire immediately:

```angelscript
SoundEffect@ sfx = self.MutSoundEffect();  // calls modified() internally
sfx.path = "sfx/land.wav";                  // OnSet re-fires → new handle resolved, observers run
sfx.Fire();                                 // playing = true; SoundEffectPlayback picks it up
```

**`GetT() const`** — Read-only on a const entity. Compile error if you try to modify:

```angelscript
const SoundEffect@ sfx = self.GetSoundEffect();  // const reference (if self is const)
// sfx.path = "..."; // compile error: cannot modify
Print("Sound: " + sfx.path); // OK
```

**Performance note:** `MutT()` marks the component modified even on read-only access. In hot read paths, prefer
`GetT()` (on a mutable entity) or the copy-returning singleton getters.

### ScriptTraversal — transparent relationship navigation

Some components (e.g., `Material`, `AlbedoMap`, `ShaderMap`) may live on a child entity linked via a relationship (e.g.,
a material entity linked to the renderable via `RenderWith`). The **ScriptTraversal** pattern lets scripts access these
transparently without knowing the relationship structure:

```angelscript
Material@ mat = self.GetMaterial();     // If not found on host, the scripting backend navigates
                                         // through (ScriptTraversal, RenderWith) to find it
AlbedoMap@ albedo = self.GetAlbedoMap();
```

The backend registers the traversal relationship at component registration time; scripts never explicitly navigate
relationships.

**Relationship access in scripts:** Direct access to arbitrary relationships (e.g., getting the `LookAt` target of a
camera) is not yet exposed to scripts. Workarounds:

1. Store the relationship target as a component field if the relationship is static (e.g., store the target entity ID).
2. Use `ScriptTraversal` if the relationship is used only for component navigation (the `RenderWith` pattern above).
3. Extend the scripting backend to expose relationship getters (future work).

## Singletons

Registered with `RegisterSingletonForScripts`, exposed as global getters that return a **copy**:

```angelscript
InputState input = GetInputState();
bool jump = input.WasKeyPressed(Key_Space);
```

## Key codes

Always use the `Key_<Name>` script globals (which map to `engine::input::KeyCode`) — never raw integers or backend key
codes:

```angelscript
if (input.WasKeyPressed(Key_Space))  { /* jump   */ }
if (input.WasKeyPressed(Key_Escape)) { /* pause  */ }
```

## Script lifecycle

All entry points are optional; implement any subset:

| Function                          | Phase                           | Use                                   |
|-----------------------------------|---------------------------------|---------------------------------------|
| `OnInit(Entity self)`             | First `PreUpdate` tick          | One-time setup, cache initial values. |
| `PreTick(Entity self, float dt)`  | `PreUpdate` (`ScriptPreTick`)   | Input pre-processing.                 |
| `Tick(Entity self, float dt)`     | `OnUpdate` (`ScriptOnTick`)     | Main game logic.                      |
| `PostTick(Entity self, float dt)` | `PostUpdate` (`ScriptPostTick`) | Cleanup, deferred actions.            |
| `OnDestroy(Entity self)`          | On `ScriptComponent` remove     | Teardown.                             |

The three tick systems carry `ecs::Pausable`, so script logic halts while the game is paused. They map to
`ScriptTickPhase::Pre/On/Post`.

## Extending the API from C++

All helpers live in `scripting_module.hpp` and no-op safely if the backend singleton is absent
(`world.has<ScriptBackendSingleton>()` guard). Register once, after the backend is imported.

| Helper                                                                 | Exposes                                           |
|------------------------------------------------------------------------|---------------------------------------------------|
| `RegisterValueTypeForScripts(world, typeEntity)`                       | A Flecs-meta value type (`vec2`, `vec3`, `Rgba`). |
| `RegisterComponentForScripts(world, componentEntity)`                  | `GetT`/`AddT`/`SetT` for a component.             |
| `RegisterSingletonForScripts(world, componentEntity)`                  | A global `T GetT()` copy getter.                  |
| `RegisterComponentMethodForScripts<&C::M>(world, "M")`                 | A component method via a generated thunk.         |
| `RegisterGlobalFunctionForScripts(world, sig, thunk)`                  | A free global function.                           |
| `RegisterObjectConstructorForScripts(world, type, params, ctor, dtor)` | A non-default ctor on a value type.               |
| `RegisterGlobalConstantsForScripts(world, {...})`                      | Named global constants (`Key_*`, `Vec3_Up`, …).   |

**Script method naming convention:**

- **Getters/setters:** `GetT()`, `SetT(v)`, `AddT()` for components.
- **Callback wiring:** `Set<FieldName>(@function)` to store a callback in a `std::function` field, then invoke it with
  `<FieldName>(args)`. AngelScript property setters cannot accept `funcdef` handles, so the `Set<Name>` method is
  required instead of `field = value` syntax. (Example: `button.SetOnClick(@MyHandler)` stores the script function, then
  `button.OnClick(self)` invokes it.) See each module's README for its specific callback names and signatures.

Example (from `audio_module.cpp`) — a global function marshalled via `ScriptCallContext`:

```cpp
scripting::RegisterGlobalFunctionForScripts(world,
    {.name = "PlaySoundHandle", .return_type = ScriptValueType::MakeVoid(),
     .params = {{.type = ScriptValueType::MakeInt(), .by_reference = false, .name = "handle"}}},
    [](const ScriptCallContext& ctx, const flecs::world& w) {
        auto* platform = w.get<platform::PlatformRef>().ptr;
        if (const int h = ctx.GetArgInt(0); h >= 0) platform->PlaySound(h);
    });
```

## Backend notes (AngelScript)

- **`asCALL_GENERIC` everywhere** — native calling conventions are unsupported on WASM, so every method and global is
  registered generically; the same code runs on desktop and browser.
- Components are `asOBJ_REF | asOBJ_NOCOUNT` (handles into Flecs storage, no ref-counting, no factory — use `AddT()`);
  math primitives stay `asOBJ_VALUE | asOBJ_POD`.
- Members are exposed by offset from the same Flecs meta metadata, so any reflected member — including `std::string`
  (registered as the opaque `"string"` type) — is script-accessible.
- `AddT()` in deferred mode returns a pointer into the command buffer; the write is applied on flush (the standard Flecs
  pattern for adding components from a system).
