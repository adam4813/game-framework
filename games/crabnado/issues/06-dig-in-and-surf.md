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
