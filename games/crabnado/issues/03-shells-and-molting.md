# 03: Shells and molting

Spec: [spec](../spec.md). Read: [The run](../spec/run.md), [Content](../spec/content.md) (upgrades).

**What to build:** Defeated enemies drop shells carrying their XP value. Shells inside the player's pickup range fly
to the player and are collected. A HUD bar shows shells toward the next molt and the molt level. When enough shells
are collected the player molts: the simulation pauses (pause as data), and a modal offers 3 distinct upgrades drawn
from a JSON upgrade registry, picked with 1/2/3 or a click. The chosen upgrade applies, and each molt needs more
shells than the last. The upgrade list starts with the ones that affect existing systems (claw damage and reach,
move speed, max HP and heal, pickup range). Later tickets add their own entries.

**Blocked by:** 02 (Core loop).

**Status:** ready-for-agent

- [ ] Killing enemies drops shells, and walking near them collects them.
- [ ] The shell bar and molt level update on the HUD.
- [ ] Molting pauses enemies, the crabnado and the run clock, and shows 3 distinct upgrades.
- [ ] Picking with 1/2/3 or a click applies the upgrade and resumes the run, and the effect is visible (e.g. a bigger
      claw ring).
- [ ] Upgrades come from a JSON registry. Adding an entry needs no C++ change.
- [ ] Upgrade rolls draw from `RngState`.
- [ ] Checked in the running game over the Remote API (e.g. set shells to one short of a molt).
