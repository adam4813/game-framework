#pragma once

#include <vector>

#include <flecs.h>

#include "creature_catalog.hpp"
#include "world_data.hpp"

// Runtime spawning + wander behaviour for ambient creatures (ticket 10). Mirrors the villager system
// seams: a BT action handler drives movement, SpawnCreatures instantiates the records produced by
// WorldGen::Generate, and AttachCreatureSprites gives each creature a dynamic upright quad (ticket
// 03). Creatures are non-interactive — no Interactable, no dialog.
namespace game::farm {

// Pre-register creature component types and register the "creature_wander" behaviour-tree action in
// the BTHandlerRegistry. `tilemapEntity` is captured by the handler so a wandering creature only
// steps onto walkable, unblocked tiles. Call once per world after the tilemap entity exists and
// before any creature ticks (the engine "ai" module must already be imported).
void RegisterCreatureSystems(flecs::world& world, flecs::entity tilemapEntity);

// Spawn one entity per creature record as a child of `sceneRoot`: CreatureTag + CreatureKind +
// CreatureWander + GridPosition + a Blackboard and a BehaviorTree running the wander action. No
// Interactable is added (creatures are ambient). File-free, so it runs in headless tests too.
void SpawnCreatures(flecs::world& world, flecs::entity sceneRoot, const std::vector<CreatureSpawn>& records);

// Give every spawned creature a dynamic upright sprite quad (Transform + GridMover + UprightMover +
// SpriteAtlas + DynamicMesh) sized from its catalog kind, so the existing upright-mover sync glides
// it as it wanders. Requires the scene's ObjectSheet + BasicRegistry (GPU path); a no-op headlessly.
void AttachCreatureSprites(flecs::world& world, flecs::entity sceneRoot, const CreatureCatalog& catalog);

} // namespace game::farm
