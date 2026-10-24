# Arena and boss

Back to the [spec](../spec.md).

## User stories

1. As a player, I want a shore of rocks and tide pools, so that where I stand matters.
2. As a player, I want enemies tumbled into a rock to take heavy damage, so that I can line up wave slams on purpose.
3. As a player, I want rocks to block movement for me and the horde, so that the layout shapes the fight.
4. As a player, I want calm tide pools that the horde also gathers in, so that safety has a cost.
5. As a player, I want the arena to have a clear edge, so that I can't wander off.
6. As a designer, I want the arena layout in level JSON, so that I can move rocks and pools without a rebuild.
7. As a player, I want a clear warning when the boss arrives at 5:00, so that the finale feels like an event.
8. As a player, I want the crabnado to come ashore and chase me during the boss, so that I have to lead it somewhere.
9. As a player, I want big crests during the boss to be clearly telegraphed, so that I can plan where to lure it.
10. As a player, I want a big crest that pushes the crabnado into a rock to count as a slam, so that the waves are how I
    win.
11. As a player, I want to see how many slams are left (3 to win), so that I know how close I am.
12. As a player, I want a victory screen when the crabnado breaks apart, so that the run has a clear end.
13. As a player, I want the horde to keep coming during the boss but more slowly, so that the boss stays readable.

## Implementation decisions

- **The arena is a level JSON file** loaded by the arena scene. Rocks and pools are entities with a position and a
  radius. Pools are placed by hand; rocks are scattered each run from the run's seed by the level's `rock_scatter`
  (a denser inner band and a sparser band near the edge, clear of pools, the start points and each other). The
  arena edge is a circle of about 70 units (the prototype's size), marked by a glowing magic barrier: a hovering ward
  line with rune pillars. Open ocean surrounds it to the horizon, so swells and rogue waves roll in from outside.
- **Game-registered level loaders** add the rock and pool components (the sandbox's tilemap scene shows the
  `RegisterLoaders` pattern).
- **Rock slam:** a tumbling enemy whose knockback carries it into a rock takes damage scaled by impact speed, enough to
  kill a crab or shrimp outright. Non-tumbling enemies are just stopped by rocks.
- **Pools** are read by the wave field (see [Wind and waves](wind-and-waves.md)), not by each consumer.
- **The boss** is the same crabnado entity switching to its boss behaviour at 5:00: it moves toward the player, its
  spawn rate drops, and big crests come on a fixed telegraphed rhythm. A big crest that pushes it into a rock counts
  as one slam, with a short cooldown so one crest can't count twice. Three slams win.
- **Fallback (cut list):** if the boss isn't working by the second half of the jam, replace it with 60 s of maximum
  storm followed by victory.
