#pragma once

#include "creature_catalog.hpp"
#include "world_data.hpp"

// Ambient-creature placement stage (ticket 10): turns a generated world + a creature catalog into a
// sparse set of CreatureSpawn records — livestock scattered around living towns and wildlife spread
// across the open wilderness. Pure + file-free (the catalog is passed in), so it is deterministic
// and unit-testable in isolation. Seeded from the world seed (not the pipeline RNG), so it can be
// appended after every existing generation stage without shifting any golden output.
namespace game::farm {

// True when a creature may stand on terrain of biome `b`. Only Plains (grass) and Beach (sand) are
// walkable natural terrain in the farm tileset, so creatures never spawn on water/forest/hills/
// mountain. Mirrors the tileset walkability used at runtime (farm_tilemap_builder BiomeToTileName +
// assets/data/tilesets/basic.json). A conservative predicate: it never reports a non-walkable tile
// as walkable, guaranteeing the "no creature on a non-walkable tile" invariant.
[[nodiscard]] bool IsCreatureWalkable(Biome b);

// Append ambient creature spawn records to world.creatures. Deterministic in (world.seed, catalog):
// livestock cluster on walkable tiles within creatureTownRadius of each living town, and wildlife is
// scattered one count-roll per creatureAreaSize sampling cell. No creature is placed on a
// non-walkable biome or a tile covered by a blocking world-object, and no two creatures share a
// tile. Call after the town-layout stage so blocking building walls are already known.
void PlaceCreatures(WorldData& world, const CreatureCatalog& catalog);

} // namespace game::farm
