# 06: `WorldInteraction` system

**What to build:** The single dispatch point for all player-world interactions. A
`WorldInteractionSystem` (OnUpdate, Pausable) each frame determines the tile the player is
facing (position + facing direction), inspects its ECS components, and computes the available
interaction based on the player's equipped tool and the tile's current state. A small hint UI
element shows the available action label. On confirm input the system dispatches the
interaction as a component mutation or Flecs event.

The system supports three interaction categories:

- **Tool-based** — the equipped item determines whether an interaction is available. The
  required tool (if any) is data-driven per target: `TilledSoil` preparation requires a hoe;
  watering requires a watering can; crop harvesting requires the tool specified in `crops.json`
  per crop type (e.g. wheat → scythe, tomato → bare hand / no-tool). The system reads the
  crop's `harvestTool` field and only offers the harvest action when the right tool is equipped
  (or no tool is required). Harvesting removes `Crop`/`GrowthStage` and adds the typeId to
  `PlayerInventory`.
- **Pickup** — a tile or entity carrying a `Pickupable` component with no tool requirement:
  confirm adds the item to `PlayerInventory` and removes the component.
- **Generic interact** — a tile or entity carrying an `Interactable` component: confirm fires
  an `InteractWith` Flecs event; the handler is the entity's responsibility (e.g. a door
  toggles open/closed, an NPC triggers dialog). The `WorldInteraction` system does not know
  what happens next.

Three starter tools (hoe, watering can, scythe) must be in the starting inventory so all
interactions can be tested from first launch.

**Blocked by:** 04

**Status:** done

- [x] Facing tile is correctly computed for all four movement directions
- [x] Interaction hint is visible when a valid action is available; absent otherwise
- [x] Hoe on bare ground adds `TilledSoil` to the tile entity
- [x] Watering can on a `Crop` sets `watered = true`
- [x] Harvest action is offered only when the equipped tool matches the crop's `harvestTool`
  (or the crop requires no tool)
- [x] Successful harvest removes crop components and adds the typeId to inventory
- [x] `Pickupable` entity is added to inventory and removed from the world on confirm
- [x] `Interactable` entity fires `InteractWith` event; a door entity toggles as a smoke test
- [x] Player starts with hoe, watering can, and scythe in inventory
- [x] Dispatching an interaction consumes the confirm input (no double-fire)
