#include "fixed_step_module.hpp"

#include <flecs.h>

#include "engine/ecs/ecs.hpp"
#include "engine/scene/scene.hpp"
#include "fixed_step_components.hpp"
#include "fixed_step_pipelines.hpp"

namespace engine::fixed_step {

FixedStepModule::FixedStepModule(const flecs::world& world) {
	world.component<ecs::FixedStep>();
	world.component<ecs::FrameSample>();
	world.component<FixedStepPipelines>();

	world.component<FixedStepConfig>()
		.member("tick_rate", &FixedStepConfig::tick_rate)
		.member("max_catch_up_ticks", &FixedStepConfig::max_catch_up_ticks);
	world.component<FixedStepState>()
		.member("tick", &FixedStepState::tick)
		.member("accumulator", &FixedStepState::accumulator)
		.member("alpha", &FixedStepState::alpha);

	world.set<FixedStepConfig>({});
	world.set<FixedStepState>({});
	world.set<FixedStepPipelines>({
		.sample = ecs::FrameSamplePipeline(world).build(),
		.fallback = ecs::FixedStepPipeline(world).build(),
		.active_scene = world.query_builder<const scene::SceneComponent>().with<scene::Active>().build(),
	});

	// A world with no active scene runs its default pipeline, which must leave out what the fixed step runs on its own.
	world.set_pipeline(ecs::FramePipeline(world).build());
}

} // namespace engine::fixed_step
