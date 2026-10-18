# Campaign save contents

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: open
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
