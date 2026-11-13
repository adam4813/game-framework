# 07: Station state reads & stamp confetti

Spec: [Signal Room spec 2: Core loop](../spec.md). Read: [Machine reads & guidance](../spec/machine-reads.md) (state reads and confetti), [Testing decisions](../spec/testing.md).
Base branch: `feat/signal-room` (branch off it, land per `games/signal-room/AGENTS.md`)
Model: sonnet

**What to build:** The machines show what they're doing. Each in-service Station's lamp and riser bar follow its Job
(off idle; amber blinking faster as progress rises; green steady with output waiting). Radios blink in their
frequency colour while On Air, glow steady while coupled, quick-blink grey through the Coupler reset and stay off
when dark. The Decoder's reels spin (faster when cranked) and its crank turns. The Telex's paper stands while loaded
and slides up on transmit with a green flash and a burst of paper confetti. All rates and intensities in data.

**Blocked by:** 03 (Radios & the Antenna Coupler).

**Status:** done

- [x] A derived, reflected read state per Station (from the Job phase, and for Radios the online / coupler state),
      driving spec 1's named greybox parts (`lamp`, `riser_bar`, `reel_l`, `reel_r`, `crank`, `paper`, `body`) per the
      spec's table.
- [x] Blink rates, the progress-to-blink curve, spin speeds, bar size and the Telex flash / slide timings in the
      tuning file; colours as palette tokens (the Radio bar in its frequency colour).
- [x] A confetti burst from the particles module on each Telex transmission, tuned in data like the dust puff.
- [x] The Target brighten still works over the new part colours (brighten and lamp state don't overwrite each other).
- [x] Harness tests (query read state and part components):
  - an idle Decoder's lamp is off; running it is amber; with its output waiting it is green;
  - the riser bar's fill tracks Job progress;
  - a Radio's read state is On Air with a Ticket, coupled while copying, resetting for 0.4 s after, dark otherwise;
  - the Decoder's reels spin faster while cranked than while running alone;
  - a transmission spawns confetti particles under the Telex.
- [x] Runtime: desktop build; screenshots of the Decoder running, the Radio coupled and the Telex loaded. No asserts.

## Comments
