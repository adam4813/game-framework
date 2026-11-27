#pragma once

#include <string>

#include "world_data.hpp"

// Serialized generation log (ticket 11). A re-ingestible JSON record of ONE generation run, written
// separately from the shareable map export (map_export.hpp) and the human report (world_report.hpp).
// It captures both the ordered history AND the spatial shaping — town building/field placement,
// world-object scatter (trees, buildings, landmarks, hub signposts, bridge decks), creature spawns,
// road-hub nodes, and bridges (with construction reason + year) — plus the seed and the full
// WorldGenParams, so a developer can inspect any generated world's decisions and a future pass can
// replay it (replay itself is out of scope — only the format is).
//
// Pure C++ — no ECS, no Platform — so it stays deterministic and unit-testable: the same WorldData
// always yields byte-identical JSON. The caller (the farm scene) serializes it at the scene boundary
// beside the other exports, keeping WorldGen::Generate a pure value→value function.
//
// ── JSON schema (also embedded in each log under the "schema" key) ────────────────────────────────
//   schemaVersion : int     — format version (bump on a breaking change).
//   schema        : string  — this human-readable schema description (self-documenting log).
//   seed          : int     — the generation seed the log is keyed by.
//   grid          : { width, height }
//   params        : { … }   — every WorldGenParams knob, for faithful replay.
//   history       : [ { year, type, a, b, description } … ]  — ordered generation history events.
//   towns         : [ { id, name, location:[x,y], size, status, foundingYear, collapseYear,
//                       isOutpost } … ]
//   townLayouts   : [ { townId, ruined, landmarkKind, landmark:[x,y],
//                       buildings:[ { kind, villagerArchetype, footprint:[[x,y]…], door:[x,y],
//                                     ruined } … ],
//                       floorTiles:[[x,y]…], fieldTiles:[[x,y]…] } … ]
//   objects       : [ { kind, footprint:[[x,y]…], height, blocksMovement } … ]  — object scatter.
//   hubs          : [ { position:[x,y], promotedToOutpost } … ]  — road-hub signpost nodes.
//   creatures     : [ { kind, tile:[x,y], wanderRadius, homeTownId } … ]
//   bridges       : [ { townA, townB, span, reason, year, description, deck:[[x,y]…] } … ]
namespace game::farm {

// Build the generation log for `world` as pretty-printed JSON text (trailing newline). Pure — no
// I/O — so it is trivially testable; callers write the returned string wherever they like.
[[nodiscard]] std::string FormatGenerationLog(const WorldData& world);

// Write the generation log for `world` to `path`, overwriting any existing file. Keyed by seed by
// convention: pass a seed-stamped path so re-generating the same seed overwrites its log. Returns
// true on success. The only function here that touches the filesystem.
bool WriteGenerationLog(const WorldData& world, const std::string& path);

} // namespace game::farm
