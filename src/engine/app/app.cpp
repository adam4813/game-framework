#include "app.hpp"

#include "engine/core/core.hpp"
#include "engine/engine_context.hpp"
#include "engine/fixed_step/fixed_step.hpp"
#include "engine/platform/platform.hpp"

namespace engine {

App::App(EngineContext* context) : context_(context) {}

App::~App() = default;

void App::Tick() const {
	auto* world = context_->GetWorld();
	auto* platform = world->get<platform::PlatformRef>().ptr;

	const float dt = platform->DeltaTime();

	platform->BeginFrame(core::colors::Background);
	fixed_step::RunFrame(*world, dt);
	platform->EndFrame();
}

void App::PostTick() const { context_->GetDeferredTaskQueue().ProcessTasks(); }

} // namespace engine
