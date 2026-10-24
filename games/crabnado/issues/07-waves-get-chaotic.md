# 07: Waves get chaotic

Spec: [spec](../spec.md). Read: [Wind and waves](../spec/wind-and-waves.md) (ramp to chaos), [Testing](../spec/testing.md).

**What to build:** The waves go from easy to learn to hard to master over the run. From 0:00 to 1:30 there is one
swell (as built in ticket 05). From 1:30, after each wind shift, the previous swell keeps running alongside the new
one, so crests from two directions cross and their heights add. From 3:00, rogue waves (single large crests from a
direction near the wind's) arrive with a short warning. The water visuals, tumbling and surfing all follow, because
they read the same wave field.

**Blocked by:** 05 (Waves and tumble).

**Status:** ready-for-agent

- [ ] Before 1:30 all crests travel along the current wind.
- [ ] After a wind shift past 1:30, crests from two directions are visible and stack where they meet.
- [ ] After 3:00, rogue waves appear with a warning.
- [ ] Rogue wave timing and direction draw from `RngState`.
- [ ] Checked in the running game by setting the run clock over the Remote API (e.g. jump to 1:25, 2:55).
- [ ] Optional: the wave-field ramp tests from the testing section, if the ramp proves fragile. Not required.

## Decisions

Settled 2026-10-24. Port the prototype's `updateWaves`. Constants: `kCrossSwellAt = 90`, `kRogueAt = 180`.

- **Cross-swells.** Detect a shift when `Wind.shifts` changes. If `RunState.clock >= 90`, every existing swell stops
  following the wind and fades (`amp -= dt / 45`). A new swell is added along the shift's **target** bearing
  (`direction + turn`). Its `t0` is now plus a random 0 to `period` seconds, and its `amp` grows from 0 to 1 over 4 s.
  Before 1:30, a shift just turns the one swell (ticket 05 behaviour). Drop swells whose `amp < 0.01`. With 4 slots,
  if all are full, the oldest fading swell is replaced.
- **Primary swell** is the newest non-fading swell. Telegraphs and Riptide (ticket 11) use it.
- **Rogues.** From 3:00, the first rogue comes after 6–10 s, then every 12–20 s. Its bearing is the wind's ± 0.6 rad,
  its amp is 2.6 and its width is `3.4 * sqrt(1.6)`. Time it to reach the player 2.5 s after it spawns
  (`t0 = time + 2.5 - dot(playerPos, dir) / speed`). Remove it once it has travelled the arena radius + 30 past the
  centre. All rolls draw from `RngState`. A rogue is not part of the big-crest cadence. With amp 2.6 it tumbles
  lobsters.
- **Warning.** "ROGUE WAVE in 2.1 s" in the telegraph label from ticket 05 (red, the highest priority), shown when
  it's under 2.6 s from the player. On a new cross-swell, show "A cross-swell is building" in the same label for
  about 2 s.
- **HUD.** Show the wave stage next to the run clock: "Steady swell", "Crossing swells" or "Rogue waves".
- **Remote checks.** Setting the clock to 1:25 and waiting for a shift (or setting `Wind.shift_timer` low) must show
  two crossing directions. Setting it to 2:55 must show a rogue within about 20 s.
- **Tests: skip.** The Remote API clock jump makes the ramp quick to check.
