# Dialog Module

Interaction-tree runner built on `engine::graph`: dialogue, cut-scenes and scripted actor interactions.
Each node entity carries a `DialogNode{type}` plus a payload component; a string-keyed registry of
per-type handlers executes nodes and drives the runner state machine. Trees are authorable in C++
(`dialog_builder.hpp`), level JSON and AngelScript.

Optional module: import with `registry.Import(world, {"dialog"})` in the game module (it pulls in `graph`).

| File                                | Purpose                                                         |
|-------------------------------------|-----------------------------------------------------------------|
| `dialog_components.hpp`             | `DialogNode`, payload components, `DialogActive`, handler type. |
| `dialog_runtime.hpp/.cpp`           | Handler registry, story flags, runner (`StartDialog`, advance). |
| `dialog_ui.hpp/.cpp`                | Builds and tears down the dialog box on `DialogActive`.         |
| `dialog_builder.hpp`                | Fluent C++ builder for node trees.                              |
| `dialog_module.hpp/.cpp`            | `DialogModule`: reflection, scripting, JSON loaders, systems.   |
| `dialog.hpp`                        | Umbrella header.                                                |
