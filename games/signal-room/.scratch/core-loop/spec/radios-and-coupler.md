# Spec 2 · Radios & the Antenna Coupler

Part of [Signal Room spec 2: Core loop](../spec.md). Radio frequencies in the Shift file, the On Air gate, and engage /
disconnect / Coupler reset.

## User stories

20. As a player, I want each Radio to be fixed to one frequency, shown on its own display ("7.0"), so that I match a
    Ticket to a Radio by walking to it.
21. As a player, I want a Radio's body and display in its frequency colour (7.0 cyan, 7.5 magenta, 8.0 violet), so
    that the Ticket swatch, the Radio and the card stripe all match.
22. As a player, I want a Radio's traffic lamp to blink in its frequency colour while an On Air Ticket names its
    frequency, so that I can see which Radio has traffic.
23. As a player, I want a Radio with no On Air Ticket to stay dark and refuse a copy, so that I can't stockpile cards.
24. As a player, I want to Hold the action key at a lit Radio for 2.0 s to copy a Raw Card, so that copying takes
    commitment.
25. As a player, I want letting go mid-copy to pause the copy and keep its progress on the Radio, so that I can come
    back to it.
26. As a player, I want engaging one Radio to darken every other Radio until I'm done, so that the room's one antenna
    is a real choice (it matters in Shift 2).
27. As a player, I want every Radio to stay dark for a short Coupler reset (0.4 s) after the coupler lets go, and then
    every Radio with traffic to come back, so that the relay re-seating reads as a beat.
28. As a player, I want a copy I've started to finish even if its Ticket clears or is lost meanwhile, so that a Hold is
    never yanked away (the card may be stale).
29. As a designer, I want each Radio's frequency set per Shift in the Shift file, so that Shift 2 can light 3 Radios on
    3 frequencies with no C++ change.
30. As a designer, I want frequency labels, their colours and their order in one frequency table, so that the HUD,
    Radios and Items read colours from one place.

## Implementation decisions

- **Frequency table** (new data file): ordered entries keyed by label (`"7.0"`, `"7.5"`, `"8.0"`), each with a palette
  token for its colour. Order runs low → high (spec 6 maps it to pitch). Unknown labels anywhere are load errors.
- **Shift file** gains `radios`: Radio placement name → frequency label. Every in-service Radio needs one; a Radio
  placement missing from `in_service` may still carry one (it's Tarped and ignored). Shift 1: `{"Radio1": "7.0"}`.
- **Online.** A Radio is online when an On Air Ticket names its frequency, the coupler is free or coupled to it, and
  no Coupler reset is running. Online is derived each frame from Ticket and coupler state; it is reflected for the
  Explorer.
- **Antenna Coupler** is a world singleton: `coupled` Radio (or none) and `reset_left` seconds. Rules:
  - **Engage:** an Operator starts a Hold on an online Radio → `coupled` = that Radio. Every other Radio goes dark.
  - **Disconnect:** the copy completes, or the Hold ends (release, move, dash). `coupled` clears and `reset_left` =
    `coupler_reset` (tuning, default 0.4 s, keep < 0.5 s).
  - **A Hold on a dark Radio refuses.** Resuming a paused copy needs the Radio online again.
  - A frequency leaving On Air during a Hold does **not** disconnect: the copy finishes (story 28).
  - With one Radio (Shift 1) this reduces to the traffic lamp plus a reset blink after each copy.
- **Hold-verb progress** for the Radio uses the shared `hold` verb from [Stations](stations-and-transforms.md). The
  Radio's output (`raw_card`) carries its frequency and waits on the Radio until picked up; the Radio can't copy again
  until then.
- **Greybox:** the Radio's `body` and riser display take the frequency colour. Dark or resetting Radios dim the body.
  The display shows the label as text in the frequency colour (a world-space label from the greybox catalogue's
  `display` part; projected UI text is fine). Lamp states are in [Machine reads](machine-reads.md).
- **Events** for the audio spec: `CouplerEngaged(radio)`, `CouplerReleased(radio)`, `RadioOnline(radio)`.
