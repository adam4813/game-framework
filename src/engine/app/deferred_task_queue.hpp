#pragma once

#include <functional>
#include <queue>

namespace engine {

// Simple deferred task queue for operations that should happen after Tick().
class DeferredTaskQueue {
public:
	DeferredTaskQueue() = default;
	~DeferredTaskQueue() = default;

	// Enqueue a task to be executed in ProcessTasks().
	void Enqueue(std::function<void()> task) { tasks_.push(std::move(task)); }

	// Process and execute all queued tasks.
	void ProcessTasks() {
		while (!tasks_.empty()) {
			tasks_.front()();
			tasks_.pop();
		}
	}

private:
	std::queue<std::function<void()>> tasks_;
};

} // namespace engine
