#include "dialog_runtime.hpp"

#include <algorithm>
#include <vector>

#include <spdlog/spdlog.h>

#include "engine/engine.hpp"

#include "dialog_components.hpp"

namespace engine::dialog {

namespace {

[[nodiscard]] int ChildOrder(const flecs::entity child) {
	if (child.has<DialogChoice>()) return child.get<DialogChoice>().order;
	if (child.has<graph::GraphNode>()) return child.get<graph::GraphNode>().order;
	return 0;
}

/// Children reachable via a graph::GraphEdge, sorted by order (stable).
[[nodiscard]] std::vector<flecs::entity> SortedChildren(const flecs::entity node) {
	std::vector<flecs::entity> children;
	node.each<graph::GraphEdge>([&](const flecs::entity child) { children.push_back(child); });
	std::ranges::stable_sort(children, [](const flecs::entity a, const flecs::entity b) {
		return ChildOrder(a) < ChildOrder(b);
	});
	return children;
}

[[nodiscard]] DialogNodeHandler LookupHandler(const flecs::world& world, const std::string& type) {
	if (!world.has<DialogNodeHandlers>()) return {};
	const auto& map = *world.get<DialogNodeHandlers>().map;
	const auto it = map.find(type);
	return it != map.end() ? it->second : DialogNodeHandler{};
}

void Finish(const flecs::world& world, const flecs::entity runner) {
	world.remove<scene::Paused>();
	world.remove<DialogActive>(); // OnRemove observer tears down the UI box
	if (runner.is_valid()) runner.destruct();
}

/// Execute nodes starting at `node`, following instant transitions until the interaction blocks
/// (Wait), branches to an invalid node, or ends. `root`/`actor` are threaded through rather than
/// read back from the runner: StartDialog may create the runner entity within a system's deferred
/// stage, so its DialogRunner component is not yet committed and must never be read here. State is
/// written with a full set<>() (current node persisted BEFORE each handler runs, so any UI the
/// handler triggers observes the up-to-date node once commands sync).
void StepRunner(
	const flecs::world& world,
	const flecs::entity runner,
	const flecs::entity root,
	const flecs::entity actor,
	flecs::entity node
) {
	while (node.is_valid()) {
		runner.set<DialogRunner>({.root = root, .current = node, .actor = actor, .waiting = false});

		const std::string type = node.has<DialogNode>() ? node.get<DialogNode>().type : std::string{};
		const DialogNodeHandler handler = LookupHandler(world, type);

		NodeResult result;
		if (handler) {
			result = handler({.world = world, .runner = runner, .node = node, .actor = actor});
		}
		else {
			spdlog::warn("[Dialog] No handler for node type '{}' — advancing", type);
		}

		switch (result.flow) {
		case DialogFlow::Wait:
			runner.set<DialogRunner>({.root = root, .current = node, .actor = actor, .waiting = true});
			return;
		case DialogFlow::End: Finish(world, runner); return;
		case DialogFlow::Branch: node = result.next; break;
		case DialogFlow::Advance: node = FirstEligibleChild(world, node); break;
		}
	}
	Finish(world, runner); // ran off the end of the tree
}

} // namespace

void RegisterDialogNodeHandler(const flecs::world& world, std::string type, DialogNodeHandler handler) {
	if (!world.has<DialogNodeHandlers>()) {
		spdlog::error("[Dialog] RegisterDialogNodeHandler before DialogModule import — ignored");
		return;
	}
	(*world.get<DialogNodeHandlers>().map)[std::move(type)] = std::move(handler);
}

void SetDialogFlag(const flecs::world& world, const std::string& flag, const bool value) {
	if (world.has<DialogFlags>()) world.get<DialogFlags>().Set(flag, value);
}

bool GetDialogFlag(const flecs::world& world, const std::string& flag) {
	return world.has<DialogFlags>() && world.get<DialogFlags>().Get(flag);
}

flecs::entity FirstEligibleChild(const flecs::world& world, const flecs::entity node) {
	for (const flecs::entity child : SortedChildren(node)) {
		if (child.has<DialogBranch>()) continue; // branch edges are chosen explicitly by condition nodes
		if (child.has<DialogChoice>()) {
			const auto& choice = child.get<DialogChoice>();
			if (!choice.requires_flag.empty() && !GetDialogFlag(world, choice.requires_flag)) continue;
		}
		return child;
	}
	return {};
}

flecs::entity StartDialog(const flecs::world& world, const flecs::entity root, const flecs::entity actor) {
	if (!root.is_valid()) {
		spdlog::warn("[Dialog] StartDialog called with an invalid root");
		return {};
	}
	const flecs::entity runner = world.entity().set<DialogRunner>({.root = root, .current = root, .actor = actor});
	world.add<scene::Paused>();
	StepRunner(world, runner, root, actor, root);
	return runner;
}

void ResolveDialog(const flecs::world& world, const flecs::entity runner, const flecs::entity chosen) {
	if (!runner.is_valid() || !runner.has<DialogRunner>()) return;
	const DialogRunner state = runner.get<DialogRunner>(); // committed from a prior frame — safe to read
	if (!state.waiting) return;                            // ignore stray resolves

	const flecs::entity next = chosen.is_valid() ? chosen : FirstEligibleChild(world, state.current);
	StepRunner(world, runner, state.root, state.actor, next);
}

void EndDialog(const flecs::world& world, const flecs::entity runner) { Finish(world, runner); }

} // namespace engine::dialog
