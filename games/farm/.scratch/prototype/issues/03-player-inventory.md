# 03: `PlayerInventory` component

**What to build:** A game-agnostic `PlayerInventory` ECS component on the player entity.
It holds a fixed-capacity array of item slots; each slot is an `Item { std::string typeId;
int count; }`. Provide an add-item API (fills first available slot or stacks onto a matching
typeId), a remove-item API, a query for whether a specific typeId is present, and a
get-equipped-item accessor (the currently selected hotbar slot index). The component is
registered with the save module so item state persists. No farm-specific item types live in
the component — crop and tool typeIds are plain strings resolved by callers. No hotbar UI is
in this ticket; that renders in the scene (ticket 04).

**Blocked by:** None (can start immediately)

**Status:** done

- [x] `PlayerInventory` component compiles and can be set on any entity
- [x] Add-item stacks onto an existing slot of the same typeId before opening a new slot
- [x] Add-item returns false (or signals failure) when the inventory is full
- [x] Remove-item reduces count; slot is cleared when count reaches zero
- [x] `equippedSlot` index is readable and settable (clamped to valid range)
- [x] Component is registered with the save module schema
- [x] Unit tests: add stacking, add overflow, remove to zero, equipped slot clamping


## Comments

**Done** — implemented on `feat/farm-prototype` (`src/game/player_inventory.*`, 17 unit tests in
`tests/unit/player_inventory_test.cpp`). Fixed-capacity slots, stacking add, remove-to-zero, equipped-
slot clamping, save-schema registration. Ticket 04 will call `RegisterPlayerInventory(world)` from the
farm module. Commits: 3a17d7d, merged in 600e36a.
