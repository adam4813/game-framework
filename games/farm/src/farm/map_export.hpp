#pragma once

#include <string>

#include "world_data.hpp"

namespace game::farm {

// Render a generated world to an RGBA image and write it as a PNG (via stb_image_write). The image
// shows biomes, rivers, roads, town pins (living vs ruined), and town names, so it pairs with the
// text report — a reader can line up the map and the report to "connect the dots". Returns true on
// success. `cellPixels` controls resolution: the image is (grid.width * cellPixels) square.
bool WriteWorldImagePng(const WorldData& world, const std::string& path, int cellPixels = 6);

} // namespace game::farm
