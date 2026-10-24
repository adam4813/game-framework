# 12: Crabnado boss

Spec: [spec](../spec.md). Read: [Arena and boss](../spec/arena-and-boss.md) (boss).

**What to build:** At 5:00 a warning announces the boss, and the crabnado comes ashore and moves toward the player.
Its spawn rate drops so the fight stays readable. Big crests arrive on a fixed, telegraphed rhythm. When a big crest
pushes the crabnado into a rock, that's one slam (with a short cooldown so one crest can't count twice), and the HUD
shows the slams left out of 3. On the third slam the crabnado breaks apart and the run is won.

**Fallback:** if the boss isn't working by the second half of the jam, replace it with 60 s of maximum storm followed
by victory.

**Blocked by:** 08 (Tide-pool arena).

**Status:** ready-for-agent

- [ ] At 5:00 the boss warning shows and the crabnado starts chasing the player.
- [ ] Big crests are telegraphed during the boss.
- [ ] Luring the crabnado in front of a rock as a big crest hits counts one slam, and the HUD counts down from 3.
- [ ] One crest never counts as two slams.
- [ ] The third slam wins the run (a victory state in run state; ticket 13 shows it).
- [ ] Checked in the running game by setting the run clock to about 4:55 over the Remote API.
