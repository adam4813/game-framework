# Spec 1 · Operator movement & collision

Part of [Signal Room spec 1: Room & bootstrap](../spec.md). Walking, dashing, facing, the tuning and bindings files, and 2D collision.

## User stories

23. As a player, I want to walk with WASD or the arrow keys at 4.5 m/s, so that crossing the room takes about 3 s.
24. As a player, I want a short ramp up (about 0.08 s) and a short stop (about 0.15 s), so that movement has weight
    but still feels responsive.
25. As a player, I want diagonal movement no faster than straight movement, so that no direction is a shortcut.
26. As a player, I want my Operator to turn toward my input quickly (about 0.06 s), so that facing follows my intent.
27. As a player, I want to dash on Shift (2.2× for 0.2 s, 0.5 s cooldown), so that I can make up lost time.
28. As a player, I want to dash while carrying, so that carrying never locks me out of a dash.
29. As a player, I want carrying an Item to scale my walk and dash by its `carry_speed`, so that heavy or fragile Items
    can feel different later (every slice Item is 1.0×).
30. As a player, I want to slide along walls and furniture when I walk into them at an angle, so that I never stick on
    corners.
31. As a player, I want Tarped Stations and dressing-prop tarps with floor footprints to block me like any furniture,
    so that the room's obstacles are honest.
32. As a player, I want the storm-window cells and the gap between Radio 2 and Radio 3 to stay walkable, so that the
    room plays as drawn.
33. As a player, I want my Operator drawn in its identity colour (P1 brick red) with a collar, so that I can find it at
    a glance.

## Implementation decisions

- **Tuning file:** walk 4.5 m/s; accel time 0.08 s; decel time 0.15 s; turn time 0.06 s; dash ×2.2 for 0.2 s with a
  0.5 s cooldown; Operator radius (start ~0.3 m); facing-ray length 0.9 m; cone fallback 1.2 m / ±50°; target pulse
  rate and brightness. The ImGui panel edits it live. Saving tuning back to disk is not required.
- **Bindings file:** actions `move_up`, `move_down`, `move_left`, `move_right`, `dash`, `interact` → key lists
  (defaults: WASD + arrows, Left/Right Shift, Space + E). This spec has a single keyboard table. Spec 3 turns it into
  the per-Operator binding layer, so game logic reads actions and never raw key codes.
- **Collision** is hand-rolled 2D on the floor plane: the Operator is a circle against the AABBs of solid cells (Station
  footprints, Tarped or not, Counters, walls, the south edge block and solid props), resolved with slide-along. No Jolt.
  Operator–Operator soft separation is spec 3 (this spec has one Operator), but the collision step should take a list
  of circles so it can be added without restructuring.
- **Movement** reads actions, builds a normalised wish direction (camera looks north, so up = −z/north), accelerates
  and decelerates toward `walk × carry_speed` (× dash while dashing), and turns facing toward input. Dash takes the
  current wish direction, or facing when there is no input. It works while carrying and respects the cooldown.
