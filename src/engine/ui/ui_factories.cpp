#include "ui_factories.hpp"

#include <tuple>
#include <utility>

#include <flecs.h>

#include "engine/assets/assets.hpp"
#include "engine/audio/audio.hpp"
#include "ui_components.hpp"
#include "ui_events.hpp"

namespace engine::ui {

void SetDefaultClickSound(const flecs::world& world, std::string path) {
	if (!world.has<UIConfig>()) {
		return;
	}
	// Pin one permanent reference to the click sound so it stays cached even as buttons — each
	// carrying their own ref-counted SoundEffect — are created and destroyed during UI rebuilds.
	// Without this pin, destroying the last button drops the asset's ref count to zero and unloads
	// it, forcing a disk reload on the very next button/click.
	if (!path.empty()) {
		(void) assets::LoadSound(world, path); // intentionally never released (app lifetime)
	}
	world.get_mut<UIConfig>().default_click_sound_path = std::move(path);
}

flecs::entity CreatePanel(const flecs::world& world, const core::Rect rect, const Panel panel) {
	return world.entity().set<UIRect>({rect}).set<Panel>(panel);
}

flecs::entity CreateLabel(const flecs::world& world, const core::Rect rect, std::string text, const float font_size) {
	return world.entity().set<UIRect>({rect}).set<Label>({.text = std::move(text), .font_size = font_size});
}

flecs::entity CreateButton(
	const flecs::world& world,
	const core::Rect rect,
	std::string label,
	std::function<void(flecs::entity)> on_click,
	const int texture
) {
	const flecs::entity entity =
		world.entity().set<UIRect>({rect}).set<Button>({.label = std::move(label), .texture = texture});
	if (on_click) {
		entity.set<OnClick>({std::move(on_click)});
	}
	// Auto-attach the default click sound so every factory-created button sounds consistent.
	if (world.has<UIConfig>()) {
		if (const auto& cfg = world.get<UIConfig>(); !cfg.default_click_sound_path.empty()) {
			entity.set<audio::SoundEffect>({.path = cfg.default_click_sound_path});
		}
	}
	return entity;
}

flecs::entity CreateProgressBar(const flecs::world& world, const core::Rect rect, const ProgressBar& bar) {
	return world.entity().set<UIRect>({rect}).set<ProgressBar>(bar);
}

flecs::entity CreateSlider(
	const flecs::world& world,
	const core::Rect rect,
	const float value,
	const float min,
	const float max,
	std::function<void(float)> on_change,
	const float step
) {
	return world.entity().set<UIRect>({rect}).set<Slider>({
		.value = value,
		.min = min,
		.max = max,
		.step = step,
		.on_change = std::move(on_change),
	});
}

flecs::entity CreateSpinner(const flecs::world& world, const core::Rect rect, const Spinner& spinner) {
	return world.entity().set<UIRect>({rect}).set<Spinner>(spinner);
}

flecs::entity CreateStack(const flecs::world& world, const core::Rect rect, const Stack& stack) {
	return world.entity().set<UIRect>({rect}).set<Stack>(stack);
}

flecs::entity CreateScrollRect(const flecs::world& world, const core::Rect rect, const ScrollRect& scroll) {
	return world.entity().set<UIRect>({rect}).set<Panel>({.color = core::colors::PanelBg}).set<ScrollRect>(scroll);
}

flecs::entity CreateModal(const flecs::world& world, const core::Rect rect, const Modal& modal) {
	return world.entity()
		.set<UIRect>({rect})
		.set<UIElement>({.z_index = 1000})
		.set<Modal>(modal)
		.set<Panel>({.color = core::colors::Panel, .roundness = 0.1F});
}

flecs::entity CreateCheckbox(
	const flecs::world& world,
	const core::Rect rect,
	std::string label,
	const bool checked,
	std::function<void(bool)> on_change
) {
	return world.entity().set<UIRect>({rect}).set<Checkbox>({
		.checked = checked,
		.label = std::move(label),
		.on_change = std::move(on_change),
	});
}

flecs::entity CreateRadio(
	const flecs::world& world,
	const core::Rect rect,
	std::string group,
	std::string value,
	std::string label,
	std::function<void(const std::string&)> on_change
) {
	return world.entity().set<UIRect>({rect}).set<Radio>({
		.group = std::move(group),
		.value = std::move(value),
		.label = std::move(label),
		.on_change = std::move(on_change),
	});
}

flecs::entity CreateTextInput(
	const flecs::world& world,
	const core::Rect rect,
	std::string placeholder,
	std::function<void(const std::string&)> on_change
) {
	return world.entity().set<UIRect>({rect}).set<TextInput>({
		.placeholder = std::move(placeholder),
		.on_change = std::move(on_change),
	});
}

flecs::entity CreateTextArea(
	const flecs::world& world,
	const core::Rect rect,
	std::string placeholder,
	std::function<void(const std::string&)> on_change
) {
	return world.entity().set<UIRect>({rect}).set<TextArea>({
		.placeholder = std::move(placeholder),
		.on_change = std::move(on_change),
	});
}

flecs::entity CreateDropdown(
	const flecs::world& world,
	const core::Rect rect,
	std::vector<std::string> options,
	std::function<void(int)> on_change
) {
	return world.entity().set<UIRect>({rect}).set<Dropdown>({
		.options = std::move(options),
		.on_change = std::move(on_change),
	});
}

flecs::entity CreatePrompt(
	const flecs::world& world,
	const core::Rect rect,
	std::string title,
	std::string message,
	std::vector<PromptButton> buttons
) {
	const flecs::entity modal = CreateModal(world, rect);

	// Vertical stack filling the dialog with padding: title, message, then a button
	const flecs::entity column = CreateStack(
									 world,
									 {.x = rect.x, .y = rect.y, .w = rect.w, .h = rect.h},
									 {.direction = StackDirection::Vertical, .spacing = 12.0F, .padding = 18.0F}
	)
									 .child_of(modal);

	std::ignore =
		CreateLabel(world, {.x = 0.0F, .y = 0.0F, .w = 0.0F, .h = 34.0F}, std::move(title), 26.0F).child_of(column);
	std::ignore =
		CreateLabel(world, {.x = 0.0F, .y = 0.0F, .w = 0.0F, .h = 48.0F}, std::move(message), 18.0F).child_of(column);

	// Vertical stack of the action buttons.
	const flecs::entity row = CreateStack(
								  world,
								  {.x = 0.0F, .y = 0.0F, .w = 0.0F, .h = 48.0F},
								  {.direction = StackDirection::Vertical, .spacing = 12.0F, .padding = 0.0F}
	)
								  .child_of(column);

	for (auto& [label, on_click] : buttons) {
		std::ignore =
			CreateButton(world, {.x = 0.0F, .y = 0.0F, .w = 140.0F, .h = 48.0F}, std::move(label), std::move(on_click))
				.child_of(row);
	}

	return modal;
}

} // namespace engine::ui
