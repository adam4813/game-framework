# Spec 2 · Shift flow, Briefing & Orientation

Part of [Signal Room spec 2: Core loop](../spec.md). The Shift phases, the Briefing card, Orientation's freeze and
pinned Ticket, the results screen and Retry.

## User stories

62. As a player, I want a Briefing card before the Shift with its name, its route (Radio → Decoder → Telex) and a short
    note on the rack and gauge, so that I know what to do before the clock matters.
63. As a player, I want to dismiss the Briefing with the action key (or a click), so that I start when I'm ready.
64. As a player, I want the room visible but still behind the Briefing, so that I can see the floor I'm about to play.
65. As a player with the `keypunch` Modifier on, I want the Briefing's route to include the Keypunch, so that the card
    matches the room.
66. As a new player, I want Shift 1 to open in Orientation, with one NORMAL Ticket On Air at once and the Window clock
    waiting, so that I can learn the route without pressure.
67. As a new player, I want the Orientation Ticket's gauge frozen and impossible to lose, so that I can take my time.
68. As a player, I want clearing the Orientation Ticket to score it as a green Clear, count it toward par and start the
    Handler Chain, so that learning still counts.
69. As a player, I want the Window, the Collection Schedule and the gauges to start on that first Clear, so that the
    real Shift begins when I've done the route once.
70. As a player, I want a results screen at the hard stop showing score, par, stars, Tickets cleared and lost and my
    best chain, so that I know how I did.
71. As a player, I want a Retry on the results screen that restarts the Shift with a fresh schedule, so that I can go
    again at once.
72. As a designer, I want Orientation to be a Shift data flag with its pinned Ticket in the Shift's schedule, so that
    other Shifts can opt in later.
73. As a designer, I want the Briefing's note text in the Shift file, so that each Shift explains what's new.

## Implementation decisions

- **Shift phase** (on the Shift clock singleton): **Briefing** → **Orientation** (only if the Shift sets
  `orientation: true`) → **Running** → **Ended**. Each phase change raises a `ShiftPhaseChanged` event. Reload and
  Retry start a new attempt at Briefing (new seed unless pinned; Ticket, score and Hint state reset; Shift
  re-loaded through the Modifiers).
- **Pausing:** in Briefing and Ended, the Room's simulation systems (movement, Jobs, Tickets) are paused through the
  engine's pause-as-data (ADR 0004). Render, UI and the debug panel keep running.
- **Briefing card** is built with the engine UI module (a `Modal`). It shows the Shift `name`, the **route** and the
  Shift's `briefing.notes` lines. The route is derived: start at the in-service Radio kind's Transform output, follow
  each Item to the in-service Station whose Transform takes it, and stop at a `transmit` Transform. Station display
  names come from the Station catalogue (`display_name`). The `interact` press (or clicking its button) dismisses it.
  In spec 2 it always shows; spec 5 makes auto-show depend on the save slot.
- **Orientation** (Shift `orientation: true`; Shift 1 sets it, and spec 2 always applies it): the Shift's `schedule`
  must have a pinned entry at `at: 0` with `unlosable: true`, which arrives On Air at once (overfill 0). While the
  phase is Orientation, the Window, the schedule and every gauge are frozen. The unlosable Ticket can't become a Lost
  Intercept. On its Clear it scores at the green share whatever the gauge shows, adds to par, steps the chain, and the
  phase becomes Running. The `unlosable` flag lasts only while Orientation holds. Spec 3 hooks the AI Operator's
  clock-on to the Orientation → Running change. Without the flag, Briefing goes straight to Running.
- **Results screen** (engine UI `Modal`, phase Ended): score, par, stars (★ filled / ☆ empty), Cleared, Lost
  Intercepts, best chain multiplier, and a **Retry** button (the `interact` key presses it). Spec 5 adds Requisitions
  and the Shift picker here.
- **Shift file** gains `orientation` (bool, default false) and `briefing.notes` (list of strings). The Station
  catalogue gains `display_name` per kind.
- All text, sizes and colours come from data (HUD layout file and palette).
