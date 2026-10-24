# 09: Shrimp and lobsters

Spec: [spec](../spec.md). Read: [Content](../spec/content.md) (enemy types).

**What to build:** Enemy types move into a JSON registry (HP, speed, radius, contact damage, XP, scale, colour,
walks-sideways, minimum crest size that tumbles it, and first appearance time), and the crab becomes its first entry.
Shrimp (fast, fragile, face forward) start appearing around 0:25. Lobsters (slow, tanky, bigger, worth more shells)
start around 1:00 and only tumble on big crests. The crabnado picks types by weighted chance from those unlocked. Enemy
HP scales with the run clock.

**Blocked by:** 02 (Core loop), 05 (Waves and tumble).

**Status:** ready-for-agent

- [ ] Shrimp appear from about 0:25 and lobsters from about 1:00, each looking and moving as described.
- [ ] Small crests tumble crabs and shrimp but not lobsters, and big crests tumble all three.
- [ ] Lobsters drop more shells.
- [ ] Changing a value in the enemy JSON (e.g. shrimp speed) changes the game with no rebuild.
- [ ] Checked in the running game (e.g. set the run clock to 1:05 over the Remote API and query enemy types).
