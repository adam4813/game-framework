# 09: Shrimp and lobsters

Spec: [spec](../spec.md). Read: [Content](../spec/content.md) (enemy types).

**What to build:** Enemy types move into a JSON registry (HP, speed, radius, contact damage, XP, scale, colour,
walks-sideways, minimum crest size that tumbles it, and first appearance time), and the crab becomes its first entry.
Shrimp (fast, fragile, face forward) start appearing around 0:25. Lobsters (slow, tanky, bigger, worth more shells)
start around 1:00 and only tumble on big crests. The crabnado picks types by weighted chance from those unlocked. Enemy
HP scales with the run clock.

**Blocked by:** 02 (Core loop), 05 (Waves and tumble).

**Status:** done

- [x] Shrimp appear from about 0:25 and lobsters from about 1:00, each looking and moving as described.
- [x] Small crests tumble crabs and shrimp but not lobsters, and big crests tumble all three.
- [x] Lobsters drop more shells.
- [x] Changing a value in the enemy JSON (e.g. shrimp speed) changes the game with no rebuild.
- [x] Checked in the running game (e.g. set the run clock to 1:05 over the Remote API and query enemy types).

## Decisions

Settled 2026-10-24.

- **File.** `assets/data/crabnado/enemies.json`, an object keyed by id (`crab`, `shrimp`, `lobster`), using the
  prototype `TYPES` values. Fields: `hp`, `speed`, `radius`, `contact_dps`, `xp`, `scale`, `color` ([r,g,b,a]),
  `walks_sideways`, `tumble_min` (0.55 / 0.55 / 1.4), `appears_at` (0 / 25 / 60), `weight` (65 / 27 / 8).
- **Load on scene Load,** not in the module constructor, so a JSON edit shows on the next Play or Retry (same "no
  recompile" meaning as ticket 08). Remove the `kCrab*` constants that move into the JSON.
- **Pick.** A weighted roll from `RngState` over the types with `appears_at <= RunState.clock`. That's about 29%
  shrimp from 0:25 to 1:00, then 65/27/8.
- **Enemy component.** Copy the type's stats onto `Enemy` at spawn (what it does now), plus `int type` (registry
  index), `tumble_min` and `walks_sideways`. To check types over the Remote API, read `type` together with
  `speed`/`radius`. `ThrowCrab` becomes `ThrowEnemy(world, parent, typeIndex, from, target)`.
- **Look.** The same body + 2 claws model for all three, scaled by `scale` and tinted by `color` (shrimp pink
  `#ff8fa3`, lobster dark red `#8b1e1e`). `walks_sideways: false` faces the direction of travel. Don't build new
  meshes.
- **HP growth** stays `hp * (1 + clock / 240)` and applies to every type.

## Comments

- Checked on desktop (Release) over the Remote API: with the run clock set to 1:05, `Enemy` rows held all three types
  (registry index is id order: 0 crab, 1 lobster, 2 shrimp) with the JSON stats (shrimp speed 7.5, radius 0.6, faces
  forward; lobster speed 2.8, radius 1.4, `tumble_min` 1.4, `xp` 4). The registry is re-read on every arena Load, so an
  edit to the copied `enemies.json` shows on the next Play.
- Not checked by eye: the shrimp/lobster look, or a small crest passing a lobster (the tumble check is now
  `wave.height > enemy.tumble_min`). Ctest: 32/32.
