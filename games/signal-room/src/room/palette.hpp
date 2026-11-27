#pragma once

#include <expected>
#include <string>

#include "engine/core/core.hpp"

namespace signal_room {

// World singleton: every greybox colour as a named token (`p1`, `lamp_amber`, `freq_7_0`, `tarp`,
// ...), loaded from the palette file so colours are tuned in one place.
struct Palette : engine::core::Registry<engine::core::Rgba> {};

// Parse a palette file: {"colours": {"token": [r, g, b] or [r, g, b, a], ...}}. The error names the
// file and the bad token.
[[nodiscard]] std::expected<Palette, std::string> LoadPaletteFile(const std::string& path);

} // namespace signal_room
