#pragma once

#include <flecs.h>

namespace engine::dialog {

/// Dialog / interaction-tree module (game level).
///
/// Turns an entity graph (engine::graph GraphNode + GraphEdge) into a runnable interaction
/// tree — dialogue, but also cut-scenes and scripted actor interactions. Each node carries a
/// DialogNode{type} plus a payload component; a registry of per-type handlers (Strategy)
/// executes nodes and drives the runner state machine. Trees are authorable in C++, JSON, and
/// AngelScript.
///
/// Registers:
///   - Reflection + scripting for every dialog component
///   - Default handlers for line/choice/move/give+take item/coins/wait/condition/set_flag/end
///   - DialogTimerSystem + DialogTweenSystem (drive wait/move nodes; non-pausable)
///   - DialogActive OnSet/OnRemove observers (build/tear down the dialog box)
///   - JSON loaders (dialog_line, dialog_choice, dialog_move, ...) + refs (subject, target)
///   - Script globals (StartDialog, SetDialogFlag, node builders, SetDialogNodeHandler)
///
/// Depends on engine modules: graph, level, scripting, ui, assets, spatial, scene — all
/// imported by EngineContext before the game module.
class DialogModule {
public:
	explicit DialogModule(const flecs::world& world);
};

} // namespace engine::dialog
