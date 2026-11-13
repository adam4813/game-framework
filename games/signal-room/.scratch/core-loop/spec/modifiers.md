# Spec 2 · Modifiers

Part of [Signal Room spec 2: Core loop](../spec.md). The Modifier catalogue, the `set` override schema, layering, the
dev toggle, `keypunch` and `telex_autostamp`.

## User stories

55. As a designer, I want a Modifier to be a named set of data overrides in a catalogue, so that a Shift gimmick or an
    A/B test is a data edit.
56. As a designer, I want a Shift to list the Modifiers it switches on, so that a Shift's gimmicks live with the Shift.
57. As a developer, I want to force any Modifier on or off from the Room panel over what the Shift declares, then
    Reload, so that I can A/B a Shift in play.
58. As a designer, I want overrides to target any of the game's data documents (Shift, Station catalogue, Items,
    Ticket rules, tuning) by path, so that one mechanism covers transforms, numbers and Radio setup.
59. As a player in the `keypunch` A/B, I want the Radio to give a Morse Tape that I punch into a Raw Card at the
    Keypunch (Hold 1.5 s), so that the two-machine route can be compared with the one-machine route.
60. As a player in the `telex_autostamp` A/B, I want placing a Decoded Card on the Telex to transmit at once, so that
    the stamp step can be compared with no stamp step.
61. As a designer, I want a bad override (unknown Modifier, unknown document, a path that doesn't resolve) to fail the
    load with a clear error, so that a typo can't silently change nothing.

## Implementation decisions

- **Modifier catalogue** (new data file): id → `name`, `description`, and `overrides`, a list of
  `{ "doc": <document>, "path": <JSON Pointer, RFC 6901>, "set": <JSON value> }`. Documents: `shift` (the Shift being
  loaded), `stations`, `items`, `tickets`, `tuning`. Spec 5 adds a `scale` op next to `set`; nothing else is added
  here.
- **`set` semantics:** if `path` exists, replace its value. Otherwise add it (a new object member, or `-` to append to
  an array). The parent must exist. Implement it through nlohmann's JSON Patch (`replace` or `add`).
- **Patching happens on raw JSON, before any document is parsed** into game structs. Every loader for those documents
  reads through one "load a game document" call that applies the active overrides. Nothing downstream knows Modifiers
  exist.
- **Active set and layering:** read the Shift's `modifiers` list from the **unpatched** Shift file. Apply the dev
  toggle: each Modifier id is *Shift default*, *forced on* or *forced off* (a world singleton, kept across Reloads,
  edited in the Room panel). Apply the active Modifiers' overrides in catalogue order, each in its listed order.
  Spec 5's Requisitions slot in after the Shift's Modifiers. The applied set is shown in the Room panel and reflected.
- **Shipped Modifiers:**
  - `keypunch`: append `Keypunch` to the Shift's `in_service`, and set the `radio` Transform's `outputs` to
    `["morse_tape"]`. The `keypunch` Station kind's Transform (`morse_tape` → `raw_card`, hold 1.5) is always in the
    Station catalogue and matters only once the Keypunch is in service.
  - `telex_autostamp`: set the `telex` Transform's `verb` to `instant`.
  Shift 1 lists neither, so both are off by default.
- **Errors** name the Modifier, the override index and the path.
