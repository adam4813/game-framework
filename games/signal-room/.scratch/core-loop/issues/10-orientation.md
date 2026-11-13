# 10: Orientation

Spec: [Signal Room spec 2: Core loop](../spec.md). Read: [Shift flow, Briefing & Orientation](../spec/shift-flow.md) (Orientation), [Tickets, Window & Collection Schedule](../spec/tickets-and-schedule.md) (pinned entries), [Testing decisions](../spec/testing.md).
Base branch: `feat/signal-room` (branch off it, land per `games/signal-room/AGENTS.md`)
Model: sonnet

**What to build:** Shift 1's held opening. After the Briefing, a Shift with `orientation: true` enters Orientation: its
pinned, unlosable NORMAL Ticket is On Air at once, and the Window clock, the Collection Schedule and every gauge wait.
Clearing that Ticket scores it as a green Clear, adds it to par, starts the Handler Chain and moves the Shift to
Running, where the Window starts its 4:00. The HUD clock shows the hold.

**Blocked by:** 09 (Shift flow: the Briefing card, results and Retry).

**Status:** ready-for-agent

- [ ] Shift file `orientation` flag (default false). With it set, the schedule must have a pinned `at: 0`,
      `unlosable: true` entry (a load error otherwise); Shift 1 sets both.
- [ ] Phase `Orientation` between Briefing and Running. While in it, the Window, the schedule and gauges are frozen
      and the unlosable Ticket can't be Lost. Its Clear scores at the green share, adds par, steps the chain, then the
      phase becomes Running. Without the flag, Briefing goes straight to Running.
- [ ] The HUD clock shows the full Window with a "HELD" tag in Orientation.
- [ ] Panel: End Orientation button (moves to Running as if Cleared, without scoring).
- [ ] Harness tests:
  - after dismissing the Briefing, Shift 1 is in Orientation with one On Air 7.0 NORMAL Ticket; 60 s later the clock
    still reads 4:00, no other Ticket has arrived and the Ticket is still open;
  - clearing it scores 20, makes par 20, steps the chain, and the phase becomes Running; 30 s later the clock reads
    3:30;
  - a fixture Shift without the flag goes from Briefing to Running;
  - a Shift with `orientation: true` and no pinned `at: 0` unlosable entry fails to load.
- [ ] Runtime: desktop build; through the Remote API dismiss the Briefing, wait, confirm the hold, clear the Ticket and
      watch the clock start. No asserts.

## Comments
