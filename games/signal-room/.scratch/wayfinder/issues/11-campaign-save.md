# Campaign save contents

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: resolved
Blocked by: 10

## Question

What does the campaign save hold and when is it written? Settle: per-Shift best score/stars, currency and owned upgrades, unlocked Shifts, settings and input bindings (separate file?), save slots (one? three?), and autosave points (after results? after requisition?). Desktop file vs browser storage on WASM is a storage concern the engine Save module leaves to the caller; decide where Signal Room puts it.

Context from [Tickets, window & scoring for Shifts 1–2](05-tickets-window-scoring.md): the save needs best stars (and
score) per Shift, the star bank balance, and owned unlocks. The Collection Schedule seed is per attempt and need not be
saved.

Context from [Requisitions for a two-Shift slice](10-requisitions.md): save the set of owned Requisition ids (not their
effects; those come from the catalogue data). The Star bank can be derived as stars earned − cost of owned Requisitions,
so decide whether to store it or derive it. Shift unlocks derive from stars earned (Shift 2 opens on Shift 1 ≥1★), never
from the bank.

## Answer

Resolved 2026-10-17 (grilling).

**Storage is already solved.** The Platform save API (`WriteSave`/`ReadSave`/`HasSave`/`DeleteSave`/`ListSaves`)
keeps a JSON blob per named slot: `saves/<name>.json` on desktop, a `localStorage` entry on WASM. The Save module
serializes registered fields and stamps `version`. No engine work needed.

**Two kinds of slot.** Three **Campaign** slots plus one **settings** slot. Reset/Delete of a Campaign never touches
settings.

**Campaign contents.** `version` + named top-level sections:
- `shifts`: keyed by Shift id → best stars, best score.
- `requisitions`: owned Requisition ids.

Stars earned (sum of best stars) and the Star bank (earned − catalogue cost of owned) are **derived, never stored**, so
nothing can desync and a price change re-prices automatically. Shift unlocks derive from stars earned. The Collection
Schedule seed is per attempt and not saved.

**Extensible by section.** A future feature registers its own top-level section. Load is tolerant: a missing section
takes its default, an unknown one is ignored, so adding a section needs no version bump. Bump `version` and migrate only
when an existing key changes meaning. Per-key type mismatches default that key (Save module behaviour).

**Settings contents (slice).** Master/SFX volume, window mode, last-used Campaign slot. Bindings come from the
data-driven action map; no rebinding UI in the slice, so no binding overrides yet (they'd land in this slot later).

**When written.** Autosave on every change: when a Shift ends (its outcome is committed before the results screen
shows, whether or not it set a best) and immediately after every Requisition purchase. No manual save, no mid-Shift
save: quitting mid-Shift forfeits the attempt. Settings write when changed.

**Slot flow.** Title → **Continue** (last-used slot) or **Play** → Slot picker: three fixed cards "Slot 1–3", each
empty or a summary (highest Shift unlocked, stars earned, Star bank). An empty card starts a fresh Campaign. Delete with
confirm; no copy, no naming. Then the Shift picker.

**Failure handling.**
- Stale ids (a Shift or Requisition the data no longer has): skipped on load with a warning, dropped on the next write.
  A removed Requisition effectively refunds.
- A slot that fails to parse: card shows "Unreadable save" and offers only Delete; never silently overwritten.

Glossary: added **Campaign**.
