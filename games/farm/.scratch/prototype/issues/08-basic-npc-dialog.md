# 08: Basic NPC and dialog

**What to build:** The first community presence in the starting village — a small cast of named
villagers with data-driven daily schedules and the ability to hold a conversation with the
player. NPC archetypes (Farmer, Shopkeeper, Elder) are defined as JSON level entities with a
`VillagerSchedule` component that drives movement between waypoints across the day via the `ai`
module's behavior tree. The player approaches a villager and uses the `Interactable` interaction
(ticket 06) to open a dialog tree via the existing `dialog` module. At least one villager is a
shopkeeper from whom the player can buy seeds and sell harvested crops via dialog branches that
modify `PlayerInventory`.

**Blocked by:** 04, 06

**Status:** done

- [x] At least three named villager entities spawn in the starting village at scene load
- [x] Each villager moves between two or more waypoints on a daily schedule driven by
  `GameTime::hour`
- [x] Approaching a villager shows the "Talk" interaction hint (via `Interactable`)
- [x] Confirming opens a short dialog tree using the existing `dialog` module
- [x] The Shopkeeper's dialog tree has a "Buy seeds" branch: deducts currency (or a placeholder
  item) and adds seed items to `PlayerInventory`
- [x] The Shopkeeper's dialog tree has a "Sell crops" branch: removes crop items from
  `PlayerInventory` and adds currency (or a placeholder item)
- [x] NPC archetypes and schedules are defined in JSON (no hardcoded positions or scripts)
- [x] Adding a new villager requires only a JSON entry, no C++ changes
