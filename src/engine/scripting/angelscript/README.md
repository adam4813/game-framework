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

## `GetT()` and the modified notification

`ComponentGetRefGeneric` (the `GetT()` dispatcher) calls `host.modified(component_id)` on every invocation. This is a
deliberate trade-off documented in the generic scripting README:

> Every `GetT()` call marks the component dirty so in-place mutations fire `OnSet` observers
> without a separate `SetT()` write-back. The cost is that read-only accesses also dirty the
> component.

The call to `modified()` is only made when the component pointer is non-null.

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

Mutations through these properties go directly to the ECS storage. Because `GetT()` calls
`modified()`, any observer watching `OnSet` for the component will re-fire on the next flush, allowing path changes to
re-trigger handle resolution without an explicit `SetT()` call.
