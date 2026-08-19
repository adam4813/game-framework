#pragma once

#include <functional>
#include <memory>

#include <flecs.h>

#include "app/deferred_task_queue.hpp"
#include "platform/platform.hpp"

namespace engine {

// Non-owning handle to the EngineContext, published as a Flecs singleton so world-only code
// (e.g. the game module, which Flecs constructs with just a flecs::world&) can reach the
// platform, deferred task queue, and other context-level services without a global.
struct EngineContextRef {
	class EngineContext* ptr{nullptr};
};

// This centralizes initialization order and dependency management, enabling proper DI.
class EngineContext {
public:
	explicit EngineContext(std::unique_ptr<platform::Platform> platform);
	~EngineContext();

	// Accessors
	flecs::world* GetWorld() { return &world_; }
	[[nodiscard]] const flecs::world* GetWorld() const { return &world_; }

	platform::Platform* GetPlatform() { return platform_.get(); }
	[[nodiscard]] const platform::Platform* GetPlatform() const { return platform_.get(); }

	DeferredTaskQueue& GetDeferredTaskQueue() { return deferredTasks_; }
	[[nodiscard]] const DeferredTaskQueue& GetDeferredTaskQueue() const { return deferredTasks_; }

private:
	std::unique_ptr<platform::Platform> platform_;
	flecs::world world_;
	DeferredTaskQueue deferredTasks_;
};

} // namespace engine
