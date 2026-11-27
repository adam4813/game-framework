Status: ready-for-agent

# Spec: Living Farm World — Generation Refinement & 2.5D Representation

## Problem Statement

The farm Game Target's procedurally generated world is structurally complete but feels lifeless
and schematic:

- **Towns are grey blobs.** Each town is a solid square of stone-floor tiles sized by population
  — no buildings, walls, streets, doors, or landmarks.
- **Forests are impassable green regions.** Forest and deep-forest tiles are non-walkable, so the
  player cannot enter them; they have no interior, clearings, or trails.
- **Roads are straight lines.** They are drawn town-centre to town-centre ignoring terrain and
  water, and because the road graph is just a set of `(townA, townB)` pairs with no spatial
  awareness, a near-collinear connection can be routed straight through a third town that sits
  between the endpoints.
- **The world is empty.** A full villager system (schedules, dialog, shops, behaviour trees)
  exists, but it spawns only a handful of villagers at the starting village; every other town and
  the wilderness are uninhabited.
- **The projection is flat.** The world renders as a near-top-down, flat view (closer to Dwarf
  Fortress than the intended cozy sim).

The player wants the generated world to read as a lived-in, Stardew-Valley-style place with depth
and inhabitants, **without** abandoning the existing deterministic, seed-reproducible generation
pipeline that the current prototype depends on.

## Solution

Refine the existing deterministic generation pipeline **in place** and add a thin 2.5D
representation layer, so the same seed still produces the same world, but that world now looks and
feels alive:

- **Towns become villages:** discrete buildings (walls, floors, doors) along short streets around
  a central landmark, a few decorative crop fields, and distinct living vs. ruined appearances.
- **Forests become enterable woodland:** walkable ground scattered with tall tree objects of
  varying density, with clearings and trails passing through.
- **Roads become believable trade routes:** least-cost paths that follow terrain, reuse one
  another into shared trunks and junctions, route around (not through) towns, skip redundant
  connections, converge through sparse natural meeting-places (hubs), and cross water only with
  deliberate bridges.
- **The world gains inhabitants:** villagers populate every living town scaled by population, plus
  a few wandering creatures.
- **The world is shown in an oblique 2.5D view:** an orthographic, tilted camera plus upright
  objects whose visual height is decoupled from their one-tile footprint — a tree stands several
  tiles tall but blocks only one tile of movement.
- **Every run is recorded:** a serialized, re-ingestible generation/history log is emitted for
  debugging and future replay.

## User Stories

### World feel and representation

1. As a player, I want the world shown at a tilted, foreshortened angle so that it reads as a
   cozy, lived-in place rather than a flat map.
2. As a player, I want trees, buildings, and characters to stand upright and be taller than the
   tile they occupy so that the world has a sense of depth.
3. As a player, I want a tall tree to block only the single tile it grows on so that movement
   around the world stays readable and fair.
4. As a developer, I want the oblique look achieved with an orthographic camera so that the scene
   does not suffer the perspective "fanning" distortion a tilted perspective camera would add.
5. As a developer, I want orthographic projection to be a reusable capability of the engine camera
   so that any Game Target — not just the farm — can adopt the look.

### Towns

6. As a player, I want towns made of individual buildings with walls, floors, and doorways so that
   a settlement looks built rather than painted.
7. As a player, I want streets and a central landmark (a well or plaza) so that a town has a
   recognisable centre and layout.
8. As a player, I want bigger-population towns to have more buildings so that town size is
   legible at a glance.
9. As a player, I want a living town to look intact and a ruined town to look broken so that
   history is visible in the architecture.
10. As a player, I want a few tilled fields around farming settlements so that the countryside
    looks worked.
11. As a developer, I want building kinds (house, shop, barn, hall) defined in data so that adding
    a new building type needs no engine rebuild.
12. As a player, I want certain buildings to correspond to the villagers who use them (a shop for
    the shopkeeper, a hall for the elder) so that the town feels purposeful.

### Forests

13. As a player, I want to walk into a forest so that the wilderness is explorable rather than a
    wall.
14. As a player, I want forests to vary in density — thick cores, sparser edges, occasional
    clearings — so that woodland feels natural instead of uniform.
15. As a player, I want trails and roads to pass through forests so that travel through the woods
    is possible.
16. As a player, I want different kinds of trees in different woodland so that forests have visual
    variety (for example darker conifers in deep forest).
