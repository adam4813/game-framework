# 02: Core loop: crab vs. crabnado

Spec: [spec](../spec.md). Read: [The run](../spec/run.md), [Technical](../spec/technical.md),
[Content](../spec/content.md) (claw snap only).

**What to build:** The first playable run. In the game scene the player's hermit crab moves with WASD or the arrows,
scuttling sideways. The crabnado wanders the arena and throws crabs in ballistic arcs that land between it and the
player. Landed crabs walk sideways toward the player and crowd without overlapping (game-side spatial grid). Claw
snap fires on its own on a cooldown, damaging every landed enemy in its radius, with a visible ring and a hit flash.
Touching an enemy drains HP, and at 0 HP the run stops (a "Swept away" placeholder is fine until ticket 13). A run
clock counts up, and the spawn rate rises with it. The camera follows the player, north-up.

**Blocked by:** 01 (Horde performance check), which sets the horde cap and the enemy rendering approach.

**Status:** ready-for-agent

- [ ] Clicking Play starts a run where the player moves, crabs land from the crabnado, chase the player, and die to
      claw snap.
- [ ] Enemies in the air never deal or take damage until they land.
- [ ] Crabs don't stack on top of each other, and crowding cost stays within the horde cap's frame budget.
- [ ] HP drains on contact, and the run stops at 0 HP.
- [ ] The spawn rate rises over the run and the horde never exceeds the cap.
- [ ] Run state (clock, kills, HP) is reflected: queried and set over the Remote API to confirm it (e.g. set HP low and
      watch the run end).
- [ ] Spawn randomness draws from `RngState`.
- [ ] The scene-flow test still passes, and the Title button still returns to the title.
