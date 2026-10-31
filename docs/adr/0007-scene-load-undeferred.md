# Scene `Load()` Runs Undeferred; Data-Reading Observers React to `OnSet`

The scene module's `OnSceneActivate` observer is the single owner of deferral during a scene load: it wraps `Load()` in
`defer_begin` + `defer_suspend` ... `defer_resume` + `defer_end`, so `Load()` always runs undeferred, whether
activation came from a merge or from a constructor at defer count 0. A scene can then look up and mutate what it has
just created without managing deferral itself, and scenes never call `defer_suspend()` themselves (a second suspend
flips the defer counter back to deferred, which crashed the tilemap scene).

The price is that observers see each operation as it happens rather than a merged batch. Undeferred, `set<T>(v)` adds
`T` default-constructed and fires `OnAdd` before copying `v` in, and level JSON applies an entity's components one at
a time in alphabetical key order (`rigid_body` before `transform`). So every observer that reads component data reacts
to `OnSet`, not `OnAdd`, and tolerates the components it depends on arriving in either order (each dependency's
`OnSet` re-checks). `OnAdd`/`OnRemove` remain for structure only (tags, pairs, teardown).

Rejected: running `Load()` deferred (the 0.6.0 behaviour). That hides the ordering bugs instead of fixing them, since
the same observers run undeferred whenever a script, a save restore or a runtime spawn sets components outside a
system, which is how rigid bodies came to spawn at the origin. It also forces every scene that needs read-back to
suspend deferral itself, which is easy to get wrong.

`tests/unit/scene_load_order_test.cpp` runs the same level both ways and expects identical transforms and bodies.
