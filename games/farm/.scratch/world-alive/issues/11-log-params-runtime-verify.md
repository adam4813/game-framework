# 11: Serialized generation log + parameter audit + runtime verification

**What to build:** This capstone ticket does three things: records every detail of a generation
run in a re-ingestible JSON log, audits all new `WorldGenParams` knobs, and runs a runtime
verification pass to confirm the full scene works end-to-end.

**Generation log:** On each call to `WorldGen::Generate`, a JSON log is written keyed by seed,
separate from the shareable map export. It captures the ordered history events *and* the spatial
shaping: building and field placement, tree and object scatter, creature placements, hub nodes,
bridges. The format is structured to be fed back in later (full replay is deferred). This log is
the canonical debug artifact: a developer can inspect any generated world's decisions.

**Parameter audit:** All new `WorldGenParams` knobs introduced across tickets 04–10 are
documented, sanity-checked for their effect at 1000 × 1000, and wired into the existing
WorldGen preview scene so they are tunable without recompiling.

**Runtime verification:** The farm scene is launched and checked against the Flecs Remote API
to confirm it loads cleanly, upright objects appear at the oblique angle, villagers exist in
multiple towns, and no crashes or assertion failures occur. This is the gate that closes the
whole effort.

**Blocked by:** 04, 05, 06, 07, 08, 10 (all detail stages must be complete so the log has
everything to record and the runtime check is meaningful)

**Status:** done

- [ ] `WorldGen::Generate` writes a JSON log alongside (not replacing) the existing shareable
  map export; the log is keyed by seed and overwritten on each generation with that seed.
- [ ] The log contains: all generation history events in order; building/field placements per
  town; tree/object scatter records; creature placement records; hub node positions and whether
  each was promoted to an outpost; bridge records with construction reason (fallback or history
  event).
- [ ] The log format is documented (in the log file itself or a companion schema comment) so a
  future replay pass can parse it without reverse-engineering.
- [ ] All `WorldGenParams` knobs introduced by tickets 04–10 are present in the default params
  struct with sensible defaults, range comments, and are exercised in at least one unit test that
  varies them.
- [ ] All new knobs are exposed in the WorldGen preview scene so a developer can tune them
  interactively.
- [ ] Runtime check via the Flecs Remote API: the farm scene loads without crash or assertion
  failure; the scene can be switched to and walked around; upright world-objects (trees,
  building walls) are visible from the oblique camera angle; villager entities exist under at
  least two distinct town-location parent entities; no world-gen log write errors appear in the
  console.

## Comments
Integration branch: integration/farm-world-alive (tip ffa3a71). Key commit: b342248 (gen log + param audit + runtime verification + play-scale fixes). Files: generation_log.{hpp,cpp} (new), generation_log_test.cpp + playscale_generation_test.cpp (new), worldgen_preview_scene.{hpp,cpp} (all knobs exposed), farm_world_scene.cpp (WriteGenerationLog), tree_species_catalog.{hpp,cpp} (by-value return), upright_object_mesh_builder.cpp (chunk by vert count), farm_world_player.level.json (camera -Z fix). 295/295 tests pass. Runtime Flecs API verified: FarmWorld scene loads, orthographic oblique camera, upright objects visible, 8 TownRoot towns, villagers present, generation log written without error. Post-review fixes: LoadObjectKindRegistry moved before SetupTilemap (bridge UV bug); RegisterCreatureSystems moved before SpawnCreatures (pre-registration invariant). Final integration tip: ffa3a71.