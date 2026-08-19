# Scripting System

Backend-agnostic ECS scripting layer. Scripts run as child entities of a host and interact with the ECS world through a
reflection-driven API. The active backend (currently AngelScript) is chosen at startup; see `angelscript/` for
implementation notes.

## Component access API

Every ECS component registered with `RegisterComponentForScripts` exposes three entity methods:

| Method            | Returns        | Behaviour                                                       |
|-------------------|----------------|-----------------------------------------------------------------|
| `T@ GetT()`       | mutable handle | Direct pointer into ECS storage; marks the component modified   |
| `T@ AddT()`       | mutable handle | Ensures the component exists (adds with defaults if absent)     |
| `void SetT(T@ v)` | —              | Upsert: adds the component if absent, replaces value if present |

### `GetT()` always marks the component modified

`GetT()` is **intentionally eager**: it calls the backend's modified notification on every call — even read-only
accesses. This means in-place mutations are seen by `OnSet` observers and change-detection queries without a separate
`SetT()` write-back:

```angelscript
// in-place mutation — GetT() called modified(), so OnSet observers fire automatically
SoundEffect@ sfx = self.GetSoundEffect();
sfx.path = "sfx/land.wav"; // observer re-triggers, new sound loaded — no SetT() needed
sfx.Fire();                 // playing = true, SoundEffectPlayback picks it up this frame
```

**Performance note**: every `GetT()` call dirtifies the component each tick, regardless of whether the script actually
writes to it. Avoid `GetT()` in hot read-only paths; use the generic field accessors (`GetFloat`, `GetInt`, `GetBool`)
or the copy-return singleton pattern (`GetInputState()`) instead.

> **TODO**: Add a read-only `PeekT()` variant (or a const overload) that returns a handle
> *without* firing the modified notification, so high-frequency read paths don't pay the
> dirty-notification cost. Current blocker: the script `self` parameter is non-const, so
> AngelScript const-overload resolution doesn't distinguish the two calls today.

### `SetT()` — upsert

`SetT(v)` copies from a handle into ECS storage, adding the component if it isn't present yet. Use it when you have a
standalone value to push onto an entity rather than fetching and mutating:

```angelscript
// Upsert: copies the value from one entity's SoundEffect onto another
SoundEffect@ src = otherEntity.GetSoundEffect();
self.SetSoundEffect(src);
```

## Key codes

Always use `engine::input::KeyCode` constants via the `Key_<Name>` script globals — **never raw integers or
backend-specific key codes**:

```angelscript
if (input.WasKeyPressed(Key_Space))  { /* jump   */ }
if (input.WasKeyPressed(Key_T))      { /* toggle */ }
if (input.WasKeyPressed(Key_Escape)) { /* pause  */ }
```

See `engine/input/input_components.hpp` for the full `KeyCode` list.

## Singleton access

Singletons registered with `RegisterSingletonForScripts` are exposed as global getter functions that return a **copy**
of the singleton value:

```angelscript
InputState input = GetInputState();
bool jump = input.WasKeyPressed(Key_Space);
```

## Script-assignable callbacks

Two backend primitives let scripts supply callbacks; both wrap the script function in a
reference-counted `ScriptFunctionHandle` that C++ invokes later (typically via a `std::function`).
Callback signatures are **entity-first** — script funcdefs have no implicit receiver, so the subject
entity is passed explicitly as the first parameter.

### Callback as an argument — `RegisterCallbackFunctionForScripts`

Declares `void <name>(<fixed args...>, <funcdef>@)` — the trailing argument is the callback. The
sink receives the fixed arguments and a `ScriptFunctionHandle`, and stores/uses it however it likes.
Used for per-entity handlers backed by existing component storage, e.g. the UI module's button click:

