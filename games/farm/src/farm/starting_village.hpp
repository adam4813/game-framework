#pragma once

#include <optional>

#include "world_data.hpp"

namespace game::farm {

// Pick the player's starting village from a generated world. A valid site is a living town that:
//   * is not coastal (no ocean within a short radius),
//   * is not on mountainous terrain,
//   * has at least one road connection, and
//   * has at least one ruined settlement within params.explorationRange tiles.
// Returns std::nullopt only if no town satisfies the criteria (WorldGen is built to guarantee one).
[[nodiscard]] std::optional<TownSite> SelectStartingVillage(const WorldData& world);

} // namespace game::farm
