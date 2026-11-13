# 13: Target glow on a Station's desk only

Spec: [Signal Room spec 2: Core loop](../spec.md). Read: [Testing decisions](../spec/testing.md). Builds on the Target brighten from the targeting ticket (`target_brighten.cpp`).
Base branch: `feat/signal-room` (branch off it, land per `games/signal-room/AGENTS.md`)
Model: sonnet

**What to build:** When the Operator targets a Radio, only its desk (the wood top and legs) brightens toward the
Operator's colour. The body, display and lamp keep their own colours, so the frequency colours from ticket 03 stay
readable and the lamp is the only thing that lights for traffic. Today the Target brighten blends every part that has a
`PartColour`, so the whole Radio glows.

The glow is data-driven: a part in the greybox catalogue can opt in (for example `"glow": true`, with the Radio's desk
parts opted in), and a model with no opted-in part keeps the current behaviour of glowing all its parts. Other Station
kinds (Decoder table, Telex) can then opt in the same way without code changes.

**Blocked by:** 03 (Radios & the Antenna Coupler).

**Status:** ready-for-agent

- [ ] The greybox catalogue accepts an optional per-part glow flag; load errors name the entry for a non-boolean value.
- [ ] The Target brighten and its restore touch only glow parts when the placement's model has any, and every part
      otherwise.
- [ ] The Radio's catalogue entry opts in the desk top and its four legs.
- [ ] The Radio's body, display and lamp colours are unaffected while targeted (the lamp still follows online state).
- [ ] Harness test: targeting Radio1 brightens its desk part, leaves `body` and `display` at their PartColour, and
      restores the desk when the Target clears. Existing targeting tests still pass.
- [ ] Runtime: desktop build; walk up to the Radio and screenshot the desk-only glow. No asserts in Debug.

## Comments
