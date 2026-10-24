#include "settings_menu.hpp"

#include <string>

#include "engine/engine.hpp"
#include "settings.hpp"
#include "ui_theme.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr float kModalWidth = 570.0F;
constexpr float kModalHeight = 360.0F;
constexpr float kRowTop = 84.0F;
constexpr float kRowHeight = 40.0F;
constexpr float kRowGap = 20.0F;
constexpr float kNameWidth = 170.0F;
constexpr float kSliderWidth = 200.0F;
constexpr float kMuteWidth = 100.0F;
constexpr float kColumnGap = 16.0F;
constexpr float kPadding = 28.0F;
constexpr float kButtonWidth = 200.0F;
constexpr float kButtonHeight = 56.0F;
// The sand panel's shading, a step darker than the panel, for the slider track and the check box.
constexpr core::Rgba kWellColor{.r = 222, .g = 196, .b = 146, .a = 255};

// Which half of AudioSettings a row edits.
struct VolumeRow {
	const char* name;
	const char* id; // names the row's widgets for the Remote API, e.g. "MusicSlider"
	float AudioSettings::* volume;
	bool AudioSettings::* muted;
};

void BuildRow(const flecs::world& world, const flecs::entity modal, const VolumeRow& row, const float x, const float y) {
	const AudioSettings& current = world.get<AudioSettings>();
	std::ignore = ui::CreateLabel(world, {.x = x, .y = y, .w = kNameWidth, .h = kRowHeight}, row.name, 28.0F)
					  .set<ui::Label>({.text = row.name, .font_size = 28.0F, .color = kPanelTextColor, .align = ui::TextAlign::Left})
					  .child_of(modal);

	const float sliderX = x + kNameWidth + kColumnGap;
	// Whole sets rather than get_mut on the factory's result: a scene's Load() runs deferred, so it isn't readable yet.
	std::ignore = world.entity((std::string{row.id} + "Slider").c_str())
					  .child_of(modal)
					  .set<ui::UIRect>({{.x = sliderX, .y = y, .w = kSliderWidth, .h = kRowHeight}})
					  .set<ui::Slider>({
						  .value = current.*row.volume,
						  .track_color = kWellColor,
						  .fill_color = kCoralColor,
						  .thumb_color = kPanelTextColor,
						  .thumb_hover_color = kCoralColor,
						  .thumb_radius = 11.0F,
						  .track_height = 10.0F,
						  .on_change =
							  [w = world, volume = row.volume](const float value) {
								  AudioSettings settings = w.get<AudioSettings>();
								  settings.*volume = value;
								  SetAudioSettings(w, settings);
							  },
					  });

	std::ignore = world.entity((std::string{row.id} + "Mute").c_str())
					  .child_of(modal)
					  .set<ui::UIRect>({{.x = sliderX + kSliderWidth + kColumnGap * 2.0F, .y = y, .w = kMuteWidth, .h = kRowHeight}})
					  .set<ui::Checkbox>({
						  .checked = current.*row.muted,
						  .label = "Mute",
						  .font_size = 24.0F,
						  .box_size = 26.0F,
						  .box_color = kWellColor,
						  .check_color = kCoralColor,
						  .border_color = kPanelTextColor,
						  .text_color = kPanelTextColor,
						  .on_change =
							  [w = world, muted = row.muted](const bool checked) {
								  AudioSettings settings = w.get<AudioSettings>();
								  settings.*muted = checked;
								  SetAudioSettings(w, settings);
							  },
					  });
}

} // namespace

flecs::entity BuildSettingsModal(const flecs::world& world, const flecs::entity parent, const float width, const float height) {
	const float x = (width - kModalWidth) / 2.0F;
	const float y = (height - kModalHeight) / 2.0F;
	const flecs::entity modal =
		ui::CreateModal(world, {.x = x, .y = y, .w = kModalWidth, .h = kModalHeight}, {.open = false})
			.set_name("SettingsModal")
			.child_of(parent);
	std::ignore =
		ui::CreateLabel(world, {.x = x, .y = y + 16.0F, .w = kModalWidth, .h = 44.0F}, "Settings", 40.0F).child_of(modal);

	BuildRow(world, modal, {.name = "Music", .id = "Music", .volume = &AudioSettings::music_volume, .muted = &AudioSettings::music_muted}, x + kPadding, y + kRowTop);
	BuildRow(
		world,
		modal,
		{.name = "Sound effects", .id = "Sfx", .volume = &AudioSettings::sfx_volume, .muted = &AudioSettings::sfx_muted},
		x + kPadding,
		y + kRowTop + kRowHeight + kRowGap
	);

	std::ignore = world.entity("FullscreenCheckbox")
					  .child_of(modal)
					  .set<ui::UIRect>({{.x = x + kPadding, .y = y + kRowTop + (kRowHeight + kRowGap) * 2.0F, .w = kNameWidth * 2.0F, .h = kRowHeight}})
					  .set<ui::Checkbox>({
						  .checked = world.get<DisplaySettings>().fullscreen,
						  .label = "Fullscreen",
						  .font_size = 28.0F,
						  .box_size = 26.0F,
						  .box_color = kWellColor,
						  .check_color = kCoralColor,
						  .border_color = kPanelTextColor,
						  .text_color = kPanelTextColor,
						  .on_change = [w = world](const bool checked) { SetDisplaySettings(w, {.fullscreen = checked}); },
					  });

	std::ignore = ui::CreateButton(
					  world,
					  {.x = x + (kModalWidth - kButtonWidth) / 2.0F, .y = y + kModalHeight - kButtonHeight - 24.0F, .w = kButtonWidth, .h = kButtonHeight},
					  "Back",
					  [modal](flecs::entity) { modal.get_mut<ui::Modal>().open = false; }
	)
					  .set_name("SettingsBackButton")
					  .child_of(modal);
	return modal;
}

} // namespace crabnado
