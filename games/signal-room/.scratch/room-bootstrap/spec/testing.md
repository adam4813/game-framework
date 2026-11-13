# Spec 1 · Testing decisions

Part of [Signal Room spec 1: Room & bootstrap](../spec.md). The one test seam (the headless Room harness), what good tests check, and the runtime check.

- **One seam: a headless Room harness**, confirmed with the user. It follows the scene-equivalence `TestWorld` (a real
  `EngineContext` on a `NullPlatform`, no window or GPU) and imports the Signal Room module. A test loads a Shift file
  (fixture files under the game's tests, plus the real Shift 1), presses actions by writing the bound keys into the
  `InputState` singleton, steps the world for N frames at a fixed delta, and asserts only on observable world state:
  Operator position and facing, the Target relationship, what the Operator carries, what each Counter holds, which
  Stations are Tarped, and load errors.
- **Good tests check behaviour a player or designer would notice, never internals.** Examples: "walking east into the
  Decoder for 2 s leaves the Operator outside its footprint"; "holding up-right does not outpace holding up"; "after one
  1 s walk the Operator is about 4.5 m away (within a tolerance for the accel ramp)"; "dash covers more ground, and a
  second dash inside 0.5 s does nothing"; "a Tarped Keypunch is never the Target even when faced point-blank"; "facing
  the Telex from its unworked south side gives no Target"; "pick up → walk → set down moves the Item to the second
  Counter, and an occupied Counter refuses"; "carry_speed 0.5 halves the walk distance"; "a Shift listing an unknown
  Station name fails to load with that name in the error"; "Shift 1 puts Radio1, Decoder and Telex in service and Tarps
  Radio2, Radio3 and Keypunch".
- Collision maths, targeting and schema parsing are covered **only through the harness**. No separate pure-function
  seam.
- **Prior art:** `tests/scene_equivalence` (`TestWorld` + `NullPlatform`) for the harness; `tests/unit` GoogleTest
  style (behaviour-named `TEST`s, `add_engine_test`) for structure and registration.
- **Runtime verification** (required by `AGENTS.md` before calling it done): launch the desktop build, use the Flecs
  Remote API to switch to the Room scene, query the Operator and Counter components, drive input overrides to walk,
  dash, pick up and set down, and confirm no runtime asserts. Also load the WASM build in a browser once.
