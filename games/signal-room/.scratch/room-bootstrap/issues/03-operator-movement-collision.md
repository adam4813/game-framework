# 03: Operator movement & collision

Spec: [Signal Room spec 1: Room & bootstrap](../spec.md). Read: [Operator movement & collision](../spec/operator-movement.md), [Debugging & tuning](../spec/debugging.md), [Testing decisions](../spec/testing.md).
Base branch: `feat/signal-room` (this game's main: branch off it, PR back into it, never into `main`)

**What to build:** The player drives one Operator around the Shift 1 room on the keyboard. It spawns at the Roster's
spawn marker in P1 brick red with an identity-colour collar. It walks with a short ramp up and stop, turns toward input
and dashes on Shift. It slides along walls, furniture, Tarped Stations and solid props, and can't leave the open south
edge. A developer tunes the feel live from the Room panel.

**Blocked by:** 02 (Shift 1 Layout & greybox reads).

**Status:** done

- [x] The Shift file gains a `roster`: an ordered list. Here it has one entry with a `spawn` marker name and a `colour`
      palette token. A missing spawn marker is a load error naming it.
- [x] A bindings file maps actions (`move_up`, `move_down`, `move_left`, `move_right`, `dash`, `interact`) to keys.
      Defaults: WASD + arrows, Left/Right Shift, Space + E. Game logic reads actions, never raw key codes.
- [x] A tuning file holds walk 4.5 m/s, accel ~0.08 s, decel ~0.15 s, turn ~0.06 s, dash ×2.2 for 0.2 s with a 0.5 s
      cooldown, and Operator radius ~0.3 m.
- [x] Movement is continuous on the floor plane (up = north, since the camera looks north), and diagonal input is
      normalised. Dash uses the wish direction, or facing when there is no input. Speed is scaled by a `carry_speed`
      factor that is 1.0 until Items exist (ticket 05).
- [x] Collision is hand-rolled 2D: the Operator's circle against solid-cell AABBs, with slide-along. The step takes a
      list of circles, so Operator–Operator separation (spec 3) can be added without restructuring. No Jolt.
- [x] The Room panel gets live tuning sliders (not saved to disk) and an Operator-circle overlay.
- [x] Position, velocity, facing and dash state are reflected components visible in the Flecs Explorer.
- [x] Harness tests:
  - a 1 s walk covers about 4.5 m, within the accel-ramp tolerance;
  - diagonal is no faster than straight;
  - a dash covers more ground than walking, and a second dash inside the cooldown does nothing;
  - walking into the Decoder or a Tarped Station never enters its footprint, and an angled approach slides along it;
  - the Operator can't cross the south edge;
  - the storm-window cells are walkable.
- [x] Runtime check: walk and dash around the room in the running game, and drive it through the Flecs Remote API's
      input overrides with no asserts.

## Comments

**2026-10-18 (implementation):** Done on `feat/signal-room-03-operator-movement`.

- Data: `roster` in `shifts/shift_01.shift.json` (`spawn_1`, `p1`), `assets/bindings.json` (keys named as the
  engine's `KeyCode` constants), `assets/tuning.json` (a `movement` section; ticket 04 can add its targeting numbers
  beside it), and an `operator` model in the greybox catalogue. Parts whose colour token is `identity` (body, collar)
  take the Roster entry's colour. Load errors name the Shift and the missing spawn marker or unknown colour token.
- **`OperatorActions` is the action seam.** `OperatorInput` fills each Operator's reflected `OperatorActions` (`move`
  wish vector, `dash`, `interact` presses) from the Bindings, and `OperatorMovement` reads only that. Spec 3's Seats
  and the AI write the same component. This also covers the runtime check: the engine's `InputState` reflects only
  the mouse, so the REST input override can't press keys. Instead, disable `OperatorInput` and set
  `OperatorActions` on `RoomRoot.Operator1`.
- The dash cooldown starts when the dash ends (0.2 s dash + 0.5 s cooldown = 0.7 s between dash starts), as the
  tuning file's comment says.
- Position is the Operator's `Transform` (x, z). `OperatorMotion` holds velocity and facing (yaw, 0 = north, mirrored
  into `Transform.rotation.y`), and `Dash` holds the dash state. All are reflected.
- Collision: `MoveCircles(FloorGrid, span<FloorCircle>)` pushes circles out of solid cells (off-grid cells count as
  solid, which covers the walls and the south edge) and splits long moves into half-radius steps so a frame hitch
  can't tunnel through a Counter. Velocity into a surface is dropped so the Operator slides on.
- Room panel: a Movement section with live sliders for every tuning value (the radius slider rebuilds the overlay),
  and an Operator circle overlay toggle (a disc parented to each Operator).
- Tests went from 44 to 59 (`operator_movement_test`, 15 tests). Breaking the collision push-out fails all five
  collision tests.
- Runtime check (desktop): over REST the Operator walked east, dashed (cooldown ticking), stopped against Counter
  (7,6), slid NE along the Decoder into the corner against the east wall and the Supply Rack tarp, and dashed along
  its facing. No asserts or errors in `signal_room.log`. A screenshot shows the red Operator with its circle overlay
  at `spawn_1`. The WASM build loads Shift 1 in headless Chrome. Keyboard play by hand has not been checked yet.
- Retro follow-up: the engine now has a `KeyOverride` singleton and `flecs-api.js press-keys`, so the bound keys
  themselves can be pressed over REST. `press-keys D`, `press-keys "D LeftShift"` and `press-keys S` walked, dashed
  and stopped the Operator against Counter (7,6). `bindings.cpp` now resolves key names with the engine's
  `KeyCodeFromName`.
