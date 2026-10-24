# 15: Balance pass

Spec: [spec](../spec.md). Read: [The run](../spec/run.md) (spawn rate, enemy HP, shells and molting),
[Content](../spec/content.md) (enemy types, upgrades).

**What to build:** The run's pacing gets tuned against the real game instead of the prototype's numbers. Three
values are off today:

- **Molts come too fast.** The shells-per-molt curve (6, then +5 per molt) is the prototype's, and the horde feeds it
  quickly: in a ticket 04 test run the 4th molt came at about 0:18 with 37 kills. Upgrades should feel like milestones,
  not interruptions.
- **Standing still is lethal.** From ticket 02's tuning note, an idle player had 11 HP left at 0:14, and in ticket 04
  testing an idle crab died at about 0:15. Contact damage and the early spawn rate together are too harsh.
- **The spawn curve** (`1.6 + 0.09 * t` per second) and enemy HP growth (×(1 + t / 240)) are untested past the first
  minute.

Tune the values (the shells-per-molt curve, crab contact damage, the spawn curve, enemy HP growth, and enemy XP values
from the ticket 09 registry) so a 5-minute run has a steady rhythm. Keep the numbers as tuning data (constants or JSON),
not logic changes. Tuning the "crabnado surges" burst (run.md story 9) is out of scope: no ticket builds it yet.

**Blocked by:** 03 (Shells and molting), 09 (Shrimp and lobsters), because enemy XP values change there.

**Status:** needs-human-playtest

- [ ] The first molt lands at about 0:20–0:30 of normal play, and later molts come no faster than one every 25–40 s.
- [ ] A player who stands still with no upgrades survives at least 30 s.
- [ ] A player who moves and fights reaches the 3:00 rogue-wave stage on most runs without upgrades carrying them
      trivially.
- [ ] Tuned values live in `crabnado_constants.hpp` or the JSON registries, with a short comment on each changed value.
- [ ] Checked by playing several full runs on desktop, with molt times and the time of death or survival noted in
      `## Comments` (read `RunState` over the Remote API).

## Decisions

Settled 2026-10-24.

- **Changing the curve's shape is allowed.** A linear shells-per-molt curve can't give a steady 25–40 s cadence while
  the kill rate rises linearly with the spawn rate. Change `ShellsNeeded` to
  `round(kShellsFirstMolt * pow(kShellsGrowth, level - 1))`. Start from `kShellsFirstMolt = 40` and
  `kShellsGrowth = 1.35`, then tune.
- **Starting points for "standing still is lethal":** crab `contact_dps` 10 → 6, `kSpawnRateBase` 1.6 → 1.0, and a
  3 s grace before the first throw. Tune from there. Prefer changing the early spawn rate over lowering `kSpawnRateGrowth`,
  because the late game needs pressure.
- **What "normal play" means for an agent.** Drive the player with a Remote API input override: walk a circle of
  radius about 30 around the centre, reversing direction every 20 s, and always pick upgrade 1. "Stands still" means
  no input and no picks. Do at least 3 runs of each and note molt times and the time of death in `## Comments`. Then
  the user does one human playtest to sign off, so the agent's runs don't close the ticket on their own.
- **Surges** stay out of scope (no ticket builds them; run.md story 9 is an open gap).

## Comments

2026-10-24, agent runs (desktop Release, Remote API; scripted bot: Choice1 on every molt, 8-direction circle with
reversal every 20 s for "walk"). Final values: `kShellsFirstMolt` 18, `kShellsGrowth` 1.3, `kSpawnRateBase` 1.0,
`kSpawnRateGrowth` 0.035 (was 0.09), `kFirstThrowDelay` 3 s, crab `contact_dps` 4 (was 10; shrimp, lobster and all XP
values unchanged, so the registry's per-enemy comment lives here since JSON has none).

- **Idle (no input, no picks), final values, 3 runs:** alive at the first molt every time: 31.6 s, 30.8 s, 30.8 s
  (23-24 kills). Meets "survives 30 s".
- **Walk bot, final values, 2 runs:** died at 111.9 s (molts 52.1, 68.6, 72.2, 88.8, 98.2) and 98.2 s (molts 46.4, 55.2,
  73.9, 82.9). **Does not meet** "reaches 3:00 on most runs" or the 20-30 s first-molt target.
- Earlier walk tunings (4 runs each pair): 40/1.35/0.09 died 81-101 s; 25/1.3/0.06 died 70-77 s; 25/1.3/0.035 died
  104-107 s. Lowering spawn growth and contact damage moved death by only ~30 s.
- Read: the bot circles at radius 30, so it collects far fewer shells than it has kills (43 kills for 18 shells) and
  lets the horde pile up; molts land at ~50 s for it but ~31 s idle. The bot is a poor proxy for a human, who kites and
  uses waves, so the walking numbers likely understate real play. **Needs the human playtest** (per Decisions) to
  confirm the first molt lands at 20-30 s and 3:00 is reachable, and to retune `kShellsFirstMolt` / `kSpawnRateGrowth`
  from there. Later molts are 15-30 s apart for the bot (the pile of uncollected shells makes some come back-to-back),
  so the 25-40 s cadence is also unconfirmed.
