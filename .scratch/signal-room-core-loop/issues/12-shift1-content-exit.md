# 12: Shift 1 content pass & spec 2 exit

Spec: [Signal Room spec 2: Core loop](../spec.md). Read: the index, [Testing decisions](../spec/testing.md), and any section a fix touches.
Base branch: `feat/signal-room` (branch off it, land per `games/signal-room/AGENTS.md`)
Model: sonnet

**What to build:** Close spec 2. Make Shift 1's data final (schedule, tiers, pinned Orientation Ticket, Briefing
notes), check every data file loads through the Modifier path, bring the docs and glossary up to date, and run the
spec's exit: one human clears Shift 1 from the Briefing through Orientation to a results screen with at least one
star, on desktop and on web. Any feel or tuning question found while playing is filed, not fixed here.

**Blocked by:** 05, 06, 07, 11.

**Status:** ready-for-agent

- [ ] Shift 1's file is complete: `radios`, `window: 240`, the schedule (25–35 s, NORMAL / a few HIGH, 7.0 only),
      `orientation: true` with its pinned Ticket, `briefing.notes`, no `modifiers`, no placeholder Item.
- [ ] A harness "play-through" test: pinned seed, a scripted route loop (copy → Decoder → Telex, repeated) until the
      hard stop, ending on the results screen with ≥ 1★ and no Lost Intercept. Keep it robust: drive by querying
      state (wait for Done, then pick up), not by fixed frame counts.
- [ ] The same play-through with `keypunch` on and with `telex_autostamp` on also reaches ≥ 1★.
- [ ] A harness test: with `keypunch` forced on, the Briefing route reads "Radio → Keypunch → Decoder → Telex".
- [ ] Docs: a `games/signal-room/README.md` (create it if missing) lists the data files and what each holds;
      `games/signal-room/GLOSSARY.md` gains any new terms used (for example Route, Band); every data file carries a
      `comment` field like `tuning.json`.
- [ ] Every open limitation from tickets 01–11 is filed as a backlog ticket (per `docs/agents/issue-tracker.md`).
- [ ] Builds: `desktop` and `wasm` presets build; `ctest --preset desktop-test-release` passes and the test count is
      above spec 1's 85.
- [ ] Spec exit (runtime): on desktop, play Shift 1 through the Remote API (or by hand) from Briefing to results with
      ≥ 1★; screenshot the HUD mid-Shift and the results screen. In the WASM build in a browser, play at least the
      Orientation Ticket to Running and reach results (End Shift from the panel is fine for the last step).
- [ ] Mark the spec index `Status: done` and add a comment on [Spec chunking & hand-off order](../../signal-room/issues/13-spec-chunking-order.md).

## Comments
