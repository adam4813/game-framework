# 05: Waves and tumble

Spec: [spec](../spec.md). Read: [Wind and waves](../spec/wind-and-waves.md),
[Presentation](../spec/presentation.md) (water visuals), [Testing](../spec/testing.md).

**What to build:** The core mechanic. A wave field module (a pure function of position, time and wind history that
returns height and push) drives everything. Crests travel downwind in sets about 6 s apart, with every third crest
big. The water is a grid of tinted, raised quads whose height and colour come from the wave field, and an upwind foam
line gives about 2 s of warning. When a crest passes an enemy above the tumble threshold, the enemy is stunned and
knocked along the push, then recovers and chases again. The player is not pushed yet (ticket 06 adds dig in and surf).
Only this one swell direction exists. Crossing swells and rogue waves come in ticket 07.

**Blocked by:** 04 (Wind).

**Status:** ready-for-agent

- [ ] Crests are visible on the water, travel in the wind's direction, and turn when the wind shifts.
- [ ] A foam line warns of each crest about 2 s ahead, and every third crest is visibly bigger.
- [ ] Crests tumble crabs: they stop chasing, get carried downwind, then recover.
- [ ] Every system that needs wave force samples the one wave field, and no consumer computes waves itself.
- [ ] Holds the frame budget from ticket 01 with the horde at its cap and the water grid drawn.
- [ ] Checked in the running game (e.g. set the wind over the Remote API and watch crests and tumbling follow).
- [ ] Optional: if the wave field logic proves easy to break, add the wave-field behaviour tests described in the
      testing section. Not required.
