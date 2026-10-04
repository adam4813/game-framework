#include "dialog_ui.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "engine/engine.hpp"

#include "dialog_components.hpp"
#include "dialog_runtime.hpp"

using namespace engine;

namespace game::dialog {

namespace {

constexpr float kBoxWidth = 560.0F;
constexpr float kBoxHeight = 300.0F;
constexpr float kBoxBottomMargin = 48.0F;
constexpr float kPortraitSize = 96.0F;

/// Option children (those carrying DialogChoice) whose flag gate passes, sorted by order.
[[nodiscard]] std::vector<flecs::entity> Options(const flecs::world& world, const flecs::entity node) {
	std::vector<flecs::entity> options;
	node.each<graph::GraphEdge>([&](const flecs::entity child) {
		if (!child.has<DialogChoice>()) return;
		const auto& choice = child.get<DialogChoice>();
		if (!choice.requires_flag.empty() && !GetDialogFlag(world, choice.requires_flag)) return;
		options.push_back(child);
	});
	std::ranges::stable_sort(options, [](const flecs::entity a, const flecs::entity b) {
		return a.get<DialogChoice>().order < b.get<DialogChoice>().order;
	});
	return options;
}

flecs::entity EnsureRoot(const flecs::world& world) {
	auto& ui = world.get_mut<DialogUI>();
	if (ui.root.is_valid()) return ui.root;
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	ui.root = world.entity("DialogUIRoot")
				  .set<ui::UIRect>(
					  {{.x = 0.0F,
						.y = 0.0F,
						.w = static_cast<float>(platform->Width()),
						.h = static_cast<float>(platform->Height())}}
				  );
	return ui.root;
}

} // namespace

void DestroyDialogBox(const flecs::world& world) {
	if (!world.has<DialogUI>()) return;
	auto& ui = world.get_mut<DialogUI>();
	if (ui.box.is_valid()) {
		ui.box.destruct();
		ui.box = {};
	}
}

void BuildDialogBox(const flecs::world& world) {
	DestroyDialogBox(world);
	if (!world.has<DialogActive>()) return;

	const flecs::entity runner = world.get<DialogActive>().runner;
	if (!runner.is_valid() || !runner.has<DialogRunner>()) return;
	const flecs::entity node = runner.get<DialogRunner>().current;
	if (!node.is_valid()) return;

	const auto has_line = node.has<DialogLine>();
	const DialogLine line = has_line ? node.get<DialogLine>() : DialogLine{};

	// Build the option buttons (choices), or a single continue/close fallback.
	std::vector<ui::PromptButton> buttons;
	for (const flecs::entity option : Options(world, node)) {
		buttons.push_back(
			{.label = option.get<DialogChoice>().label,
			 .on_click = [runner, option](const flecs::entity e) { ResolveDialog(e.world(), runner, option); }}
		);
	}
	if (buttons.empty()) {
		const bool has_next = FirstEligibleChild(world, node).is_valid();
		if (has_next && !has_line) {
			// There are connecting nodes, but the current node has no text to show (e.g. give_item) - auto-advance.
			ResolveDialog(world, runner, {});
			return;
		}
		buttons.push_back({.label = has_next ? "Continue" : "Close", .on_click = [runner](const flecs::entity e) {
							   ResolveDialog(e.world(), runner, {});
						   }});
	}

	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const float x = (static_cast<float>(platform->Width()) - kBoxWidth) * 0.5F;
	const float y = static_cast<float>(platform->Height()) - kBoxHeight - kBoxBottomMargin;
	const std::string title = line.speaker.empty() ? "Dialog" : line.speaker;

	const flecs::entity root = EnsureRoot(world);
	const flecs::entity box =
		ui::CreatePrompt(world, {.x = x, .y = y, .w = kBoxWidth, .h = kBoxHeight}, title, line.text, std::move(buttons))
			.child_of(root);

	// Enable word-wrap + extra height on the message label (CreatePrompt layout:
	// modal -> column -> [title_label, message_label, button_row]).
	std::vector<flecs::entity> column_children;
	box.children([&](const flecs::entity child) {
		child.children([&](const flecs::entity grandchild) { column_children.push_back(grandchild); });
	});
	if (column_children.size() >= 2 && column_children[1].has<ui::Label>()) {
		column_children[1].get_mut<ui::UIRect>().rect.h = 120.0F;
		auto& label = column_children[1].get_mut<ui::Label>();
		label.wrap = true;
		label.align = ui::TextAlign::Left;
	}

	// Optional portrait panel to the left of the box.
	if (!line.portrait.empty()) {
		const int texture = assets::LoadTexture(world, assets::ResolveAsset(world, line.portrait));
		if (texture >= 0) {
			std::ignore = ui::CreatePanel(
							  world,
							  {.x = x - kPortraitSize - 16.0F, .y = y, .w = kPortraitSize, .h = kPortraitSize},
							  {.texture = texture}
			)
							  .child_of(box); // tie portrait lifetime to the box
		}
	}

	world.get_mut<DialogUI>().box = box;
}

} // namespace game::dialog
