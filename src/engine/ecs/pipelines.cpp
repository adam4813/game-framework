#include "pipelines.hpp"

namespace engine::ecs {

namespace {

// The terms Flecs' own default pipeline uses: systems in a phase, sorted by phase dependency, skipping a system
// whose phase or parent is disabled.
flecs::pipeline_builder<> PhasedSystems(const flecs::world& world) {
	world.component<FixedStep>();
	world.component<FrameSample>();
	// clang-format off
	return world.pipeline()
		.with(flecs::System)
		.with(flecs::Phase).cascade(flecs::DependsOn)
		.without(flecs::Disabled).up(flecs::DependsOn)
		.without(flecs::Disabled).up(flecs::ChildOf);
	// clang-format on
}

} // namespace

flecs::pipeline_builder<> FramePipeline(const flecs::world& world) {
	auto builder = PhasedSystems(world);
	builder.without<FixedStep>().without<FrameSample>();
	return builder;
}

flecs::pipeline_builder<> FixedStepPipeline(const flecs::world& world) {
	auto builder = PhasedSystems(world);
	builder.with<FixedStep>();
	return builder;
}

flecs::pipeline_builder<> FrameSamplePipeline(const flecs::world& world) {
	auto builder = PhasedSystems(world);
	builder.with<FrameSample>();
	return builder;
}

} // namespace engine::ecs
