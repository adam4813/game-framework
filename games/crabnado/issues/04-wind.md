# 04: Wind

Spec: [spec](../spec.md). Read: [Wind and waves](../spec/wind-and-waves.md) (wind only),
[Presentation](../spec/presentation.md).

**What to build:** The breeze becomes visible and felt. A wind singleton holds direction, strength and gust. The
direction shifts every 20–30 s, turning over a few seconds, and a warning shows on the HUD before the turn. Gusts
briefly raise strength. A north-up HUD compass shows the wind's direction and strength (as a Beaufort-style force).
Wind streaks move across the screen with it. The wind pushes the player and landed enemies slightly downwind, and
the crabnado drifts downwind.

**Blocked by:** 02 (Core loop).

**Status:** done

- [x] The compass and streaks show the current wind, and a warning appears before each shift.
- [x] Shifts happen every 20–30 s, and gusts are noticeable in the streaks and the compass.
- [x] Walking upwind is slightly slower than walking downwind, and idle enemies drift downwind.
- [x] The wind biases the crabnado's next waypoint downwind, but it keeps roaming the interior and never drifts out
      to the arena edge.
- [x] Wind shifts and gusts draw from `RngState`.
- [x] The wind singleton is reflected: setting its direction over the Remote API turns the compass and the streaks.

## Comments

- Checked in the running game over the Remote API: with `direction` set to 0 (blowing north), walking north covered
  15.3 units/s and walking south 12.8 units/s, and the idle crab drifted about 1 unit/s. Forcing `shift_timer` to
  4.5 s set `warning` 3 s ahead, then a 66° turn took about 4 s, and the next shift landed 20.7 s out. Over 25 crabnado
  waypoints in a south wind, mean z was +11.8 (expected ≈ +12) and none was past the 42-unit roam radius. Screenshots
  showed the compass arrow, the force readout, the "Wind shifting!" warning and the streaks following the set direction.
- Enemy drift is added to every landed enemy's step in `HordeChase`, so it shows on enemies that are blocked in a
  crowd. It wasn't measured on its own.
- The wind keeps blowing (and the streaks keep moving) only while the scene isn't paused, so a molt or a lost run
  freezes it.
