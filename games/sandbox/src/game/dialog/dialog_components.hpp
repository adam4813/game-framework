#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <flecs.h>
#include <glm/glm.hpp>

namespace game::dialog {

// ---------------------------------------------------------------------------
// Node identity & flow vocabulary
// ---------------------------------------------------------------------------

/// Every interaction-tree node carries this. `type` is the dispatch key into the
/// DialogNodeHandlers registry (e.g. "line", "choice", "move", "give_item", "wait",
/// "condition", "set_flag", "end"). Each payload loader/builder sets it, so authoring a
/// node is simply attaching its payload component.
struct DialogNode {
	std::string type;
};

/// Tag: the terminal node — finishing the interaction. Carries no data.
struct DialogEnd {};

// ---------------------------------------------------------------------------
// Node payload components (the "what" of each node)
// ---------------------------------------------------------------------------

/// A spoken/narration beat. The interactive dialog box renders `portrait` (an asset
/// path, optional), `speaker` and `text`. Used by both "line" and "choice" nodes; a
/// choice node's `text` is the prompt shown above its options.
struct DialogLine {
	std::string speaker;
	std::string text;
	std::string portrait; ///< optional texture asset path (resolved + loaded on show)
};

/// Edge metadata attached to a CHILD node: marks that edge as a selectable option of
/// its parent choice/line node. Mirrors graph::GraphEdgeWeight but dialog-specific.
struct DialogChoice {
	std::string label;
	std::string requires_flag; ///< optional gate: option hidden unless this flag is set (empty = always)
	int order{0};
};

/// Move a subject entity to a world position over time. The subject is the node's
/// DialogSubject relationship target, defaulting to the interaction's actor.
struct DialogMove {
	glm::vec3 to{0.0F};
	float speed{4.0F};
	bool wait_for_arrival{true}; ///< block the interaction until the move completes
};

/// Make a subject entity continuously follow the interaction's actor. The subject is the node's
/// DialogSubject relationship target. Unlike move, follow does not block: it installs an ongoing
/// DialogFollower behaviour and advances immediately (e.g. an NPC that tags along after you agree).
struct DialogFollow {
	float speed{3.0F};
	float stop_distance{1.0F}; ///< keep at least this far from the leader
};

/// Give or take a named item. Dispatched to the registered "give_item"/"take_item"
/// handler; the engine ships no inventory, so games register the effect.
struct DialogGiveItem {
	std::string item;
	int count{1};
};

/// Give or take coins/currency. Dispatched to the registered "give_coins"/"take_coins"
/// handler.
struct DialogGiveCoins {
	int amount{1};
};

/// Pause the interaction for `seconds` before auto-advancing (cut-scene beat).
struct DialogWait {
	float seconds{1.0F};
};

/// Branch on a story flag: follow the child tagged DialogBranch{true} when the flag is
/// set, else the child tagged DialogBranch{false}.
struct DialogCondition {
	std::string flag;
};

/// Edge metadata attached to a CHILD of a condition node: which branch it represents.
struct DialogBranch {
	bool on{true};
};

/// Set or clear a story flag, then auto-advance.
struct DialogSetFlag {
	std::string flag;
	bool value{true};
};

// ---------------------------------------------------------------------------
// Relationships (first-class links, resolved from JSON "refs")
// ---------------------------------------------------------------------------

/// node --DialogSubject--> entity the node acts on (e.g. the mover for a "move" node).
/// Exclusive: a node has at most one subject.
struct DialogSubject {};

/// line --DialogTarget--> entity the line is addressed to (optional; for framing/aim).
/// Exclusive.
struct DialogTarget {};

// ---------------------------------------------------------------------------
// Runtime state
// ---------------------------------------------------------------------------

/// A live interaction. One entity per running tree; multiple may coexist. `current`
/// is the node being executed or awaited; `waiting` is true while blocked on player
/// input, a timer, or a move.
struct DialogRunner {
	flecs::entity root;
	flecs::entity current;
	flecs::entity actor;
	bool waiting{false};
};

/// Countdown attached to a runner by a "wait" node; the timer system resolves it.
struct DialogTimer {
	float remaining{0.0F};
	flecs::entity runner;
};

/// Positional tween attached to a move subject; the tween system drives it and, when
/// `resolve` is set, resumes the waiting runner on completion.
struct DialogTween {
	glm::vec3 from{0.0F};
	glm::vec3 to{0.0F};
	float elapsed{0.0F};
	float duration{0.0F};
	flecs::entity runner;
	bool resolve{true};
};

/// Ongoing follow behaviour installed by a "follow" node: the carrying entity steers toward
/// `leader` each frame, stopping within `stop_distance`. Persists until removed.
struct DialogFollower {
	flecs::entity leader;
	float speed{3.0F};
	float stop_distance{1.0F};
};

// ---------------------------------------------------------------------------
// Handler dispatch vocabulary
// ---------------------------------------------------------------------------

/// What the runner does after a node handler returns.
enum class DialogFlow : uint8_t {
	Advance, ///< follow the first eligible child edge (or finish if none)
	Branch,  ///< follow NodeResult::next explicitly (or finish if invalid)
	Wait,    ///< block; an external event (choice click, timer, move) will resume
	End,     ///< finish the interaction now
};

struct NodeResult {
	DialogFlow flow{DialogFlow::Advance};
	flecs::entity next; ///< target for DialogFlow::Branch
};

/// Context handed to every node handler.
struct DialogRunContext {
	flecs::world world;
	flecs::entity runner;
	flecs::entity node;  ///< the node being executed
	flecs::entity actor; ///< the interaction initiator
};

/// A node-type strategy: executes one node and tells the runner how to proceed. New
/// node types are added by registering a handler under a new type key — no engine
/// branching. See RegisterDialogNodeHandler.
using DialogNodeHandler = std::function<NodeResult(const DialogRunContext&)>;

// ---------------------------------------------------------------------------
// Singletons
// ---------------------------------------------------------------------------

/// Set while an interaction owns the on-screen dialog box; its OnSet/OnRemove observers
/// build and tear down the UI. `runner` points at the owning DialogRunner entity.
struct DialogActive {
	flecs::entity runner;
};

/// Story flags consulted by condition/set_flag nodes and choice gating. Backed by a
/// heap map so writes are visible immediately, independent of ECS command staging.
struct DialogFlags {
	std::shared_ptr<std::unordered_map<std::string, bool>> values =
		std::make_shared<std::unordered_map<std::string, bool>>();

	[[nodiscard]] bool Get(const std::string& key) const {
		const auto it = values->find(key);
		return it != values->end() && it->second;
	}
	void Set(const std::string& key, const bool value) const { (*values)[key] = value; }
};

/// Registry of node-type handlers (Strategy). Backed by a heap map so game code can
/// register/override handlers at any time without ECS staging concerns.
struct DialogNodeHandlers {
	std::shared_ptr<std::unordered_map<std::string, DialogNodeHandler>> map =
		std::make_shared<std::unordered_map<std::string, DialogNodeHandler>>();
};

} // namespace game::dialog
