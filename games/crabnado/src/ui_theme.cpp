#include "ui_theme.hpp"

#include "crabnado_constants.hpp"

using namespace engine;

namespace crabnado {

namespace {

// The title logo's outline navy, so panels and buttons read as part of the same set.
constexpr core::Rgba kOutlineColor = kPanelTextColor;
constexpr core::Rgba kSandColor{.r = 247, .g = 229, .b = 188, .a = 255};
constexpr core::Rgba kCoralHoverColor{.r = 242, .g = 122, .b = 82, .a = 255};
constexpr core::Rgba kCoralPressedColor{.r = 184, .g = 70, .b = 42, .a = 255};
constexpr core::Rgba kButtonTextColor{.r = 255, .g = 244, .b = 220, .a = 255};
// Deep water, dark enough that a menu stands clear of the scene behind it.
constexpr core::Rgba kBackdropColor{.r = 4, .g = 18, .b = 34, .a = 215};

} // namespace

void ApplyUiTheme(const flecs::world& world) {
	auto* platform = world.get<platform::PlatformRef>().ptr;
	std::ignore = platform->SetTextFont(assets::ResolveAsset(world, constants::kUiFontPath));

	auto& config = world.get_mut<ui::UIConfig>();
	config.button = {
		.font_size = 26.0F,
		.normal = kCoralColor,
		.hover = kCoralHoverColor,
		.pressed_color = kCoralPressedColor,
		.border_color = kOutlineColor,
		.text_color = kButtonTextColor,
		.roundness = 0.4F,
		.border_thickness = 4.0F,
	};
	config.modal_panel = {.color = kSandColor, .border_color = kOutlineColor, .border_thickness = 5.0F, .roundness = 0.08F};
	config.modal_backdrop = kBackdropColor;
	config.label_color = kPanelTextColor;
}

} // namespace crabnado
