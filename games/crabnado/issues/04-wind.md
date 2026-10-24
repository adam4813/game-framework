# 04: Wind

Spec: [spec](../spec.md). Read: [Wind and waves](../spec/wind-and-waves.md) (wind only),
[Presentation](../spec/presentation.md).

**What to build:** The breeze becomes visible and felt. A wind singleton holds direction, strength and gust. The
direction shifts every 20–30 s, turning over a few seconds, and a warning shows on the HUD before the turn. Gusts
briefly raise strength. A north-up HUD compass shows the wind's direction and strength (as a Beaufort-style force).
Wind streaks move across the screen with it. The wind pushes the player and landed enemies slightly downwind, and
the crabnado drifts downwind.

**Blocked by:** 02 (Core loop).

**Status:** ready-for-agent

- [ ] The compass and streaks show the current wind, and a warning appears before each shift.
- [ ] Shifts happen every 20–30 s, and gusts are noticeable in the streaks and the compass.
- [ ] Walking upwind is slightly slower than walking downwind, and idle enemies drift downwind.
- [ ] The crabnado drifts with the wind but stays inside the arena.
- [ ] Wind shifts and gusts draw from `RngState`.
- [ ] The wind singleton is reflected: setting its direction over the Remote API turns the compass and the streaks.
