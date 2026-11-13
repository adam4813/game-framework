# Spec 2 · Debugging & tuning

Part of [Signal Room spec 2: Core loop](../spec.md). The Room panel's Shift section and Explorer-visible components.

## User stories

98. As a developer, I want a button to spawn a Ticket of a chosen tier and frequency, On Air or with an overfill, so
    that I can test the rack and route on demand.
99. As a developer, I want a toggle to freeze the Window (clock, schedule and gauges), so that I can study a moment
    without the Shift running away.
100. As a developer, I want to see and pin the attempt's seed, so that I can replay a schedule exactly.
101. As a developer, I want each Modifier's dev toggle (Shift default / on / off) and the applied set in the panel, so
     that A/B play is two clicks and a Reload.
102. As a developer, I want buttons to skip the Briefing, end Orientation and end the Shift now, so that I can reach
     any phase quickly.
103. As a developer, I want the Coupler reset, Transform durations and Ticket timers editable live, so that I can tune
     them in play.
104. As a developer, I want Tickets, Jobs, the Shift clock, the score and the coupler visible as reflected components in
     the Flecs Explorer, so that I can inspect them at runtime.

## Implementation decisions

- **Room panel gains a Shift section** (same `DebugUi`-based panel, Room-scoped, main thread): phase and Window left;
  Freeze Window; spawn Ticket (tier, frequency, overfill or On Air); seed (shown, pin with a value or unpin); Modifier
  tri-state toggles plus the applied set; Skip Briefing, End Orientation, End Shift; score, par and chain readouts.
  Each control lands with the ticket that builds the feature it drives.
- **Live tuning** edits the loaded values in memory (Coupler reset, Transform durations, tier times). Saving back to
  disk is not required; Reload re-reads the files.
- **Reflection:** every new component and singleton is pre-registered in the module constructor with members, so the
  Explorer shows it (`CODING_STANDARDS.md`).
- Debug-only actions are panel buttons that write components or raise the same events gameplay uses; they never call
  into systems directly. That keeps them usable from the Flecs Remote API for the runtime checks.
