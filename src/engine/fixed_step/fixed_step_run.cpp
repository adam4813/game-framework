#include "fixed_step_run.hpp"

#include <algorithm>
#include <cmath>

#include <flecs.h>

#include "engine/ecs/ecs.hpp"
#include "engine/scene/scene.hpp"
#include "fixed_step_components.hpp"
#include "fixed_step_pipelines.hpp"

namespace engine::fixed_step {

namespace {

// How far short of a whole Tick, as a fraction of one, still counts as a Tick. A Frame dt worth exactly N Ticks often
// rounds to a hair under N in float; without the slack it would run N - 1 and the next Frame N + 1.
constexpr double kTickSlack = 1e-4;

// The active scene's fixed-step pipeline, or every FixedStep system when no active scene built one.
flecs::entity TickPipeline(const FixedStepPipelines& pipelines) {
	flecs::entity pipeline = pipelines.fallback;
	pipelines.active_scene.each([&pipeline](const scene::SceneComponent& scene_comp) {
		if (scene_comp.instance && scene_comp.instance->GetFixedStepPipeline()) {
			pipeline = scene_comp.instance->GetFixedStepPipeline();
		}
	});
	return pipeline;
}

// Runs the fixed-step pipeline once with the fixed dt and counts the Tick. Ticks within one Frame share Flecs' frame
// count; that only matters if a FixedStep system creates or deletes systems, which none should.
void Tick(const flecs::world& world, const flecs::entity pipeline, const float tick_dt) {
	world.run_pipeline(pipeline, tick_dt);
	++world.get_mut<FixedStepState>().tick;
}

} // namespace

void RunFrame(flecs::world& world, const float dt) {
	const auto& pipelines = world.get<FixedStepPipelines>();
	world.run_pipeline(pipelines.sample, dt);

	if (!world.has<scene::Paused>()) {
		const auto& config = world.get<FixedStepConfig>();
		const float tick_dt = config.TickDeltaTime();
		const flecs::entity pipeline = TickPipeline(pipelines);

		const double due = tick_dt * (1.0 - kTickSlack);
		world.get_mut<FixedStepState>().accumulator += dt;
		int ticks = 0;
		while (world.get<FixedStepState>().accumulator >= due && ticks < config.max_catch_up_ticks) {
			Tick(world, pipeline, tick_dt);
			world.get_mut<FixedStepState>().accumulator -= tick_dt;
			++ticks;
		}

		auto& state = world.get_mut<FixedStepState>();
		if (state.accumulator >= due) {
			const double dropped = std::floor(state.accumulator / tick_dt + kTickSlack);
			ecs::WorldLog(world).debug(
				"[FixedStep] Frame ran its cap of {} Ticks; dropping {} more ({:.3f}s)",
				config.max_catch_up_ticks,
				dropped,
				dropped * tick_dt
			);
			state.accumulator -= dropped * tick_dt;
		}
		// A Tick taken within the slack leaves the accumulator a hair below zero.
		state.accumulator = std::max(state.accumulator, 0.0);
		state.alpha = static_cast<float>(state.accumulator / tick_dt);
	}

	world.progress(dt);
}

void RunTick(flecs::world& world) {
	Tick(world, TickPipeline(world.get<FixedStepPipelines>()), world.get<FixedStepConfig>().TickDeltaTime());
}

} // namespace engine::fixed_step
