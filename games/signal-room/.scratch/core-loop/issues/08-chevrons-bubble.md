# 08: Where-to-go chevrons & the Bubble ✗

Spec: [Signal Room spec 2: Core loop](../spec.md). Read: [Machine reads & guidance](../spec/machine-reads.md) (chevrons and the Bubble), [Stations, Transforms & Jobs](../spec/stations-and-transforms.md) (interact rules and the refusal event), [Testing decisions](../spec/testing.md).
Base branch: `feat/signal-room` (branch off it, land per `games/signal-room/AGENTS.md`)
Model: sonnet

**What to build:** Guidance that never retires. While the Operator carries an Item, a down chevron in its identity
colour floats over every in-service Station that would take that Item right now, anywhere in the room (never over
Counters, never while empty-handed). A refused press shows a red ✗ in a Bubble above the Operator's head, offset to
the side, for a short time.

**Blocked by:** 02 (Stations, Transforms & Jobs).

**Status:** done

- [x] One acceptance check ("would this Station take this Item now") shared by the interact rule and the chevrons, so
      the two can't disagree.
- [x] Chevrons per carrying Operator over each accepting Station, in the Operator's identity colour, from a greybox
      catalogue `chevron` entry; height, bob and side-by-side spacing in tuning. Don't assume one Operator.
- [x] A Bubble per Operator; the refusal event shows the `refuse` glyph (a red ✗) for a data duration, restarting on a
      repeat. A glyph table in data (spec 3 adds AI intent glyphs).
- [x] Harness tests:
  - carrying a Raw Card puts a chevron over the Decoder only; once the Decoder is busy, none;
  - carrying a Decoded Card puts one over the Telex;
  - empty-handed, no chevrons; Counters never get one;
  - a refused press shows the ✗; it is gone after its duration;
  - a successful press shows no ✗.
- [x] Runtime: desktop build; screenshot a chevron over the Decoder and a ✗ after a refused press. No asserts.

## Comments