```cpp
scripting::RegisterCallbackFunctionForScripts(world,
    { .name = "SetOnClick",
      .fixed_params = {{ .type = MakeObject(world.component<ScriptEntityRef>()), .name = "button" }},
      .callback = { .funcdef_name = "ClickCallback",
                    .params = {{ .type = MakeObject(world.component<ScriptEntityRef>()), .name = "button" }} } },
    [](ScriptCallContext& ctx, flecs::world&, ScriptFunctionHandle* handle) {
        auto* ref = static_cast<ScriptEntityRef*>(ctx.GetArgObject(0));
        if (!ref || !handle) return;                 // null handle clears
        handle->AddRef();
        auto sp = std::shared_ptr<ScriptFunctionHandle>(handle, [](auto* h){ if (h) h->Release(); });
        ref->GetEntity().set<OnClick>({[sp](flecs::entity e){
            ScriptEntityRef r{e}; const void* a[] = {&r}; sp->Invoke(a, 1);
        }});
    });
```

```angelscript
// script: bind self's parent button to a handler
SetOnClick(self.GetParent(), @OnClicked);
void OnClicked(Entity button) { Print("clicked " + button.GetName()); }
```

### Callback on a descriptor view — `RegisterCallbackViewTypeForScripts`

Exposes a C++ struct as a **non-owning reference view** (`asOBJ_REF | asOBJ_NOCOUNT`) with plain
fields by offset plus callback slots. Each slot registers a setter method `view.Set<Name>(@fn)` and
an optional invoker `view.<Name>(<params>)`. The setter's sink stores the callback on the C++ object
(no separate proxy struct or funcdef-handle field); the invoker fires the stored callback — which may
be script- or C++-registered. The tilemap module uses this for `TileDescriptor.SetOnEnter` /
`TileDescriptor.OnEnter` (see `engine/tilemap/README.md`).

> **AngelScript property syntax (`view.name = @fn`) is not usable for callbacks.** Property setters
> coerce their value parameter to `const T &in`, which is invalid for a funcdef handle. A
> `Set<Name>(@fn)` method is the portable form; funcdef handles work fine as normal method arguments.

### ScriptFunctionHandle lifetime

`ScriptFunctionHandle` is reference-counted (initial count 1). The backend releases its own reference
after the sink returns, so a sink that retains the callback must `AddRef()` it (typically by wrapping
it in a `shared_ptr` with a `Release` deleter, as above). `Invoke(args, count)` marshals each
`args[i]` (a pointer to the C++ value) to the matching AngelScript type from the registered parameter
list.

### Shutdown ordering

At final world teardown Flecs may release the AngelScript engine before the component/registry that
owns a stored callback, which would release a script function through a dead engine. Modules that
retain callbacks register a `RegisterScriptShutdownCallback` that clears them just before the engine
is torn down (scene unload is already safe — the owning storage is destroyed while the engine lives).

A benign `asMSGTYPE_WARNING: There is an external reference ...` may still appear during shutdown if
AngelScript's context pool/GC briefly holds a reference across `ShutDownAndRelease()`; all resources
are released by process exit. `ctx->Unprepare()` before `Release()` and a full GC cycle before
`ShutDownAndRelease()` minimise it.

**Important**: always pass funcdef handles with `@` to avoid implicit delegate creation:
```angelscript
td.SetOnEnter(@OnPromptTileEnter);   // ✓ direct function handle
// td.SetOnEnter(OnPromptTileEnter);  // ✗ creates an implicit delegate (GC-tracked, harder to clean up)
```


| Function                          | Phase                   | Typical use                            |
|-----------------------------------|-------------------------|----------------------------------------|
| `OnInit(Entity self)`             | First PreUpdate tick    | One-time setup, caching initial values |
| `PreTick(Entity self, float dt)`  | Per-frame, before main  | Input pre-processing                   |
| `Tick(Entity self, float dt)`     | Per-frame, main         | Game logic                             |
| `PostTick(Entity self, float dt)` | Per-frame, after main   | Cleanup, deferred actions              |
| `OnDestroy(Entity self)`          | ScriptComponent removed | Teardown                               |

`self` is the **script child entity**; component accessors resolve to the **parent (host)
entity** automatically.
