# 03: Radios & the Antenna Coupler

Spec: [Signal Room spec 2: Core loop](../spec.md). Read: [Radios & the Antenna Coupler](../spec/radios-and-coupler.md), [Stations, Transforms & Jobs](../spec/stations-and-transforms.md) (Holds and the `hold` verb), [Testing decisions](../spec/testing.md).
Base branch: `feat/signal-room` (branch off it, land per `games/signal-room/AGENTS.md`)
Model: sonnet

**What to build:** The route's first step. Each in-service Radio is tuned to the frequency its Shift gives it, shows
that label on its riser display, and wears its frequency colour. A Radio is online only while an On Air Ticket names
its frequency. Holding the action key at an online Radio (empty-handed) for 2.0 s copies a Raw Card striped in that
colour, which waits on the Radio until picked up. Engaging a Radio couples the antenna to it and darkens every other
Radio. When the copy ends or the Hold drops, every Radio stays dark for the 0.4 s Coupler reset, then every Radio with
traffic comes back. After this ticket the full route plays: Radio → Decoder → Telex clears a Ticket.

**Blocked by:** 02 (Stations, Transforms & Jobs).

**Status:** ready-for-agent

- [ ] The Antenna Coupler singleton (`coupled`, `reset_left`) and a derived, reflected online state per Radio.
- [ ] Engage on a Hold start at an online Radio; disconnect on copy complete or Hold end; `coupler_reset` (tuning,
      0.4 s, live in the panel) before Radios come back. A Hold at a dark Radio refuses. A frequency leaving On Air
      mid-Hold does not stop the copy.
- [ ] The Radio's copy uses the generic `hold` verb (∅ → `raw_card`, 2.0 s), keeps progress when paused, and the card
      takes the Radio's frequency. The Radio can't copy again until the card is picked up.
- [ ] Greybox: `body` and the riser display in the frequency colour, dimmed while dark or resetting; the display shows
      the frequency label in that colour. (Lamp blink states are ticket 07.)
- [ ] Events: `CouplerEngaged`, `CouplerReleased`, `RadioOnline`.
- [ ] Harness tests:
  - with no On Air Ticket, a Hold at Radio1 refuses and makes no card;
  - with a 7.0 Ticket On Air, Holding 2.0 s gives a 7.0 Raw Card on Radio1;
  - releasing at 1.0 s and Holding again for 1.0 s finishes the card;
  - (fixture Shift with Radio1 7.0 and Radio2 7.5 in service, both On Air) engaging Radio1 makes Radio2 offline; both
    are offline for 0.4 s after the copy, then both online;
  - a copy finishes even if its Ticket is cleared mid-Hold;
  - the full route (copy → Decoder → Telex) on Shift 1 clears the Ticket.
- [ ] Runtime: desktop build; play the full route on Shift 1 through the Remote API; screenshot the coloured Radio and
      striped card. No asserts.

## Comments
