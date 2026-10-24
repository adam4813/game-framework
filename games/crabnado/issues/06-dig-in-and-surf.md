# 06: Dig in, stamina and surfing

Spec: [spec](../spec.md). Read: [Wind and waves](../spec/wind-and-waves.md) (dig in, surf).

**What to build:** Crests now act on the player too, and the player answers them. Holding the dig-in action roots the
hermit crab: wave and wind push are ignored, movement is ignored, weapons keep firing, and stamina drains. Stamina
refills when not dug in, and a HUD bar shows it. With no stamina, dig in stops working. When not dug in, a passing
crest carries the player as extra velocity on top of steering, a surf burst. A "deep roots" stamina upgrade joins the
upgrade registry.

**Blocked by:** 05 (Waves and tumble).

**Status:** ready-for-agent

- [ ] Without digging in, a crest visibly carries the player, and steering still works while surfing.
- [ ] Holding dig in through a crest keeps the player in place while weapons keep firing.
- [ ] Stamina drains while dug in, refills when released, and dig in fails at 0. The HUD bar shows it.
- [ ] Dig in is a game action bound through the input layer, not a raw key code in logic.
- [ ] The "deep roots" upgrade appears in the upgrade choices and raises stamina.
- [ ] Checked in the running game (e.g. read stamina over the Remote API while holding dig in via a key override).

## Decisions

Settled 2026-10-24. Values are the prototype's, in `crabnado_constants.hpp`.

- **Binding.** Space is the dig-in key. Add `bool dig_in` to `PlayerIntent`, set in `ReadControls` from `IsKeyDown`.
- **Component.** Add a new reflected `Stamina` component on the player: `value`, `max` (100), `regen` (18/s),
  `drain` (30/s), `exhausted`, `dug_in`. Use it rather than growing `PlayerStats`, so upgrades read `"Stamina.max"`
  etc.
- **Exhaustion.** At 0 stamina, set `exhausted`, and dig in stays off until stamina refills to 25% of max, even if
  Space is still held. This stops the dig-in flickering on and off at 0 that the prototype has. The HUD bar turns
  grey while exhausted.
- **Dug in.** Ignore move input, wind push and wave push, and clear the surf knock. Contact damage still applies, and
  weapons still fire. Visual: squash the player's y scale to about 0.7 and sink it by 0.35. HUD text "DUG IN".
- **Surf.** When not dug in and sampled height > 0.3, set the player's knock to the wave push. Otherwise decay it by
  `exp(-3 dt)`. Velocity is steering + wind push + knock. The player rides the water at `height * 0.75`. HUD text
  "SURFING" while the knock is above 4 units/s.
- **HUD.** A stamina progress bar under the HP readout, using the retained UI progress bar like the shell bar.
- **Deep Roots.** Add an entry to `upgrades.json`: "Max stamina +40, refills 30% faster". Effects: `Stamina.max`
  add 40, `Stamina.value` add 40, `Stamina.regen` multiply 1.3.
