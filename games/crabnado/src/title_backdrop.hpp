#pragma once

#include <glm/glm.hpp>

#include <flecs.h>

// The title screen's backdrop: a tropical island with the hermit crab on it, the crabnado swaying across a bobbing
// ocean behind it, and breeze lines blowing past. It is lit and framed by its own camera.
namespace crabnado {

// An entity that drifts around `origin` and spins: `position = origin + amplitude * (sin, 0, cos)` of its own phase.
// `time` is the phase's accumulated seconds; `rate` is radians per second and `spin` the yaw in radians per second.
struct TitleSway {
	glm::vec3 origin{0.0F};
	glm::vec3 amplitude{0.0F};
	float rate{0.0F};
	float spin{0.0F};
	float time{0.0F};
};

// An ocean tile that rises and falls: `phase` is its offset in the swell and `rest` its resting height.
struct TitleBob {
	float phase{0.0F};
	float rest{0.0F};
};

// A breeze line that blows along +x at `speed` units per second and wraps around the ocean.
struct TitleBreeze {
	float speed{0.0F};
};

// Registers and reflects the title components and the TitleSway, TitleBob and TitleBreezeMove systems.
// They are tagged GameScene so the title's pipeline, which runs the render systems, runs them. Call from the module constructor.
void RegisterTitleBackdrop(const flecs::world& world);

// Builds the backdrop (camera, lights, ocean, island, crab, crabnado, breeze) as children of `parent`.
void BuildTitleBackdrop(const flecs::world& world, flecs::entity parent);

} // namespace crabnado
