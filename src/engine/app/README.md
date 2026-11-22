# App

The per-frame driver a game's `main` runs, and the queue for work that has to wait until a frame is over. Not a Flecs
module: it registers no components or systems.

## Structure

- **app.hpp / app.cpp** — `App`, built from an `EngineContext*`. `Tick()` runs one frame; `PostTick()` runs the deferred
  tasks.
- **deferred_task_queue.hpp** — `DeferredTaskQueue`, a FIFO of `std::function<void()>`. `EngineContext` owns one.

## Usage

```cpp
engine::App app(context.get());

while (!context->GetPlatform()->ShouldClose()) {
    app.Tick();      // BeginFrame -> fixed_step::RunFrame(world, dt) -> EndFrame
    app.PostTick();  // runs the queued deferred tasks
}
```

The WASM build calls the same two methods from the `emscripten_set_main_loop` callback.

`Tick()` takes `dt` from `Platform::DeltaTime()` and hands the frame to `fixed_step::RunFrame`
(see `engine/fixed_step/README.md`); it does no simulation of its own.

### Deferring work past the frame

Queue a task from a system, an observer or a UI callback when it must not run inside the frame, such as work that
changes the active scene or destroys the entity running the callback:

```cpp
world.get<EngineContextRef>().ptr->GetDeferredTaskQueue().Enqueue([this]() { SetupPathfindingAgent(); });
```

- Tasks run in enqueue order during the next `PostTick()`, after `EndFrame`.
- `ProcessTasks()` drains until the queue is empty, so a task that enqueues another runs it in the same `PostTick()`.
- The queue is not thread-safe. Enqueue from the main thread, not from a multithreaded system.

## Systems registered

None.