17. As a developer, I want tree species defined in data so that adding a species needs no engine
    rebuild.

### Roads

18. As a player, I want roads to curve and follow the land so that they feel like real trade
    routes rather than ruled lines.
19. As a player, I want roads to go around hills, water, and other obstacles so that routes make
    geographic sense.
20. As a player, I want a road that reaches a town to enter it at its edge rather than cut through
    its centre so that towns are not bisected by through-traffic.
21. As a player, I want roads between the same places to share a common route so that the network
    forms trunks and junctions instead of parallel duplicates.
22. As a player, I want the network to avoid building a redundant road when two towns are already
    well connected through a third so that the layout stays sensible.
23. As a player, I want roads to converge naturally at a handful of meeting-places so that the
    world grows organic crossroads.
24. As a player, I want a very well-connected place to grow into a notable hub ("all roads lead to
    Rome") so that importance emerges from the network.
25. As a player, I want roads to cross water only where a bridge has been built so that water is a
    real barrier.
26. As a player, I want some bridges to be deliberate historical investments by prosperous
    communities so that infrastructure reflects the world's history.
27. As a player, I want a bridge only across narrow water, with far-flung places left unconnected
    when a crossing would be implausible, so that the map stays believable.

### Inhabitants

28. As a player, I want villagers living in every surviving town, scaled to its size, so that the
    world feels populated beyond my starting village.
29. As a player, I want villagers to belong to a town's buildings (home and workplace) so that
    their daily routines are grounded in the settlement.
30. As a player, I want a variety of villager archetypes with varied names so that towns feel
    distinct.
31. As a player, I want a few animals and wildlife — livestock near towns, creatures in the wild —
    so that the world is not static.
32. As a developer, I want villager archetypes, name pools, and creature kinds defined in data so
    that expanding the cast needs no engine rebuild.

### Generation integrity and tooling

33. As a developer, I want every refinement to stay deterministic from the seed so that a given
    world is always reproducible.
34. As a developer, I want the new behaviour tunable through generation parameters so that density,
    costs, and population ratios can be adjusted without code changes.
35. As a developer, I want each new stage covered by tests at the generation seam so that
    regressions are caught without running the game.
36. As a developer, I want a serialized log of each generation run — history events plus the
    spatial shaping (building and field placement, trees, animals, hubs, bridges) — written
    separately from the shareable map export, so that I can inspect a world and, later, replay it.
37. As a developer, I want to verify the finished scene at runtime through the Flecs Remote API so
    that crashes and wrong placements that unit tests cannot see are caught before sign-off.

## Implementation Decisions

### Scale and determinism

- The farm world is generated at **1000 × 1000 tiles** (the scene's current override of the
  default parameters). All new placement radii, densities, and costs are expressed as proportions
  or parameters derived from world size, not fixed tile counts, so the same logic holds at the
  unit-test default (160 × 160) and at play scale.
- **Scale reasoning:** at 1000 tiles per edge a town footprint of a few dozen tiles is well under
  1% of the world; building counts, tree densities, and road costs must be tuned against the
  1000-tile edge, and downstream consumers (minimap, map modal, object mesh budget) size against
  it.
- All refinements remain **pure, deterministic functions of `(params, seed)`**. Two generations
  with the same inputs must produce identical output, including the serialized log.

### Generation concentrated behind one seam

- All new generation logic extends the existing pure-C++ `WorldGen::Generate(params, seed) →
  WorldData` seam. `WorldData` gains new value-type fields for the world-object layer (buildings,
  trees, props, bridges, hub markers, field plots), hub nodes, bridges, creature placements, and
  the generation log. No ECS or Platform dependency enters the generation layer.
- Procedural **taxonomies are data-driven registries**, not closed enums or `if/else` chains:
  building kinds, tree species, villager archetypes, and creature kinds live in JSON and are
  resolved by string key. The terrain biome classification stays a closed set (fixed elevation
  bands); it is not a procgen taxonomy a designer would extend.

### World-object layer (footprint decoupled from height)

- A new **world-object** concept represents every placed, upright thing: `{ footprint tile(s),
  object kind (data key), visual height in tiles, blocks-movement }`. Terrain tiles remain the flat
  ground; world-objects sit on top. Trees, building walls/roofs, doors, props, bridge decks, hub
  markers, decorative crops, and creatures are all world-objects.
- **Collision** is driven by the footprint's walkability (reusing the existing tile walkability
  mechanism), decoupled from visual height, so a one-tile-footprint tree can render several tiles
  tall while blocking exactly one tile.

