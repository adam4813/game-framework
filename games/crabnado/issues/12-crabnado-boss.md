# 12: Crabnado boss

Spec: [spec](../spec.md). Read: [Arena and boss](../spec/arena-and-boss.md) (boss).

**What to build:** At 5:00 a warning announces the boss, and the crabnado comes ashore and moves toward the player.
Its spawn rate drops so the fight stays readable. Big crests arrive on a fixed, telegraphed rhythm. When a big crest
pushes the crabnado into a rock, that's one slam (with a short cooldown so one crest can't count twice), and the HUD
shows the slams left out of 3. On the third slam the crabnado breaks apart and the run is won.

**Fallback:** if the boss isn't working by the second half of the jam, replace it with 60 s of maximum storm followed
by victory.

**Blocked by:** 08 (Tide-pool arena).

**Status:** done

- [ ] At 5:00 the boss warning shows and the crabnado starts chasing the player.
- [ ] Big crests are telegraphed during the boss.
- [ ] Luring the crabnado in front of a rock as a big crest hits counts one slam, and the HUD counts down from 3.
- [ ] One crest never counts as two slams.
- [ ] The third slam wins the run (a victory state in run state; ticket 13 shows it).
- [ ] Checked in the running game by setting the run clock to about 4:55 over the Remote API.

## Decisions

Settled 2026-10-24. Prototype boss values.

- **Phase.** Add `Boss` and `Won` to `RunPhase`. At `clock >= 300` the phase becomes `Boss` once. A centre banner says
  "The crabnado comes ashore! Slam it into the rocks" for about 3 s.
- **Movement.** The boss moves toward the player at 5 units/s. If its sampled wave height > 1.3, its knock is
  `push * 0.9`, otherwise the knock decays by `exp(-3 dt)`. It's pushed out of rocks with radius 3 and clamped at
  arena radius − 8. Within 4 units of the player it drains 25 HP/s.
- **Slam.** When it's pushed out of a rock with knock speed > 6 and `slam_cooldown <= 0`: `slams++`,
  `slam_cooldown = 2.5`, knock *= −0.3, and a "SLAM! n/3" banner. Store `slams` and `slam_cooldown` in `RunState`
  (reflected). HUD: "CRABNADO · slams left 3" under the shell bar.
- **"Fixed, telegraphed rhythm"** keeps the every-third-crest cadence (one big crest every 18 s), which the prototype
  playtest liked. It also gives a fight of about 1 minute, which fits the 5–6 minute run. During the boss: no new
  cross-swells, no rogues, and the wind keeps shifting. The telegraph label always shows "BIG CREST in 12.3 s" for the
  boss, not only in the last 2.2 s. Riptide still works and is the player's way to speed this up.
- **Spawns.** The spawn rate ×0.35 during the boss.
- **Win.** On the third slam, set `phase = Won`, pause the scene, hide the crabnado's visuals (a short-lived particle
  emitter is optional), and show "The crabnado breaks apart!". If ticket 13 has landed, open its results modal with
  the victory heading. If not, show a placeholder label like "Swept away".
- **Fallback.** The user decides at jam time whether to switch to the "60 s of storm then victory" cut. Don't build
  it unless asked.
