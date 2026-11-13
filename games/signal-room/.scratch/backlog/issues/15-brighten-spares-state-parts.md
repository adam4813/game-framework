# The Target brighten should spare lamps and riser bars

Status: ready-for-agent
Source: runtime check of [04: Targeting & the Target brighten](../../room-bootstrap/issues/04-targeting-brighten.md), 2026-10-18
Blocked by:

Land it before spec 2 drives lamp colours.

## Problem

`TargetBrighten` blends **every** greybox part of the Target toward the Operator's identity colour. In the running game,
a targeted Decoder's dark `lamp` turns reddish brown under P1's brick red. Once spec 2 lights the lamps, a brightened
idle lamp could read as the red Jam/Seize lamp, and a lamp that animates its own colour will fight the tint.

Two smaller issues:
- Spec 2's lamp and riser animations will write `Material.color`, while the brighten restores from `PartColour`. Either
  they write `PartColour` too, or the brighten has to skip those parts.
- When two Operators share a Target, the last one painted wins, and the order is not defined. Spec 3's co-op needs a
  rule here, such as blending both or using the first Seat.

## To do

- [ ] Let the catalogue mark parts the brighten leaves alone. For example, a per-part `"brighten": false`, defaulting to
      true, set on `lamp`, `riser_bar` and `paper`. Or the brighten skips a fixed set of named state parts, if that
      proves simpler.
- [ ] Harness test: a targeted Station's `lamp` keeps its colour while its body brightens.
