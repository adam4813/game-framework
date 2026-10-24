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
