---
status: accepted
---

# Farm: preview-first entry with a shared World as single source of truth

## Context

The farm Game Target launches straight into `FarmWorldScene`, which generates a 1000×1000
world (full building/tree/creature catalogs) and immediately builds its tilemap and entities.
A separate developer `WorldGenPreviewScene` (toggled by F5) generates a *different, smaller*
world (160×160, default catalogs) and renders it as a flat biome map. The two never share
state, so what the preview shows is not the world you play, and there is no way to inspect a
world before committing to it. The farm also persists to a single fixed save slot
(`"farm_world"`), with no title screen or new/continue/load flow.

## Decision

Model the generated farm world as **one `WorldData` singleton** — the single source of truth —
and treat the preview and the in-game farm as two **lenses** on that same state:

- A new **FarmTitleScene** is the farm's first scene, offering **Continue / New / Load**
  (Quit is out of scope until a platform close API exists).
- **New** opens the slot picker first (choose an empty slot, or confirm overwrite of a full
  one), then the **World Preview**. The preview generates the `WorldData` singleton using the
  *canonical gameplay parameters and full catalogs* (via a shared helper), and renders it as a
  one-pixel-per-tile biome map. It is data-only: it never builds the tilemap or spawns
  entities. The player rerolls the seed until satisfied, then presses **Play**.
- **Play** switches to `FarmWorldScene`, which builds its tilemap and entities from the
  *same* `WorldData` singleton — no regeneration, no copy. In-session the handoff is simply the
  shared singleton.
- Each save **slot** (fixed set `farm_slot_1..3`) persists **seed + generation parameters +
  mutations** (player position, game time, tilled soil / crops). On a **cold start**
  (Continue/Load after relaunch) the `WorldData` singleton is reconstructed by regenerating
  deterministically from the saved seed + params and overlaying the saved mutations, keeping
  save files compact. **Continue** resumes the persisted last-played slot; **Load** reuses the
  slot picker (load a saved slot, or Delete to clear it).
- The in-game F5 preview toggle is removed.

## Considered options

- **Copy the generated `WorldData` between scenes via a singleton** — rejected as redundant:
  the preview and gameplay are different renderings of one state, not two states to sync.
- **Regenerate the world from the seed on Play (seed-only handoff)** — rejected for the
  in-session path: regeneration recreates state that is already in memory. Regeneration is
  retained only for cold-start loads, where no in-memory state exists.
- **Serialize the full world grid into each save** — rejected in favour of compact saves
  (seed + params + mutations) reconstructed deterministically on load.

## Consequences

- Generation must be deterministic in seed + params + catalogs (it is), and the canonical
  params + catalog loading must live in **one shared helper** used by both the preview and
  `FarmWorldScene`, eliminating the old size/catalog divergence.
- Persisting params (not just the seed) keeps existing saves faithful even if the canonical
  gameplay params are later retuned.
- `FarmWorldScene::Load` becomes a consumer of the `WorldData` singleton (new game) or a
  reconstructor from a slot (load), with a fallback to self-generation for direct dev launches.
