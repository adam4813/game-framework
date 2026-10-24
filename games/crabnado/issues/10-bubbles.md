# 10: Bubbles

Spec: [spec](../spec.md). Read: [Content](../spec/content.md) (weapons, upgrades).

**What to build:** The second weapon. Bubbles fire on their own at the nearest landed enemy, drift with the wind and
the wave push as they fly (sampled from the wind singleton and the wave field), and pop on their first hit. The upgrade
registry gains "bubble burst" (+1 bubble per volley, with spread) and "quick bubbles" (faster fire rate).

**Blocked by:** 03 (Shells and molting), 04 (Wind).

**Status:** ready-for-agent

- [ ] Bubbles fire at the nearest enemy and damage it on hit.
- [ ] Bubbles curve downwind in strong wind, so shooting downwind travels further than upwind.
- [ ] Both bubble upgrades show up in molt choices and visibly change the volley.
- [ ] Bubble hits use the spatial grid, and many bubbles at the horde cap hold the frame budget.
- [ ] Checked in the running game.

## Decisions

Settled 2026-10-24. Prototype `bub` values, in constants.

- **Component.** A reflected `Bubbles` weapon on the player: `damage 2`, `cooldown 0.7`, `count 1` (int), `speed 22`,
  `timer`. Each bubble is an entity with a `Bubble {velocity, life 1.6}` and a small pale-blue sphere (radius 0.35),
  as a child of the horde root so it's destroyed on Unload. Cap 400 live bubbles.
- **Targeting.** The nearest landed enemy within 30 units, found with a linear scan once per volley (cheap at one
  scan per 0.7 s). Without a target, don't fire and leave the timer at 0. Volley spread is 0.18 rad between bubbles,
  centred on the target.
- **Drift.** Each frame, add `(windDir * strength * 12 + wavePush * 1.5) * dt` to the bubble's velocity.
- **Hit.** Query the `CrowdGrid` around the bubble. On the first landed enemy within its radius + 0.35, call
  `DamageEnemy` and destroy the bubble. Tumbling enemies can be hit.
- **Upgrades.** `bubble_burst`: `Bubbles.count` add 1. `quick_bubbles`: `Bubbles.cooldown` multiply 0.8 (the
  prototype's numbers; "25% faster" is its label). `ApplyEffect` uses `flecs::cursor` `get_float`/`set_float`, which
  converts for int members. Check `count` goes 1 → 2 over the Remote API. If it doesn't, make `ApplyEffect` handle
  ints, not a float `count`.
