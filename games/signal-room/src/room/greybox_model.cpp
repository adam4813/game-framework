#include "greybox_model.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>

#include "engine/engine.hpp"
#include "greybox_catalogue.hpp"
#include "room_components.hpp"

using namespace engine;

namespace signal_room {

// A placement's owner stands at its footprint centre; for fitted models `fit` is the footprint size
// and height.
void BuildGreyboxModel(
	const flecs::world& world,
	const flecs::entity owner,
	const GreyboxModel& model,
	const glm::vec3 fit,
	const core::Rgba identity
) {
	std::unordered_map<std::string, flecs::entity> named;
	for (const auto& part : model.parts) {
		auto e = world.entity().child_of(part.parent.empty() ? owner : named.at(part.parent));
		if (!part.name.empty()) {
			e.set_name(part.name.c_str());
			named.emplace(part.name, e);
		}
		const glm::vec3 size = part.size * fit;
		spatial::Transform transform{.position = part.offset * fit, .rotation = part.rotation};
		switch (part.shape) {
		case PartShape::Cube: e.set<render::CubePrimitive>({.size = size}); break;
		case PartShape::Sphere:
			e.set<render::SpherePrimitive>({.radius = 0.5F});
			transform.scale = size;
			break;
		case PartShape::Capsule:
			e.set<render::CapsulePrimitive>({.radius = size.x * 0.5F, .height = std::max(0.0F, size.y - size.x)});
			break;
		}
		const auto colour = part.identity ? identity : part.colour;
		e.set<spatial::Transform>(transform).set<render::Material>({.color = colour}).set<PartColour>({colour});
		if (!part.visible) {
			e.disable();
		}
	}
}

} // namespace signal_room
