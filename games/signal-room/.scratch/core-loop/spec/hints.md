# Spec 2 · Hints

Part of [Signal Room spec 2: Core loop](../spec.md). The Hint rule table, which Station a Hint follows, retiring, and
the route-verb and Dash Hints.

## User stories

92. As a new player, I want a Hint floating over the Station that is my next step, showing the key and a short verb
    ("Hold [Space] to copy"), so that I learn the route by playing it.
93. As a player, I want the Hint to follow what I'm doing: the next step for the Item I hold, else picking up or
    working what's waiting for me, else the Radio for the oldest On Air Ticket, so that it always points somewhere
    useful.
94. As a player, I want each kind of Hint to retire for the attempt once I've done that step once, so that Hints get
    out of the way as I learn.
95. As a player, I want a Dash Hint over my Operator the first time I carry something, gone after my first dash, so
    that I learn I can dash with an Item.
96. As a player, I want the key in a Hint to come from my bindings, so that a rebound key shows the right glyph.
97. As a designer, I want Hints as an ordered rule table in data (when → where → glyph + text → retire), so that Shift 2's
    Fault Hints and later Hints need no C++.

## Implementation decisions

- **Hint rules** (new data file), ordered; per rule: `id`, `when`, `anchor`, `action` (the bindings action whose first
  key is shown, e.g. `interact`, `dash`), `text`, `retire_on` and `retire_after` (count, default 1).
  - `when` is a small, fixed vocabulary, combined with AND: `hands` (`empty`, `any_item`, or an Item id),
    `station_kind`, `station_state` (`idle`, `running`, `output_waiting`, `loaded`, `online`), `accepts_held_item`
    (bool), and `radio_for_oldest_on_air` (bool). Anything else is a load error.
  - `anchor`: `station` (the matched Station) or `operator` (the human's own Operator).
  - `retire_on`: an action event by the human's Operator: `place` / `pick_up` / `hold` / `tap` at a Station kind, or
    `dash`.
- **Choosing Hints:** each frame, for the human's own Operator, take the first live (unretired) `station` rule that
  matches some in-service Station; if several Stations match, the nearest one. At most one `station` Hint and one
  `operator` Hint show at once. The rule order encodes "held Item's next step → what's waiting → the Radio".
- **Retiring:** a per-attempt counter per rule id, counted on the matching action events. Steps another (AI) Operator
  does don't count (spec 3 relies on this). Reset at each attempt start.
- **Slice rules:** carrying a card → `place` at the Station that accepts it (Decoder, Telex, or the Keypunch under the
  Modifier); Telex loaded → `tap` to stamp; Decoder running → `hold` to crank; output waiting → `pick_up`; empty-handed
  → Radio for the oldest On Air Ticket → `hold` to copy (and the Keypunch's hold under the Modifier); carrying anything
  → `dash` at the Operator, retired by a dash.
- **Rendering:** a small UI panel (key glyph + text) projected above the anchor through the engine's screen
  projection, built with the engine UI module. Offsets, sizes and colours are HUD data. Hints show only in the
  Orientation and Running phases. The settings toggle is spec 5; Hints are always on here.
