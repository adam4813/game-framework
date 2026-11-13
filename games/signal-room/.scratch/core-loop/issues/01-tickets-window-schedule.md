# 01: Tickets, the Window & the Collection Schedule

Spec: [Signal Room spec 2: Core loop](../spec.md). Read: [Tickets, Window & Collection Schedule](../spec/tickets-and-schedule.md), [Radios & the Antenna Coupler](../spec/radios-and-coupler.md) (the frequency table and the Shift's `radios` only), [Debugging & tuning](../spec/debugging.md), [Testing decisions](../spec/testing.md).
Base branch: `feat/signal-room` (branch off it, land per `games/signal-room/AGENTS.md`)
Model: sonnet

**What to build:** The Ticket simulation, with no route and no HUD yet. Loading Shift 1 starts a 4:00 Window. A seeded
Collection Schedule brings Tickets: each waits on Stand-by while its overfilled gauge drains, goes On Air with a timer
capped to the Window, and becomes a Lost Intercept if it runs out. The rack caps at 5, an empty rack halves the wait,
Last Call stops new On Air Tickets in the final 20 s, and at 0:00 the Shift ends. A transmission event clears the most
urgent matching Ticket (the Telex raises it in ticket 02; here only tests and the debug panel do). The Room panel can
spawn Tickets, freeze the Window and pin the seed.

**Blocked by:** none.

**Status:** done

- [x] A Ticket rules data file holds the tiers (`normal`, `high`, `flash` with `on_air_time`, `base`, `overfill`
      range, `label`), the gauge bands and shares, the Handler Chain steps, the Lost Intercept penalty, `rack_cap`,
      `last_call`, `empty_rack_wait_factor`, default star fractions and a callsign pool, with the spec's starting
      values. (Scoring reads it in ticket 04.)
- [x] A frequency table data file (`7.0` cyan, `7.5` magenta, `8.0` violet, as palette tokens, ordered low → high).
- [x] The Shift file gains `radios` (placement → frequency), `window` and `schedule` (`interval`, `tier_weights`,
      `frequency_weights`, `min_repeat_gap`, `pinned`). Shift 1 gets `radios: {"Radio1": "7.0"}`, `window: 240` and a
      schedule (25–35 s, NORMAL 0.8 / HIGH 0.2, all weight on 7.0). Load errors name the file and entry for an unknown
      tier or frequency, a frequency on no in-service Radio, and a malformed pinned entry.
- [x] Ticket entities and the Shift clock singleton (phases `Running` and `Ended` for now; tickets 09–10 add the rest),
      all reflected and pre-registered.
- [x] Gauge, Stand-by → On Air (timer = `min(tier, window left)`), Lost Intercept, rack cap with waiting, empty-rack
      halving, `min_repeat_gap`, pinned entries, Last Call and the hard stop (open Tickets unscored, Items destroyed),
      all per the spec section.
- [x] A transmission event clears the On Air Ticket of that frequency with the least `on_air_left` (lowest arrival on
      a tie); no match does nothing.
- [x] Randomness comes from the world `RngState`, seeded per attempt; a pinned seed repeats the schedule exactly.
- [x] Events raised: `TicketArrived`, `TicketOnAir`, `TicketCleared`, `LostIntercept`, `LastCall`, `ShiftEnded`.
- [x] Room panel Shift section: phase and Window left; Freeze Window; spawn Ticket (tier, frequency, overfill or On
      Air); seed shown, pin/unpin.
- [x] The harness gains a pinned-seed Shift load and `StepSeconds`, plus read-only helpers for the Shift clock and open
      Tickets.
- [x] Harness tests:
  - a 30% overfill NORMAL Ticket is Stand-by for 27 s, On Air for 90 s, then Lost;
  - a Ticket going On Air with 50 s of Window left gets a 50 s timer;
  - with the rack full a due Ticket waits and arrives when a slot frees; an empty rack halves the remaining wait;
  - the same pinned seed gives the same arrivals (time, tier, frequency) twice; a different seed differs;
  - a pinned entry arrives at its `at`;
  - nothing goes On Air in the last 20 s; at 0:00 the phase is Ended and open Tickets are neither Cleared nor Lost;
  - a transmission with two On Air 7.0 Tickets clears the one with less time left; a 7.5 transmission with only 7.0
    open changes nothing;
  - Freeze Window stops the clock and every gauge;
  - a Shift naming an unknown tier fails to load with its name in the error.
- [x] Runtime (per `AGENTS.md`): desktop build, Remote API on 27760; query Tickets and the Shift clock as they arrive,
      spawn one from the panel, freeze, and let one go Lost. No asserts in the log.

## Comments

**Retro (2026-10-18).**
- Shift clock singleton is `ShiftWindow` (the glossary lists "shift clock" under Avoid). Glossary gained Rack, Last Call, Seed.
- Randomness: the schedule draws from `ScheduleRng` (an `engine::ecs::RngState` of its own, seeded per attempt from the world `RngState` or the pin), not the world `RngState`: dust puffs also draw from the world stream and would change a pinned seed's schedule. Panel spawns draw callsigns from the world stream for the same reason.
- `TicketArrived`/`TicketOnAir` are raised by an `OnSet` observer: emitting from the spawning system fires before the deferred `set<Ticket>` lands and matches nothing.
- Tests live in the `desktop-test` preset (`cmake --preset desktop-test`); `desktop-release` does not build them. 85 -> 99 tests.
- Runtime (desktop, 27760): Tickets arrived on the Shift 1 schedule, Freeze Window held the Window and gauges, a Ticket forced to 1.5 s left went Lost, no asserts. The ImGui spawn/seed buttons were not clicked (no Remote API path); Tickets spawned through the same `SpawnTicket` are covered by tests.
- Code review follow-ups not done: gate the three systems with `ecs::Pausable` when the Room gets a pause pipeline; reflect `TicketRules`/`Schedule`/`ScheduleRng` members for the Explorer; split `ticket_simulation.cpp` (clock / gauges / schedule) and the schedule parsing out of `shift.cpp`; unify the `Weights` alias and the `Raise`/`RaiseOnClock` helpers; hard stop deletes Items globally and does not end Holds (ticket 02+).
