#pragma once

#include <algorithm>
#include <expected>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "engine/core/core.hpp"
#include "palette.hpp"

namespace signal_room {

// The engine primitive a greybox part is drawn with.
enum class PartShape { Cube, Sphere, Capsule };

// One primitive of a greybox model. `offset` is from the footprint centre at floor level (+y up,
// -z north/back). `size` is the cube's extents, the sphere's diameters, or the capsule's
// [diameter, end-to-end length, diameter] along its local y. `rotation` is Euler radians. A named part
// (`body`, `lamp`, `riser_bar`, `reel_l`, `crank`, `paper`, ...) becomes a child entity of that name
// so later specs can find and animate it; `visible: false` builds it disabled (the Telex paper,
// shown only while loaded). A part naming an earlier part as its `parent` hangs off it (offset from
// the parent part's position), so it moves with it: the crank's grip turns with the crank.
struct GreyboxPart {
	std::string name;
	std::string parent;
	PartShape shape{PartShape::Cube};
	glm::vec3 offset{0.0F};
	glm::vec3 size{1.0F};
	glm::vec3 rotation{0.0F};
	engine::core::Rgba colour;
	bool visible{true};
	// The part's colour token is `identity`: it takes the Operator's identity colour when built.
	bool identity{false};
};

// The colour token a part uses to take its Operator's identity colour (the Operator model's body and
// collar), resolved when the model is built rather than from the Palette.
inline constexpr const char* kIdentityColourToken = "identity";

// A kind's greybox look. A footprint-fitted model (the tarp, the rug) is authored for a 1×1 cell and
// 1 m tall: its offsets and sizes stretch to the footprint across and to `height` up. `height` is
// also how tall the tarp stands when a Station of this kind is Tarped.
struct GreyboxModel {
	std::vector<GreyboxPart> parts;
	bool fitFootprint{false};
	float height{1.0F};

	// How high the unfitted model's top surface stands: the highest top of its parts that hang off no
	// other part, ignoring rotation. What sits on a Counter sits at its Counter model's Top().
	[[nodiscard]] float Top() const {
		float top = 0.0F;
		for (const auto& part : parts) {
			if (part.parent.empty()) {
				top = std::max(top, part.offset.y + part.size.y * 0.5F);
			}
		}
		return top;
	}
};

// A wall height class (`full`, `low`): a slab `thickness` thick standing against the floor edge.
struct WallStyle {
	float height{1.0F};
	float thickness{0.2F};
	engine::core::Rgba colour;
};

struct FloorStyle {
	engine::core::Rgba grout;
	engine::core::Rgba tile;
	engine::core::Rgba tileAlt;
};

// World singleton loaded from the greybox catalogue file: per kind, the primitive parts it is built
// from, with colours resolved from the Palette. Station and prop kinds are string keys, so a new
// machine, prop or Item shape is a catalogue entry and no C++ change.
struct GreyboxCatalogue {
	engine::core::Registry<GreyboxModel> stations;
	engine::core::Registry<GreyboxModel> props;
	// Item shapes, keyed by the Item catalogue's `shape`. Offsets are from the Item's base: the top of
	// the Counter holding it, or a point in front of the Operator carrying it.
	engine::core::Registry<GreyboxModel> items;
	engine::core::Registry<WallStyle> walls;
	GreyboxModel counter;
	GreyboxModel operatorModel; // "operator" in the file
	FloorStyle floor;
};

// The prop kind drawn over a Tarped Station's footprint (also used for the dressing tarps).
inline constexpr const char* kTarpPropKind = "tarp";

// Parse a greybox catalogue file, resolving every colour token against `palette`. The error names
// the file, the kind and the bad part or token.
[[nodiscard]] std::expected<GreyboxCatalogue, std::string>
LoadGreyboxCatalogueFile(const std::string& path, const Palette& palette);

} // namespace signal_room
