# Signal Room spec 2: Core loop (Shift 1 solo)

Status: ready-for-agent
Map: [Signal Room: Vertical Slice](../signal-room/map.md) (Spec 2 packet on
[Spec chunking & hand-off order](../signal-room/issues/13-spec-chunking-order.md))
Sources: [Routine Cable route & station verbs](../signal-room/issues/01-routine-cable-route.md),
[Tickets, window & scoring](../signal-room/issues/05-tickets-window-scoring.md),
[Room layout, movement & set-down](../signal-room/issues/07-room-layout-movement.md),
[Radio presets & spectrum read](../signal-room/issues/12-radio-tuning-prototype.md),
[Shift 1 tutorial](../signal-room/issues/14-shift1-tutorial.md),
[Greybox readability & juice](../signal-room/issues/15-greybox-readability.md)
Builds on: [Signal Room spec 1: Room & bootstrap](../signal-room-room-bootstrap/spec.md) (done)
Base branch: `feat/signal-room` (branch off it, land per `games/signal-room/AGENTS.md`; it never merges into `main`)

## Problem Statement

Spec 1 gave Signal Room a room to walk around in, but there is no game in it yet. The machines are silent props, the
only Item is a placeholder, and nothing asks the player to do anything. The design's whole core loop is still on paper:
HQ's Tickets arriving on the rack, copying a frequency at its Radio, decoding the card, stamping it out at the Telex,
racing the gauge, building the Handler Chain and earning stars. Until it runs, nobody can tell whether Shift 1 is fun,
whether the route reads without a tutorial script, whether the timers are fair for one person, or whether the A/B
questions (a separate Keypunch, an auto-stamping Telex) matter.

## Solution

Shift 1, playable solo from start to stars. The Shift opens on a Briefing card that shows the route. Dismissing it
starts **Orientation**: one NORMAL Ticket is On Air at once, and the Window clock waits. The player Holds to copy at the
lit Radio and gets a Raw Card striped in the Radio's frequency colour. They feed it to the Decoder (walk away, or crank
it faster) and pick up the Decoded Card. Then they place it on the Telex and stamp it. The Ticket clears, the Window
starts its 4:00 run, and the Collection Schedule brings more Tickets. Each Ticket waits on Stand-by on its overfilled
gauge, then goes On Air with its gauge draining green → amber → red. The HUD shows the rack, the Window clock, the
score and the Handler Chain. Machines blink, spin and crank. Where-to-go chevrons mark where a carried Item can go, a
red ✗ shows a refused press, and Hints float over the next step until the player has done it once. At 0:00 the Shift
stops hard and the results screen shows score, par and stars, with a Retry. Two Modifiers (`keypunch`,
`telex_autostamp`) can be forced on or off from the Room panel for A/B play. All rules, numbers and text are data.

## Sections

Read this index, then only the sections your ticket names. Each section holds its user stories (numbered across the
whole spec) next to its implementation decisions.

| Section | Covers |
|---|---|
| [Stations, Transforms & Jobs](spec/stations-and-transforms.md) | The Station catalogue, route Items and their frequency, the four verbs, Jobs, the interact rules and refusals. |
| [Radios & the Antenna Coupler](spec/radios-and-coupler.md) | Radio frequencies in the Shift file, the On Air gate, engage / disconnect / Coupler reset. |
| [Tickets, Window & Collection Schedule](spec/tickets-and-schedule.md) | Ticket rules data, tiers, the gauge, Stand-by → On Air → Cleared / Lost Intercept, the Window, Last Call, the hard stop, the schedule and the seed. |
| [Scoring & stars](spec/scoring-and-stars.md) | Value, band share, the Handler Chain, Lost Intercept, par and stars. |
| [Modifiers](spec/modifiers.md) | The Modifier catalogue, the `set` override schema, layering, the dev toggle, `keypunch` and `telex_autostamp`. |
| [Shift flow, Briefing & Orientation](spec/shift-flow.md) | The Shift phases, the Briefing card, Orientation's freeze and pinned Ticket, the results screen and Retry. |
| [In-play HUD](spec/hud.md) | The Ticket rack, the Window clock, score and Handler Chain on screen. |
| [Machine reads & guidance](spec/machine-reads.md) | Lamp and riser-bar states, part animation, stamp confetti, where-to-go chevrons and the Bubble ✗. |
| [Hints](spec/hints.md) | The Hint rule table, which Station a Hint follows, retiring, and the route-verb and Dash Hints. |
| [Debugging & tuning](spec/debugging.md) | The Room panel's Shift section, Explorer-visible components. |
| [Testing decisions](spec/testing.md) | The one seam (the Room harness, extended), what good tests check, and the runtime exit check. |

**Data files.** All JSON under the game's assets, loaded at Room load and re-read on Reload, every one passed through
the active Modifiers first. New this spec: the Station catalogue, Ticket rules, frequency table, Modifier catalogue,
Hint rules and HUD layout. Extended: the Shift file, the Item catalogue, the greybox catalogue, the palette and the
tuning file. Later specs extend these documents and never redefine them.

