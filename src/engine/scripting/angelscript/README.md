# AngelScript Backend

Implementation of `IScriptBackend` for AngelScript 2. Translates the engine's reflection-driven component API into
AngelScript type registrations and generic-calling-convention dispatchers.

## Why generic calling convention everywhere

AngelScript's native calling conventions (`asCALL_THISCALL`, `asCALL_CDECL`) are unsupported on WebAssembly. Every
method and global function is registered via `asCALL_GENERIC` so the build works identically on desktop and WASM.

## Component type registration

Components registered via `RegisterComponentForScripts` are registered as
`asOBJ_REF | asOBJ_NOCOUNT` reference types (not `asOBJ_VALUE | asOBJ_POD`):

- **Ref type** (`asOBJ_NOCOUNT`) — scripts hold a handle (`T@`) pointing directly into Flecs component storage. No
  ref-counting; Flecs owns the lifetime.
- **No factory** — scripts cannot construct a new instance with `T()`. Use `AddT()` on an entity instead.
- **Members exposed by offset** via `RegisterObjectProperty` — the same Flecs-reflected member metadata drives this, so
  any type registered in Flecs meta (including the opaque `string`
  type) is accessible from scripts.

Value types (math primitives: `vec2`, `vec3`, `Rgba`) stay `asOBJ_VALUE | asOBJ_POD` and are registered via
`RegisterValueTypeForScripts`.

## Component access variants: `GetT()`, `MutT()`, and const access

Three variants are registered for each component:

- **`GetT()`** (`ComponentGetRefGeneric`) — mutable handle without marking modified. Use for read-only or silent mutations.
- **`MutT()`** (`ComponentGetMutRefGeneric`) — mutable handle that calls `host.modified(component_id)`, firing `OnSet` observers.
- **`GetT() const`** (`ComponentGetConstRefGeneric`) — const handle, only on const entity references. Read-only only.

## `AddT()` and deferred structural changes

`ComponentAddGeneric` calls `ecs_ensure_id`, which adds the component (zero-initialised) if absent. In deferred mode
(inside a system tick), the add is queued and a pointer to temporary command-buffer storage is returned. Scripts write
to this pointer; when deferred commands flush, Flecs applies the value. This is safe and is the standard Flecs pattern
for adding components from within a system.

## String member support

`std::string` is registered as an opaque Flecs type named `"string"` (matching the AngelScript
`scriptstdstring` registration). `ResolveMemberTypeName` maps the Flecs `std::string` entity to
`"string"` via the generic name lookup, so string members on components (e.g. `SoundEffect.path`,
`TextureMap.path`) are automatically exposed as `string` properties via `RegisterObjectProperty`.

Mutations through these properties go directly to the ECS storage. Use `MutT()` when you need observer notifications, or `GetT()` for silent mutations.
