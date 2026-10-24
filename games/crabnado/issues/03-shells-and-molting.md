# 03: Shells and molting

Spec: [spec](../spec.md). Read: [The run](../spec/run.md), [Content](../spec/content.md) (upgrades).

**What to build:** Defeated enemies drop shells carrying their XP value. Shells inside the player's pickup range fly
to the player and are collected. A HUD bar shows shells toward the next molt and the molt level. When enough shells
are collected the player molts: the simulation pauses (pause as data), and a modal offers 3 distinct upgrades drawn
from a JSON upgrade registry, picked with 1/2/3 or a click. The chosen upgrade applies, and each molt needs more
shells than the last. The upgrade list starts with the ones that affect existing systems (claw damage and reach,
move speed, max HP and heal, pickup range). Later tickets add their own entries.

**Blocked by:** 02 (Core loop).

**Status:** done

- [x] Killing enemies drops shells, and walking near them collects them.
- [x] The shell bar and molt level update on the HUD.
- [x] Molting pauses enemies, the crabnado and the run clock, and shows 3 distinct upgrades.
- [x] Picking with 1/2/3 or a click applies the upgrade and resumes the run, and the effect is visible (e.g. a bigger
      claw ring).
- [x] Upgrades come from a JSON registry. Adding an entry needs no C++ change.
- [x] Upgrade rolls draw from `RngState`.
- [x] Checked in the running game over the Remote API (e.g. set shells to one short of a molt).

## Comments

- Upgrades live in `assets/data/crabnado/upgrades.json`. Each effect names a reflected float on the player as
  `"Component.member"` with `"op": "add"` or `"multiply"`, and is applied through Flecs reflection, so a new entry that
  tunes an existing stat needs no C++. Optional conditions (e.g. Riptide unlocked) and the first molt's wave upgrade come
  with the tickets that add those systems.
- The molt level starts at 1 and the first molt needs 6 shells, then 6 + 5 per level (the prototype's curve).
- Checked over the Remote API on desktop: kills drop shells that fly in and fill the bar; a molt pauses the clock and
  the horde; 3 distinct upgrades show; key 1 and a click on `Choice1` both apply the upgrade (claw damage 2 → 3, claw
  radius 4.5 → 5.4, Thick Shell max HP 100 → 125 and HP +25) and resume the run. Setting `shells` one short also
  triggers the molt on the next pickup.
