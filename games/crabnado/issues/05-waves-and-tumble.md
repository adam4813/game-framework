# 05: Waves and tumble

Spec: [spec](../spec.md). Read: [Wind and waves](../spec/wind-and-waves.md),
[Presentation](../spec/presentation.md) (water visuals), [Testing](../spec/testing.md).

**What to build:** The core mechanic. A wave field module (a pure function of position, time and wind history that
returns height and push) drives everything. Crests travel downwind in sets about 6 s apart, with every third crest
big. The water is a grid of tinted, raised quads whose height and colour come from the wave field, and an upwind foam
line gives about 2 s of warning. When a crest passes an enemy above the tumble threshold, the enemy is stunned and
knocked along the push, then recovers and chases again. The player is not pushed yet (ticket 06 adds dig in and surf).
Only this one swell direction exists. Crossing swells and rogue waves come in ticket 07.

**Prototype:** the wave rules here were playtested in the three.js prototype (`prototypes/crab-jam-prototype.html`,
Crabnado mode, commit `cf4755e`). Verdict: dig in, surfing, tumble, rock slams, crossing swells, rogue waves and the
3-slam boss all felt good as specified. The only issue was the spawner parking at the arena edge, now fixed in
[The run](../spec/run.md). The prototype's wave field is a working reference for crest timing, big-crest cadence and
pool calming.

**Blocked by:** 04 (Wind).

**Status:** ready-for-agent

- [x] Crests are visible on the water, travel in the wind's direction, and turn when the wind shifts.
- [x] A foam line warns of each crest about 2 s ahead, and every third crest is visibly bigger.
- [x] Crests tumble crabs: they stop chasing, get carried downwind, then recover.
- [x] Every system that needs wave force samples the one wave field, and no consumer computes waves itself.
- [x] Holds the frame budget from ticket 01 with the horde at its cap and the water grid drawn.
- [x] Checked in the running game (e.g. set the wind over the Remote API and watch crests and tumbling follow).
- [ ] Optional: if the wave field logic proves easy to break, add the wave-field behaviour tests described in the
      testing section. Not required.

## Decisions

Judgement calls settled before implementation (2026-10-24). Prototype values come from
`prototypes/crab-jam-prototype.html` (`WAVE`, `makeWaveField`, `CrabMode`). Put every number in
`crabnado_constants.hpp`.

- **Module shape.** New `waves.hpp/.cpp`. A `WaveField` singleton holds the wave state: its own `time` (advanced by
  a pausable `WaveUpdate` system, like `Wind.time`), a fixed `std::array` of up to 4 swells, and (from ticket 07)
  rogues. Fixed-size arrays keep it reflectable. Each swell has a bearing (same convention as `Wind.direction`, use
  `WindDirection`), `t0`, `amp`, and `follow`/`grow`/`fade` flags. The pure function is
  `WaveSample SampleWaves(const WaveField&, std::span<const TidePool> pools, glm::vec2 position)`, which returns
  `{height, push}`. Ticket 05 passes an empty pool span, and ticket 08 fills it. Nothing else computes waves.
- **Crest maths: copy the prototype.** `speed 11`, `period 6`, `width 3.4`, `bigAmp 2.2` (crest index
  `n mod 3 == 2` is big), `pushK 9`. Phase is `(speed * (time - t0) - dot(pos, dir)) / (speed * period)`. Height is a
  Gaussian on the distance to the nearest crest, and push is `dir * height * pushK`. In this ticket, one swell follows
  the wind's current direction every frame (`follow`), as in the prototype.
- **Clock.** Crests run on `WaveField.time`. The stage gates in ticket 07 read `RunState.clock`, so jumping the clock
  over the Remote API changes the stage without a jump in crest phase.
- **Water grid.** One entity per tile: a `CubePrimitive` with an inline `Material`, cell size 2.6 and 0.97 fill,
  covering the arena circle plus 2 units (about 2,300 tiles), as children of the scene root. Each frame a
  `WaterTiles` system sets tile y so the top sits at `height * 0.75` (rest = 0). It sets the tint to water `#2f8fa8`,
  lerped toward foam `#eaf7fa` by `clamp((h - 0.35) / 1.3, 0, 1)`. Move the `Sand` quad down to y = -0.6 so the sand
  and water don't z-fight. Enemies and shells ride the water: their y is `height * 0.75`.
- **The "foam line about 2 s ahead"** is the crest's own foam tint. At 11 units/s a crest shows on screen well over
  2 s upwind. Don't draw a separate line ahead of it. Add one HUD **telegraph label** (centre, below the shell bar):
  "BIG CREST in 1.8 s" when the next big crest of the primary swell reaches the player within 2.2 s. Later tickets
  reuse this label for rogue, Riptide and boss warnings, in priority order rogue > Riptide > big.
- **Tumble: prototype values.** A landed enemy whose sampled height is above its tumble threshold gets
  `tumble = 0.9 s` and `knock = push`. Knock decays by `exp(-3 dt)` only once height is back under the threshold.
  The crab threshold is `kCrabTumbleMin = 0.55` (it moves into the enemy registry in ticket 09). Add `tumble`,
  `knock` and `spin` to `Enemy` (or a separate reflected `Tumble` component, the implementer's choice).
- **While tumbling,** an enemy skips chasing and the "queue behind a touching neighbour" rule, so the crest can carry
  it. It still separates from overlaps, still takes claw damage, and deals **no** contact damage. It also spins on
  its local x axis (rotate the root, about 9 rad/s), so the tumble reads.
- **Shells drift** with `push * 0.4` until they start flying to the player (prototype). This is cheap, and it's what
  "every system samples the field" means for this ticket. Bubbles, player and boss sample it in tickets 10, 06 and 12.
- **Wind push is unchanged.** It stays separate from the wave push.
- **Tests: skip** (see `spec/testing.md`, and the user's "only if truly needed"). Remote API checks cover it.

## Comments

Checked on desktop over the Remote API (port 27751): swell direction follows `Wind.direction` when set; enemies tumble as crests pass; 1,000 enemies with about 2,400 water tiles held 59 fps. The wave-field behaviour tests were skipped (optional).
