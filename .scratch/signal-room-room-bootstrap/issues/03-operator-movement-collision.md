# 03: Operator movement & collision

Spec: [Signal Room spec 1: Room & bootstrap](../spec.md)
Base branch: `feat/signal-room` (this game's main: branch off it, PR back into it, never into `main`)

**What to build:** The player drives one Operator around the Shift 1 room on the keyboard. It spawns at the Roster's
spawn marker in P1 brick red with an identity-colour collar. It walks with a short ramp up and stop, turns toward input
and dashes on Shift. It slides along walls, furniture, Tarped Stations and solid props, and can't leave the open south
edge. A developer tunes the feel live from the Room panel.

**Blocked by:** 02 (Shift 1 Layout & greybox reads).

**Status:** ready-for-agent

- [ ] The Shift file gains a `roster`: an ordered list. Here it has one entry with a `spawn` marker name and a `colour`
      palette token. A missing spawn marker is a load error naming it.
- [ ] A bindings file maps actions (`move_up`, `move_down`, `move_left`, `move_right`, `dash`, `interact`) to keys.
      Defaults: WASD + arrows, Left/Right Shift, Space + E. Game logic reads actions, never raw key codes.
- [ ] A tuning file holds walk 4.5 m/s, accel ~0.08 s, decel ~0.15 s, turn ~0.06 s, dash ×2.2 for 0.2 s with a 0.5 s
      cooldown, and Operator radius ~0.3 m.
- [ ] Movement is continuous on the floor plane (up = north, since the camera looks north), and diagonal input is
      normalised. Dash uses the wish direction, or facing when there is no input. Speed is scaled by a `carry_speed`
      factor that is 1.0 until Items exist (ticket 05).
- [ ] Collision is hand-rolled 2D: the Operator's circle against solid-cell AABBs, with slide-along. The step takes a
      list of circles, so Operator–Operator separation (spec 3) can be added without restructuring. No Jolt.
- [ ] The Room panel gets live tuning sliders (not saved to disk) and an Operator-circle overlay.
- [ ] Position, velocity, facing and dash state are reflected components visible in the Flecs Explorer.
- [ ] Harness tests:
  - a 1 s walk covers about 4.5 m, within the accel-ramp tolerance;
  - diagonal is no faster than straight;
  - a dash covers more ground than walking, and a second dash inside the cooldown does nothing;
  - walking into the Decoder or a Tarped Station never enters its footprint, and an angled approach slides along it;
  - the Operator can't cross the south edge;
  - the storm-window cells are walkable.
- [ ] Runtime check: walk and dash around the room in the running game, and drive it through the Flecs Remote API's
      input overrides with no asserts.
