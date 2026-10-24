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
