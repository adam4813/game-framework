# Campaign save contents

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: open
Blocked by: 09, 10

## Question

What does the campaign save hold and when is it written? Settle: per-Shift best score/stars, currency and owned upgrades, DEFCON state, unlocked Shifts, settings and input bindings (separate file?), save slots (one? three?), and autosave points (after results? after requisition?). Desktop file vs browser storage on WASM is a storage concern the engine Save module leaves to the caller; decide where Signal Room puts it.

Context from [Tickets, window & scoring for Shifts 1–2](05-tickets-window-scoring.md): the save needs best stars (and
score) per Shift, the star bank balance, and owned unlocks. The Collection Schedule seed is per attempt and need not be
saved.