### Orthographic oblique camera (Engine Module, Platform-routed)

- The engine `Camera` component gains an **orthographic** projection mode plus an orthographic
  view-size field. The matrix is built with `glm::ortho`; the raylib backend selects
  `CAMERA_ORTHOGRAPHIC`. Per ADR-0001 all Raylib access stays behind the `Platform` interface;
  per ADR-0005 the farm follow-camera's fixed oblique tilt is authored on its local `Transform`,
  never on `WorldTransform`.
- The farm camera is a **fixed oblique angle** (no free rotation). Because the scene is real 3D
  with a depth buffer, overlapping upright objects are resolved by depth testing — no manual
  depth sort.

### Upright-object renderer

- A new geometry builder, mirroring the existing tilemap mesh builder, bakes static world-objects
  into **upright, alpha-tested cutout quads** sized by each object's height, rebuilt when the
  object set changes. Alpha-test (cutout) avoids transparency sorting. Dynamic inhabitants
  (villagers, creatures) get a small moving upright quad. All rendering remains Platform-routed;
  materials are reached through the existing `RenderWith` relationship.

### Towns

- Town realisation replaces the solid stone-floor fill with a generated **village layout**: a
  central landmark, building footprints whose count scales with population, placed along a short
  street/path network, each building composed of blocking walls enclosing a floor with a door
  opening. Living towns use intact building variants; ruined towns use broken/rubble variants.
- A small **building catalog** (JSON) defines kinds (house, shop, barn, hall) with footprints,
  heights, and sprites; some kinds map to villager archetypes so an archetype can be housed in its
  building. A few **decorative tilled-field** plots ring farming settlements (ambience only, not
  interactive farm plots).

### Forests

- Forest and deep-forest terrain become **walkable** woodland ground. Impassable **tree
  world-objects** are scattered by a secondary density field (dense cores, sparse edges), with
  occasional clearings; deep forest is denser/taller. Tree **species** come from a JSON catalog
  keyed by biome. Roads and trails clear a passage through the trees where they cross.

### Roads: spatial router, hubs, and bridges

- History still decides **which** towns want to be connected (the existing event-driven
  simulation is preserved). **Realisation** becomes a spatial **least-cost tile router** with:
  - biome-weighted movement cost (cheap plains/beach, costlier forest which it clears, blocked
    mountain and deep water);
  - **road reuse** — existing road tiles are cheap, so new routes merge onto shared trunks and
    junctions rather than laying parallels;
  - **town avoidance** — town footprints are blocking/expensive, so roads route around and connect
    at a town's edge, never through its centre;
  - a **redundancy filter** — a wanted connection is skipped when the realised network already
    links those towns within a detour factor (so an A–C road is dropped when A–B–C already
    serves).
