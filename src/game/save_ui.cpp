#include "save_ui.hpp"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "engine/engine.hpp"
#include "meta_save_example.hpp"

using namespace engine;

namespace game {
namespace {

// Smallest "slotN" name not already present, so the New Save button always makes a fresh slot.
std::string NextSlotName(const flecs::world& world) {
	const std::vector<std::string> existing = ListSaveSlots(world);
	const std::unordered_set taken(existing.begin(), existing.end());
	for (int i = 1;; ++i) {
		if (std::string candidate = "slot" + std::to_string(i); !taken.contains(candidate)) {
			return candidate;
		}
	}
}

} // namespace

flecs::entity BuildSaveBrowser(const flecs::world& world, const platform::Rect& rect, const flecs::entity& parent) {
	constexpr float pad = 12.0F;
	constexpr float rowHeight = 32.0F;
	constexpr float rowGap = 6.0F;
	constexpr float deleteWidth = 64.0F;

	const float innerX = rect.x + pad;
	const float innerW = rect.w - 2.0F * pad;
	const float loadWidth = innerW - deleteWidth - rowGap;

	const auto panel = ui::CreatePanel(world, rect, {.color = platform::colors::PanelBg, .roundness = 0.06F});
	panel.child_of(parent);

	const auto heading =
		ui::CreateLabel(world, {.x = rect.x, .y = rect.y + 8.0F, .w = rect.w, .h = 26.0F}, "Saves", 22.0F);
	heading.set<ui::Label>({.text = "Saves", .font_size = 22.0F, .color = platform::colors::Title});
	heading.child_of(panel);

	// Scrollable list of slot rows (built/rebuilt by refresh below).
	const float scrollY = rect.y + 80.0F;
	const float scrollH = rect.h - 80.0F - pad;
	const auto scroll = ui::CreateScrollRect(world, {.x = innerX, .y = scrollY, .w = innerW, .h = scrollH}, {});
	scroll.child_of(panel);
	const auto list = ui::CreateStack(
		world,
		{.x = innerX, .y = scrollY, .w = innerW, .h = 0.0F},
		{.direction = ui::StackDirection::Vertical, .spacing = rowGap, .padding = 0.0F, .stretch = true}
	);
	list.child_of(scroll);

	// A shared, self-referential refresh so button callbacks can trigger a rebuild after they mutate
	// the slot set. Captured by value (shared_ptr) into every callback.
	auto refresh = std::make_shared<std::function<void()>>();
	*refresh = [world, list, loadWidth, refresh]() {
		// Clear existing rows (deferred-safe: destructs are queued and applied at the next merge).
		std::vector<flecs::entity> rows;
		list.children([&rows](const flecs::entity& child) { rows.push_back(child); });
		for (const flecs::entity row : rows) {
			row.destruct();
		}

		for (const std::string& name : ListSaveSlots(world)) {
			const auto row = ui::CreateStack(
				world,
				{.x = 0.0F, .y = 0.0F, .w = 0.0F, .h = rowHeight},
				{.direction = ui::StackDirection::Horizontal, .spacing = rowGap, .padding = 0.0F, .stretch = true}
			);
			row.child_of(list);

			// Load button — labelled and named with the slot name so it's REST-clickable
			// (e.g. `flecs-api.js click slot1`).
			const auto loadBtn = ui::CreateButton(
				world,
				{.x = 0.0F, .y = 0.0F, .w = loadWidth, .h = rowHeight},
				name,
				[name](const flecs::entity& e) { LoadGameFromSlot(e.world(), name); }
			);
			loadBtn.set_name(name.c_str());
			loadBtn.child_of(row);

			// Delete button — removes the slot, then refreshes the list.
			const auto& refreshCopy = refresh;
			ui::CreateButton(
				world,
				{.x = 0.0F, .y = 0.0F, .w = deleteWidth, .h = rowHeight},
				"Delete",
				[name, refreshCopy](const flecs::entity& e) {
					DeleteSaveSlot(e.world(), name);
					(*refreshCopy)();
				}
			).child_of(row);
		}
	};

	// New Save button — writes a fresh auto-named slot, then refreshes the list. Named so it can be
	// triggered programmatically (e.g. `flecs-api.js click NewSaveButton`).
	const auto& refreshForNew = refresh;
	const auto newSaveButton = ui::CreateButton(
		world,
		{.x = innerX, .y = rect.y + 42.0F, .w = innerW, .h = 32.0F},
		"New Save",
		[refreshForNew](const flecs::entity& e) {
			SaveGameToSlot(e.world(), NextSlotName(e.world()));
			(*refreshForNew)();
		}
	);
	newSaveButton.set_name("NewSaveButton");
	newSaveButton.child_of(panel);

	(*refresh)();
	return panel;
}

} // namespace game
