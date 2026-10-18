# 05: Modifiers, the Keypunch and the auto-stamp Telex

Spec: [Signal Room spec 2: Core loop](../spec.md). Read: [Modifiers](../spec/modifiers.md), [Stations, Transforms & Jobs](../spec/stations-and-transforms.md) (the catalogue only), [Debugging & tuning](../spec/debugging.md), [Testing decisions](../spec/testing.md).
Base branch: `feat/signal-room` (branch off it, land per `games/signal-room/AGENTS.md`)
Model: sonnet

**What to build:** A Modifier catalogue of named data overrides, applied to the game's raw JSON documents before they
are parsed. A Shift lists the Modifiers it wants, and a Room panel dev toggle forces each one on or off before a
Reload. Two ship: `keypunch` (the Radio gives a Morse Tape, and the Keypunch comes out from under its tarp to punch it
into a Raw Card with a 1.5 s Hold) and `telex_autostamp` (placing a Decoded Card on the Telex transmits at once).

**Blocked by:** 03 (Radios & the Antenna Coupler).

**Status:** ready-for-agent

- [ ] A Modifier catalogue data file (id → `name`, `description`, `overrides` of `{doc, path, set}`), and the Shift
      file's `modifiers` list.
- [ ] One "load a game document" path applies the active overrides to the raw JSON of `shift`, `stations`, `items`,
      `tickets` and `tuning` before parsing. `set` replaces an existing path, else adds (`-` appends), through
      nlohmann's JSON Patch. Every loader for those documents goes through it.
- [ ] Active set: the unpatched Shift's `modifiers`, then the dev toggle (Shift default / on / off per id, a world
      singleton kept across Reloads), applied in catalogue order. The applied set is reflected and shown in the panel.
- [ ] `keypunch` and `telex_autostamp` as the spec defines them; the `keypunch` Station kind's Transform is in the
      Station catalogue already (ticket 02), so the Modifier only puts the Keypunch in service and changes the Radio's
      output. Shift 1 lists neither.
- [ ] Errors name the Modifier, the override index and the path: unknown Modifier id (in a Shift or the toggle),
      unknown `doc`, a path whose parent doesn't exist.
- [ ] Harness gains forced Modifiers on Shift load.
- [ ] Harness tests:
  - with `keypunch` forced on, the Keypunch is in service (not Tarped), the Radio's copy gives a Morse Tape, and a
    1.5 s Hold at the Keypunch turns it into a Raw Card with the same frequency;
  - with it off, the Keypunch is Tarped and the Radio gives a Raw Card;
  - with `telex_autostamp` on, placing a Decoded Card on the Telex clears the Ticket without a press;
  - a fixture Shift listing a Modifier and the toggle forcing it off leaves it off;
  - a fixture Modifier with a path whose parent doesn't exist fails the load naming the Modifier.
- [ ] Runtime: desktop build; toggle `keypunch` in the panel, Reload, and play the four-Station route through the
      Remote API. No asserts.

## Comments
