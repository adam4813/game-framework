#pragma once

namespace signal_room {

// Shift the game opens on (asset-relative).
inline constexpr const char* kBootShift = "shifts/shift_01.shift.json";
// Directory the Room panel lists Shift files from (asset-relative).
inline constexpr const char* kShiftDirectory = "shifts";

// Flecs REST port (desktop). Unique to Signal Room so it never collides with the engine default
// 27750 the sandbox and other games use; tools reach it with FLECS_REST_PORT=27760.
inline constexpr int kRestPort = 27760;

// Greybox data files (asset-relative), re-read on every Room load.
inline constexpr const char* kPaletteFile = "greybox/palette.json";
inline constexpr const char* kGreyboxCatalogueFile = "greybox/catalogue.json";
inline constexpr const char* kItemCatalogueFile = "items.json";
// Feel tuning and keyboard bindings (asset-relative), re-read on every Room load.
inline constexpr const char* kTuningFile = "tuning.json";
inline constexpr const char* kBindingsFile = "bindings.json";

// The floor: a grout slab under inset cell tiles (colours come from the catalogue's "floor").
inline constexpr float kFloorThickness = 0.1F;
inline constexpr float kFloorTileInset = 0.04F;

// Where a carried Item's base sits: this far ahead of the Operator's centre, at about hand height. On
// a Counter it sits on the Counter model's Top().
inline constexpr float kCarryAhead = 0.42F;
inline constexpr float kCarryHeight = 0.72F;
// Each Counter's dust emitter child, which a set-down bursts, and the palette token it puffs in.
inline constexpr const char* kDustEmitterName = "dust";
inline constexpr const char* kDustColourToken = "dust";
// A carried Item with tilt_to_camera turns its top this far (radians, about +x) toward the camera,
// which looks down at about 55 degrees: square on to it.
inline constexpr float kCarryTiltToCamera = 0.61F;

} // namespace signal_room
