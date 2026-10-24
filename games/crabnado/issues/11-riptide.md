# 11: Riptide

Spec: [spec](../spec.md). Read: [Content](../spec/content.md) (weapons, upgrades), [Wind and waves](../spec/wind-and-waves.md).

**First to cut if the jam runs behind.** If cut, remove its upgrade entries from the data, and the first molt's
guaranteed wave upgrade falls back to "deep roots".

**What to build:** The third weapon. Riptide is unlocked by an upgrade. On its cooldown it raises the height of the
next crest, making it big enough to tumble lobsters and slam harder. A follow-up upgrade strengthens it. The first molt
always offers the Riptide unlock, so players discover the wave synergy early.

**Blocked by:** 03 (Shells and molting), 05 (Waves and tumble).

**Status:** done

- [x] The first molt always includes the Riptide unlock.
- [x] With Riptide, the next crest is visibly bigger and tumbles lobsters.
- [x] The strengthening upgrade only appears once Riptide is unlocked.
- [x] Riptide raises crests through the wave field, not by pushing enemies directly.
- [x] Checked in the running game.

## Decisions

Settled 2026-10-24.

- **Component.** A reflected `Riptide` on the player, always present: `level` (float, 0 = locked), `cooldown 8`,
  `multiplier 2.2`, `timer`. The system does nothing while `level == 0`. Unlocking sets the timer to 2 s, so it
  fires soon after the pick.
- **Firing (prototype `fireRiptide`).** On the primary swell, find the next crest index `n` that reaches the player
  at least 1 s from now, and store `boost = {n, multiplier}` on that swell in `WaveField`. `SampleWaves` multiplies
  that crest's height (on top of `bigAmp` if it's big). Nothing pushes enemies directly. Telegraph: "RIPTIDE CREST
  in 1.6 s".
- **Upgrade conditions.** Extend the registry format with an optional `"requires"` object:
  `{ "stat": "Riptide.level", "min": 1 }` or `{ "stat": "Riptide.level", "max": 0 }`. It's read by reflection like
  the effects. Entries: `riptide` (Riptide.level add 1, requires max 0) and `stronger_riptide` (cooldown multiply
  0.75, multiplier add 0.5, requires min 1).
- **First molt.** Add an optional `"first_molt": true` flag. On the first molt (molt level 1 → 2), one eligible flagged
  entry fills slot 1, and the other 2 slots roll as usual from the rest. Flag `riptide`. **If Riptide is cut,**
  delete both entries and move `"first_molt": true` to `deep_roots`.

## Comments

- Checked on desktop over the Remote API: the first molt offered `riptide` in slot 1; picking it set `Riptide.level` 1 and
  the primary swell's `boost_n`/`boost_mult` (2.2) were set within the 2 s delay. The Telegraph read "RIPTIDE CREST in
  1.4 s". The second molt offered `stronger_riptide` (it only appears once unlocked); picking it took the cooldown 8 → 6
  and the multiplier 2.2 → 2.7. A boosted crest is 2.2× a normal one, the same height as a big crest, so it tumbles
  lobsters; I didn't watch a lobster tumble in play. The visible size follows from the `SampleWaves` height the water tiles read.
