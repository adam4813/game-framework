#pragma once

#include <flecs.h>
#include <glm/glm.hpp>

#include "engine/core/core.hpp"

namespace signal_room {

struct GreyboxModel;

// Build the greybox look of a loaded Layout under `parent` from the GreyboxCatalogue singleton: the
// floor tiles, the walls, each Station (its machine parts in service, the tarp when Tarped), each
// Counter and each prop. Named catalogue parts become named children of their placement. Call after
// LoadLayout and ApplyInService.
void BuildLayoutGreybox(const flecs::world& world, flecs::entity parent);

// Add a catalogue model's parts as children of `owner`. `fit` scales offsets and sizes per axis (1
// for an unfitted model); parts using the `identity` colour token are drawn in `identity`.
void BuildGreyboxModel(
	const flecs::world& world,
	flecs::entity owner,
	const GreyboxModel& model,
	glm::vec3 fit = glm::vec3{1.0F},
	engine::core::Rgba identity = {}
);

} // namespace signal_room