- **Hubs / nexus nodes:** sparse nodes placed at geographic funnel points (river fords and
  confluences, mountain passes, isthmuses) that are central to multiple towns, chosen as spaced
  local maxima of a "funnel score". Each hub applies a **soft cost discount** (an attractor, never
  a forced waypoint) so nearby roads tend to converge into organic crossroads, and drops a small
  **marker** world-object where roads actually use it. Towns themselves also act as attractors
  scaled by size/connectivity, so a large well-connected town becomes a natural hub ("all roads
  lead to Rome"). A bounded **post-pass** promotes a high-convergence hub with no nearby town into
  a small young "crossroads outpost" (a real living town that then receives inhabitants). Feeding
  convergence back into the in-simulation founding loop is explicitly deferred.
- **Bridges** are not a separate pipeline layer. They are created (1) as a **routing fallback**
  when the router finds no land path for a wanted connection, bridging the narrowest viable
  crossing, and (2) as a **prosperity-gated history event** — a thriving town/network chooses to
  invest in a crossing that meaningfully shortens a route or reaches an otherwise-isolated place —
  recorded in the generation timeline. A **maximum bridge span** parameter caps crossings to
  narrow water (rivers, straits, pond necks, short island hops); a forced crossing that exceeds
  the cap is left **unbuilt**, and the place stays isolated.

### Inhabitants

- Villagers spawn in **every living town**, count scaled to population, drawn from a **data-driven
  archetype catalog** and a procedural **name pool**, reusing the existing schedule/dialog/
  behaviour-tree/shop systems. Each villager's home and workplace waypoints bind to that town's
  generated buildings.
- A **few wandering creatures** (livestock near towns, wildlife in forest/plains) are placed by
  biome and proximity from a JSON creature catalog, animated by the existing wander behaviour
  tree, rendered as upright sprites, and **non-interactive** for now.

### Serialized generation log

- Each generation writes a **JSON log**, separate from the shareable map export and keyed by seed,
  capturing the ordered history events **and** the spatial shaping: building and field placement,
  tree/object scatter, creature placements, hubs, and bridges — enough detail to reconstruct the
  world. It is structured to be **fed back in** later; actual replay is deferred.

### Tunable parameters

- New generation parameters cover at least: buildings-per-capita and town-layout spacing; tree
  density and clearing rate; road biome costs, reuse discount, town-avoidance cost, and redundancy
  detour threshold; hub count, spacing, and attractor discount; maximum bridge span; villagers-per-
  capita; and decorative field and creature counts.

## Testing Decisions

A good test here asserts **observable generation outcomes and external behaviour**, not internal
steps — e.g. "a living town contains at least one building with a door," not "the layout function
called PlaceDoor". Generation tests run as pure functions with no ECS or Platform; rendering is
split into CPU-testable geometry/matrix output plus a runtime visual check.

Seams and test types (confirmed with the developer):

1. **`WorldGen::Generate` (pure C++ unit tests) — the dominant seam.** All generation detail is
   produced and asserted here: determinism (same seed ⇒ identical `WorldData` *and* identical
   serialized log); town layouts (buildings and doors present, counts scale with population,
   living vs. ruined variants); forest walkability proportion and tree-density variation with
   clearings; road router behaviour (routes avoid deep water and mountains, do not pass through a
   town footprint, reuse existing road tiles, and drop redundant connections); hub placement and
   the crossroads-outpost post-pass; bridge creation only within the span cap with over-cap
   crossings left isolated; creature and field placement counts. Prior art: the existing world-gen
   unit tests.
2. **Geometry builder (data → mesh), mirroring the tilemap mesh builder.** CPU-level asserts on
   the upright-object mesh: a one-tile-footprint object produces an upright quad of the expected
   height; object count maps to quad count. No GPU required.
3. **Camera projection (data → matrix).** Unit-test that orthographic mode yields the expected
   `glm::ortho` view/projection matrix and view size; the raylib backend wiring and the final
   look are verified at **runtime via the Flecs Remote API**, not in a unit test.
4. **Headless scene integration (`LoadHeadless` / `SpawnVillagersHeadless`).** Existing no-file-IO
   seam for integration asserts: the scene loads without error, villagers exist in more than one
   town, and world-objects (buildings, trees) are instantiated as entities. Prior art: the
   existing farm scene integration test.

Procgen acceptance criteria are written as **minimum guarantees** (e.g. "the starting village has
at least one road connection"), not global invariants, so the simulation is free to produce
isolated clusters and emergent structure.

## Out of Scope

- Lit windows / lanterns at night and chimney smoke.
- Interactive ambient farming (the decorative fields are visual only) and forageables / resource
  nodes in forests.
- Feeding road-convergence back into the in-simulation town-founding loop (only the bounded
  post-pass promotion is in scope).
- Actual **replay** of a world from the serialized log (the log format is in scope; replaying it
  is not).
- Free camera rotation or multiple camera angles (the farm camera is a single fixed oblique
  angle).
- Interactive creatures (wildlife is ambient movement only).
- Any change to the shareable map export's format or purpose (the new log is a separate artifact).

## Further Notes

- The orthographic camera is an **Engine Module** capability so other Game Targets can reuse it;
  the fixed oblique tilt is farm-specific scene/level authoring.
- Phasing suggestion for breaking this spec into tickets: (A) orthographic camera + world-object
  layer and renderer; (B) towns; (C) forests; (D) road router, then hubs + crossroads outpost,
  then bridges; (E) villager spread + creatures; (F) serialized log + parameter audit + runtime
  Flecs verification. Each generation ticket owns its scale reasoning and minimum-guarantee
  acceptance criteria.
- Earlier in the design conversation an orthographic camera was chosen over a quick oblique
  *perspective* camera; the perspective option (and a true 2D sprite pipeline) were considered and
  rejected for fidelity/fit reasons.
