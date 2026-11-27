# 02: `GameTime` singleton

**What to build:** A game-agnostic `GameTime` world singleton and `TimeAdvanceSystem` that
tracks in-game time progression. The singleton holds: `tick` (raw frame counter), `day`
(1-based within a season), `season` (Spring/Summer/Autumn/Winter), `hour` (0–23), and `phase`
(Dawn/Day/Dusk/Night derived from hour). `TimeAdvanceSystem` (Pausable, OnUpdate) increments
tick each frame and rolls over hour → day → season at configurable thresholds.

The system also supports **time hooks**: named thresholds (e.g. `midnight`, `dawn`, `bedtime`)
at which a Flecs event fires on the world singleton. Hooks are registered by name with an
`hour` (and optional `minute`) value. Any system or observer can subscribe to these events —
e.g. a "pass out" mechanic would observe `bedtime`, an NPC schedule would observe `dawn`. No
hooks are wired up by default in this ticket; the mechanism just has to exist and be testable.

The singleton carries no farm-specific data and is a candidate to promote to an engine module
later.

**Blocked by:** None (can start immediately)

**Status:** done

- [x] `GameTime` world singleton is set and readable from any system
- [x] `TimeAdvanceSystem` increments tick each frame and is tagged Pausable
- [x] Hour, day, and season roll over at configurable thresholds
- [x] `phase` derives correctly from `hour` (Dawn/Day/Dusk/Night)
- [x] Named time hooks can be registered; the system fires the corresponding Flecs event the
  first tick the clock reaches or passes the hook's hour
- [x] Each hook fires exactly once per day (not continuously while the hour matches)
- [x] Thresholds are `constexpr` in `constants.hpp`, not magic numbers
- [x] Unit tests: rollover, phase derivation, hook fires once at the right tick, hook does not
  re-fire until the next day


## Comments

**Done** — implemented on `feat/farm-prototype` (`src/game/game_time.*`, 14 unit tests in
`tests/unit/game_time_test.cpp`). `GameTime` singleton + `TimeAdvanceSystem` (OnUpdate, Pausable) with
named time-hook events firing once per day. Thresholds are constexpr in `constants.hpp`. Ticket 04 will
call `RegisterGameTime(world)` from the farm module. Commits: afa772d, merged in 55f4950.
