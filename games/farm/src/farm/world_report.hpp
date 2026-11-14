#pragma once

#include <string>

#include "world_data.hpp"

namespace game::farm {

// Format a shareable, human-readable Markdown report of a generated world: the generation
// parameters, notable bodies of water, every city with its status and road connections, and the
// full chronological history (the generation timeline). Pure function — no I/O — so it is trivially
// testable; callers write the returned string wherever they like.
[[nodiscard]] std::string FormatWorldReport(const WorldData& world);

} // namespace game::farm
