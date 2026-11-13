# Greybox readability & juice

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:prototype
Type: prototype
Status: resolved
Blocked by:

## Question

How do Stations, Tarped machines, carried Items, machine states (idle, running, done, Faulted) and the interaction
target highlight read in the 3D greybox from the fixed iso camera, without art? Settle the shape, colour and labelling
language per Station and Item, and which juice belongs in the slice (camera shake, emergency lighting, particles, or
none). This gates spec 1 (Room & bootstrap); see the packets in
[Spec chunking & hand-off order](13-spec-chunking-order.md).

## Answer

**Verdict (2026-10-17).** Four rounds in a three.js prototype of the Shift 1–2 Layout. Variant A (silhouettes &
lamps) won the shape language, with Radios coloured by frequency (from B). Labels and badges (C) and per-kind hue
blocks (B) were not picked. The outline, lift and floor-tile highlights distracted: the target **brightens** instead,
and chevrons became a **where-to-go** hint shown only while carrying.

**Machines: small, on furniture, read from the top and back.** Each Station is a small machine standing on a
table or bench (start ~0.62 m high) that fills its footprint, not a full-height block. Anything that tells state sits on the top deck or on a
**rear riser** (a small panel on a post at the back, tilted toward the camera), never on the front face, so an Operator
working the machine from the south never hides it. Silhouettes, per kind:
- **Radio:** a set on a wall bench, body in its **frequency colour**, riser display showing the frequency label in
  that colour, antenna whip, traffic lamp on a post. Dark/reset Radios dim their body.
- **Decoder:** a cabinet on a sturdy 2×2 table, two tape reels lying on the top deck (spin while running, faster while
  cranked, jitter when Jammed), a side crank that turns while cranked, a lamp on a post at the back.
- **Telex:** a low unit on a stand with a platen roll; a paper sheet stands behind it while loaded and slides up on
  transmit.
- **Keypunch:** a small desk unit with a slanted top.
- **Counter:** a plain wood block with a lighter top slab.
- **Tarped:** a khaki cloth over the footprint with a lumpy top and dark rope bands. It carries no lamp and has no state. The same mesh serves the dressing-prop tarps.

**State language: one lamp per Station, plus a riser progress bar.** Off = idle. Amber blinking, faster as progress
rises = running. Green steady = output waiting. Red strobe = Jam (the bar shows taps toward 3). Slow red pulse = Seize
(the bar shows the Oil Can Hold). Radio lamp: frequency colour slow-blink = On Air, steady = coupled/copying, quick grey
blink = Coupler reset, off = dark. Telex: amber blink = loaded, green flash = transmitted. The bar uses the same colours
(the Radio's bar is in its frequency colour).

**Items: shaped, frequency as a coloured stripe.** Raw Card = cream card, Decoded Card = white card with a dark band,
Morse Tape = spool, Oil Can = red can with a spout. Intercept Items carry a stripe in their frequency colour, matching
the Radio body/display and the Ticket swatch. Carried cards tilt toward the camera. Items sit on Counter tops and
on a Station's deck slot.

**Frequency palette** (data). The colours used for frequencies must stay clear of the Operator identity colours (P1 brick red, P2 mustard,
from [Operators & Swap](02-operator-seats-coop-input.md)) and of the lamp colours (amber, green, red). Starting values:
7.0 cyan, 7.5 magenta, 8.0 violet. The prototype's yellow for 8.0 clashed with mustard.

**Targeting & where to go** (replaces "the target is always highlighted" in
[Room layout, movement & set-down](07-room-layout-movement.md)):
- **Target:** the machine or Counter the Seated Operator would act on **brightens** with a soft pulse in that Operator's
  identity colour. No outline, lift or floor tile.
- **Where-to-go chevrons:** while an Operator carries an Item, a down chevron in its colour floats over **every Station
  that would accept that Item right now** (a Transform input with a free slot, or a Seized Decoder for the Oil Can),
  anywhere in the room. Counters are never marked. Empty-handed Operators get no chevrons. Each Operator's chevrons sit
  side by side when they share a Station.

**Operator Bubble** (new): one speech bubble per Operator, above the head, offset to the side. It shows a red ✗ on a
refused action (replacing any red flash on the machine; the "no" buzz sound stays). For the AI Operator it shows its
intent as a glyph naming the target Station (Radio glyph + frequency in its colour, Decoder, Telex, Counter), replacing
the intent icon from Operators & Swap. It is the base for emotes later. Glyphs are data. Each Operator also wears an
identity-colour collar, so its chevrons can be told apart at a glance.

**Juice in the slice:** **emergency lighting** (while a Fault is active the room lights dim to ~35% and a red beacon
on the north wall pulses) and **particles** (sparks on a Jam and on each Jam tap, steam while Seized, paper confetti on
a stamp, a dust puff when an Item is set down; the engine's particles module covers this). **No camera shake**. The
camera stays dead still, and the shake offset from Operators & Swap is dropped. All intensities are tuning data.

**Camera framing check:** the fixed shot at ~55° pitch looking north fits the 13×9 floor with the north wall, window and
beacon in view. Nothing in the slice Layout needed a cutaway.

**Prototype (primary source):** branch `prototype/greybox-readability` (local, not merged; restacked with `feat/signal-room`), file
`games/signal-room/.scratch/wayfinder/prototypes/greybox-readability.prototype.html`. Variant D is the locked synthesis; A–C are kept
for reference.

Ripples: comments added to [Operators & Swap](02-operator-seats-coop-input.md) and
[Room layout, movement & set-down](07-room-layout-movement.md). Glossary: added **Bubble**, **Where-to-go chevron**
and **Target**.