## Tickets

All `ready-for-agent`, all sized for Sonnet. After 03, tickets 04, 05, 07 and 08 can run in parallel.

| # | Ticket | Blocked by |
|---|---|---|
| 01 | [Tickets, the Window & the Collection Schedule](issues/01-tickets-window-schedule.md) | — |
| 02 | [Stations, Transforms & Jobs (Decoder and Telex)](issues/02-stations-transforms-jobs.md) | 01 |
| 03 | [Radios & the Antenna Coupler](issues/03-radios-antenna-coupler.md) | 02 |
| 04 | [Scoring, the Handler Chain & stars](issues/04-scoring-chain-stars.md) | 02 |
| 05 | [Modifiers, the Keypunch and the auto-stamp Telex](issues/05-modifiers-keypunch.md) | 03 |
| 06 | [In-play HUD (rack, clock, score)](issues/06-in-play-hud.md) | 04 |
| 07 | [Station state reads & stamp confetti](issues/07-station-state-reads.md) | 03 |
| 08 | [Where-to-go chevrons & the Bubble ✗](issues/08-chevrons-bubble.md) | 02 |
| 09 | [Shift flow: the Briefing card, results and Retry](issues/09-briefing-results-flow.md) | 04 |
| 10 | [Orientation](issues/10-orientation.md) | 09 |
| 11 | [Hints](issues/11-hints.md) | 03, 08, 10 |
| 12 | [Shift 1 content pass & spec 2 exit](issues/12-shift1-content-exit.md) | 05, 06, 07, 11 |

## Scale

Shift 1 is one 13×9 Layout, one Radio (7.0), one human and a 240 s Window (after Orientation). Arrivals every 25–35 s
give about 8 scheduled Tickets plus the pinned one. One solo route run is about 12–16 s (2.0 s copy, ~5 m to the
Decoder, 6 s walk-away or ~2.5 s cranked, ~3 m to the Telex, one stamp), so a single human keeps up with a NORMAL
Ticket's 90 s gauge with room to spare. That is intended: Shift 1 teaches; Shift 2 (spec 4) brings pressure with 3
Radios and 15–25 s arrivals. Par for a typical Shift 1 attempt is about 9 × ~22 ≈ 200. Every number lives in data.

## Out of Scope

- Swap, a second Operator, the AI Operator and its clock-on at the end of Orientation, the Swap Hint, the Seat ring
  and the Bubble's AI intent glyphs (spec 3). Shift 1's Roster stays one entry.
- Faults (Jam, Seize), the Oil Can, emergency lighting, Fault particles and Fault Hints; Shift 2's file and content
  (spec 4). The Station catalogue, Ticket rules and Shift schedule must already cope with 3 Radios and FLASH Tickets,
  but no Shift 2 file ships here.
- Requisitions, the `scale` override op, the Star bank, save slots, the Hints settings toggle, the Shift picker and its
  Briefing button, and the slot-derived rules (Briefing auto-show, Orientation's 1★ check) (spec 5). Here the Briefing
  always shows, Orientation always applies, and Hints are always on.
- All audio, including the refusal buzz, the Lost Intercept siren and the red-band ticking buzzer (spec 6). The game
  stays silent; every place a sound will go is a named event the audio spec can hook.
- VIP Tickets, Decoys, consumables and boosts, telemetry, camera shake.
- The open spec 1 feel questions ([backlog 11](../backlog/issues/11-operator-feel-decisions.md),
  [backlog 14](../backlog/issues/14-targeting-feel-decisions.md)) and the [ticket 05 follow-ups](../backlog/issues/17-items-follow-ups.md).
  They stay on the backlog.

## Further Notes

- Decided while writing this spec (not settled by the source issues):
  - **Working a verb needs empty hands** (confirmed with the user). A press while carrying always means "place this Item
    here", or a ✗ if refused. Copy, crank, stamp and pick-up all need empty hands.
  - **A Telex stamp clears the most urgent matching Ticket**: the On Air Ticket with that frequency that has the least
    On Air time left (oldest on a tie). Stand-by Tickets are never cleared.
  - **A copy already under way finishes** even if its Ticket clears or is lost during the Hold; the card may be stale.
  - **The Briefing route is derived** from the in-service Stations' Transforms, so the `keypunch` Modifier shows
    Radio → Keypunch → Decoder → Telex without a second card.
  - **The Briefing and the results screen pause the Room** through the engine's pause-as-data (ADR 0004).
  - **Last Call** stops arrivals and holds Stand-by gauges at the On Air line, so nothing new goes On Air in the
    final 20 s.
- Ticket 04's authoring note applies: any ticket naming a Layout position must check it against
  `bunker.layout.json` (for example, CounterTelex stands directly south of the Telex).
- If a useful term comes up (for example **Route** for the derived Transform chain, or **Band** for a gauge band),
  add it to `games/signal-room/GLOSSARY.md`.
- Exit criterion (from the packet): one human clears Shift 1 to a stars screen, on desktop and on web.
