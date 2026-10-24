#include "arena_scene.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

#include "bubbles.hpp"
#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "arena.hpp"
#include "claw_snap.hpp"
#include "crabnado.hpp"
#include "horde.hpp"
#include "player.hpp"
#include "run.hpp"
#include "settings.hpp"
#include "settings_menu.hpp"
#include "sfx.hpp"
#include "ui_theme.hpp"
#include "enemy_types.hpp"
#include "upgrades.hpp"
#include "riptide.hpp"
#include "waves.hpp"
#include "wind.hpp"

using namespace engine;

namespace crabnado {

namespace {

// The deep sea under the water tiles, showing through the gaps between them and past the farthest tiles.
constexpr core::Rgba kSeabedColor{.r = 0x0f, .g = 0x3a, .b = 0x55, .a = 255};
constexpr core::Rgba kResultsHeadingColor{.r = 200, .g = 50, .b = 35, .a = 255};
constexpr core::Rgba kTelegraphColor{.r = 255, .g = 209, .b = 102, .a = 255};
constexpr core::Rgba kVictoryHeadingColor{.r = 40, .g = 150, .b = 80, .a = 255};
constexpr core::Rgba kBossColor{.r = 170, .g = 40, .b = 30, .a = 255};
constexpr core::Rgba kBannerColor{.r = 255, .g = 244, .b = 220, .a = 255};
// The start countdown's numbers: kCountdownFontSize, popping in kCountdownPop larger.
constexpr float kCountdownFontSize = 120.0F;
constexpr float kCountdownPop = 0.5F;
constexpr core::Rgba kRogueColor{.r = 235, .g = 60, .b = 50, .a = 255};
constexpr core::Rgba kStaminaBarColor{.r = 120, .g = 200, .b = 230, .a = 255};
constexpr core::Rgba kExhaustedBarColor{.r = 140, .g = 140, .b = 140, .a = 255};
// The loadout in the lower left: a row of weapon slots, with the dig-in bar above it. A slot is the weapon's
// icon, a dark overlay over the share of its cooldown still to run, and a level chip on its corner.
constexpr float kLoadoutMargin = 16.0F;
constexpr float kSlotSize = 60.0F;
constexpr float kSlotGap = 8.0F;
constexpr float kWeaponIconSize = 48.0F; // the icon PNGs' size: a textured panel draws its texture unscaled
constexpr float kLevelChipWidth = 26.0F;
constexpr float kLevelChipHeight = 20.0F;
constexpr float kDigInChipWidth = 82.0F;
constexpr float kDigInChipHeight = 26.0F;
constexpr float kDigInBarWidth = 180.0F;
constexpr float kDigInBarHeight = 18.0F;
constexpr core::Rgba kCooldownShade{.r = 0, .g = 0, .b = 0, .a = 150};
constexpr core::Rgba kDimmedChipColor{.r = 60, .g = 70, .b = 84, .a = 200};
constexpr core::Rgba kDimmedTextColor{.r = 150, .g = 150, .b = 150, .a = 255};
// The shell bar toward the next molt, top centre, with the molt-level chip capping its right end (overlapping
// it, so it reads as attached).
constexpr float kMoltBarWidth = 400.0F;
constexpr float kMoltBarTop = 16.0F;
constexpr float kMoltBarHeight = 26.0F;
constexpr float kChipOverlap = 10.0F;
constexpr float kChipRise = 4.0F; // how far a chip sticks out above and below the bar
constexpr float kMoltChipWidth = 48.0F; // room for two digits
constexpr float kHudOutline = 2.0F;
// Crab orange, against a dark track; the chips are navy so they stand apart from the fill.
constexpr core::Rgba kMoltFillColor{.r = 242, .g = 118, .b = 46, .a = 255};
constexpr core::Rgba kMoltTrackColor{.r = 8, .g = 20, .b = 34, .a = 170};
constexpr core::Rgba kChipColor{.r = 18, .g = 40, .b = 70, .a = 255};
constexpr core::Rgba kChipBorderColor{.r = 255, .g = 244, .b = 220, .a = 255};
constexpr core::Rgba kChipTextColor{.r = 255, .g = 209, .b = 102, .a = 255};
constexpr core::Rgba kHudTextColor{.r = 255, .g = 255, .b = 255, .a = 255};
constexpr core::Rgba kHudOutlineColor{.r = 0, .g = 0, .b = 0, .a = 255};
// The run-stats panel in the lower right: one row each for the run clock and kills, an icon then the value.
constexpr float kStatsPanelWidth = 132.0F;
constexpr float kStatsRowHeight = 30.0F;
constexpr float kStatsPadding = 8.0F;
constexpr float kStatsMargin = 16.0F;
constexpr float kStatsIconSize = 22.0F; // the icon PNGs' size: a textured panel draws its texture unscaled
constexpr core::Rgba kStatsPanelColor{.r = 18, .g = 40, .b = 70, .a = 200};
constexpr std::string_view kTimeIconPath = "textures/crabnado/hud/time.png";
constexpr std::string_view kKillsIconPath = "textures/crabnado/hud/kills.png";
constexpr float kMoltModalWidth = 850.0F;
constexpr float kMoltModalHeight = 500.0F;
// Molt cards, measured from the modal's top (kCardsTop) or the card's top (the rest).
constexpr float kCardsTop = 64.0F;
constexpr float kCardWidth = 250.0F;
constexpr float kCardHeight = 340.0F;
constexpr float kCardGap = 20.0F;
constexpr float kCardPadding = 14.0F;
constexpr float kIconSize = 96.0F; // the icon PNGs' size: a textured panel draws its texture unscaled
constexpr float kStatsTop = 226.0F;
constexpr float kStatLineHeight = 30.0F;
constexpr float kStatFontSize = 19.0F;
constexpr float kDeltaFontSize = 15.0F;
constexpr float kStatGap = 5.0F;
// The arrow icon's size: a textured panel draws its texture unscaled.
constexpr float kArrowWidth = 20.0F;
constexpr float kArrowHeight = 14.0F;
constexpr std::string_view kArrowIconPath = "textures/crabnado/upgrades/arrow.png";
constexpr float kSelectedBorder = 6.0F;
constexpr float kConfirmWidth = 220.0F;
constexpr core::Rgba kSelectedCardColor{.r = 255, .g = 232, .b = 212, .a = 255};
constexpr core::Rgba kDisabledButtonColor{.r = 176, .g = 170, .b = 160, .a = 255};
const ui::Button kCardStyle{
	.normal = {.r = 255, .g = 251, .b = 240, .a = 255},
	.hover = {.r = 255, .g = 242, .b = 222, .a = 255},
	.pressed_color = {.r = 250, .g = 228, .b = 196, .a = 255},
	.border_color = kPanelTextColor,
	.roundness = 0.1F,
	.border_thickness = 3.0F,
};
constexpr float kResultsModalWidth = 520.0F;
constexpr float kResultsModalHeight = 380.0F;
constexpr float kPauseModalWidth = 360.0F;
constexpr float kPauseModalHeight = 312.0F;
constexpr float kModalButtonWidth = 200.0F;
constexpr float kModalButtonHeight = 56.0F;
constexpr float kChoiceGap = 16.0F;
// During the boss the telegraph always shows the next big crest, not only in the last seconds.
constexpr float kBossCrestLookahead = 3.0F * constants::kWavePeriod;
constexpr float kCompassRadius = 56.0F;
constexpr float kCompassMargin = 30.0F;
constexpr float kCompassTop = 20.0F;
// Where the crabnado starts, away from the player at the centre.
constexpr glm::vec2 kCrabnadoStart{30.0F, -25.0F};
// Scattered rocks keep this clear of the player's start (the centre) and the crabnado's.
constexpr float kStartClearance = 5.0F;

// A stat value with at most two decimals and no trailing zeros: "100", "3.6", "0.96".
std::string FormatStat(const float value) {
	std::string text = std::format("{:.2f}", value);
	text.erase(text.find_last_not_of('0') + 1);
	if (text.back() == '.') {
		text.pop_back();
	}
	return text == "-0" ? "0" : text;
}

} // namespace

void ArenaScene::InitializePipeline(const flecs::world& world) {
	// clang-format off
	pipeline_ = world.pipeline()
					.with(flecs::System)
					.without<scene::MenuScene>()
					.with(flecs::Phase).cascade(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::ChildOf)
					.build();

	pausedPipeline_ = world.pipeline()
					.with(flecs::System)
					.without<scene::MenuScene>()
					.without<ecs::Pausable>()
					.with(flecs::Phase).cascade(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::ChildOf)
					.build();
	// clang-format on
}

void ArenaScene::RegisterLoaders(const flecs::world& world) {
	RegisterArena(world);
}

void ArenaScene::Load(flecs::world& world) {
	StartRun(world);
	LoadEnemyTypes(world, world.get<platform::PlatformRef>().ptr->AssetDirectory() + std::string{constants::kEnemiesPath});
	StartWind(world);
	StartWaves(world);
	sceneRoot_ = world.entity("Arena");
	crestCued_ = false;
	rogueCued_ = false;
	music_ = world.entity("Music")
				 .child_of(sceneRoot_)
				 .set<audio::Music>({.path = assets::ResolveAsset(world, "audio/crabnado/music.ogg"), .loop = true})
				 .set<MusicLevel>({constants::kMusicVolume});
	horde_ = world.entity().child_of(sceneRoot_);
	BuildStage(world);
	BuildUI(world);
}

void ArenaScene::BuildStage(const flecs::world& world) {
	world.set<render::AmbientLight>({.color = {.r = 120, .g = 130, .b = 150, .a = 255}, .intensity = 0.45F});

	// A scene Load runs in a deferred frame, so the level's entities wouldn't exist for BuildArena to cache.
	const bool deferred = world.is_deferred();
	if (deferred) {
		world.defer_suspend();
	}
	const std::array<RockCircle, 2> keepClear{
		{{.center = glm::vec2{0.0F}, .radius = kStartClearance}, {.center = kCrabnadoStart, .radius = kStartClearance}}};
	BuildArena(world, sceneRoot_, keepClear);
	if (deferred) {
		world.defer_resume();
	}
	const flecs::entity player = SpawnPlayer(world, sceneRoot_);
	AddClawSnap(world, player);
	AddBubbles(player);
	AddRiptide(player);
	crabnado_ = SpawnCrabnado(world, sceneRoot_, horde_, kCrabnadoStart);
	SpawnWaterTiles(world, sceneRoot_);
	SpawnWindStreaks(world, sceneRoot_);

	const glm::vec3 cameraOffset{0.0F, constants::kCameraHeight, constants::kCameraBack};
	std::ignore = world.entity("Camera")
					  .child_of(sceneRoot_)
					  .set<spatial::Transform>({.position = cameraOffset})
					  .set<render::Camera>({.fov = 60.0F, .far_plane = 250.0F})
					  .set<FollowCamera>({.offset = cameraOffset})
					  .add<render::LookAt>(player);

	std::ignore = world.entity("Sun").child_of(sceneRoot_).set<render::DirectionalLight>({});

	std::ignore = world.entity("Seabed")
					  .child_of(sceneRoot_)
					  .set<spatial::Transform>({.position = {0.0F, constants::kSeabedY, 0.0F}})
					  .set<render::QuadPrimitive>({.size = glm::vec2{2.0F * constants::kSeabedReach}})
					  .set<render::Material>({.color = kSeabedColor, .cast_shadow = false, .unlit = true});
}

void ArenaScene::BuildUI(const flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());

	uiRoot_ = world.entity("ArenaUI");
	uiRoot_.set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = w, .h = h}});

	const float barX = (w - kMoltBarWidth) / 2.0F;
	// White with a dark outline so it reads over the water.
	hud_ = ui::CreateLabel(world, {.x = 16.0F, .y = 12.0F, .w = barX - 32.0F, .h = 28.0F}, "", 24.0F)
			   .set<ui::Label>(
				   {.font_size = 24.0F,
					.color = kHudTextColor,
					.align = ui::TextAlign::Left,
					.outline = kHudOutline,
					.outline_color = kHudOutlineColor}
			   )
			   .set_name("Hud")
			   .child_of(uiRoot_);

	const core::Rect barRect{.x = barX, .y = kMoltBarTop, .w = kMoltBarWidth, .h = kMoltBarHeight};
	moltBar_ = ui::CreateProgressBar(
				   world,
				   barRect,
				   {.fill_color = kMoltFillColor, .track_color = kMoltTrackColor, .border_color = kChipColor, .roundness = 0.5F}
	)
				   .set_name("MoltBar")
				   .child_of(uiRoot_);
	moltLabel_ = ui::CreateLabel(world, barRect, "", 18.0F)
					 .set<ui::Label>({.font_size = 18.0F, .color = kHudTextColor, .outline = kHudOutline, .outline_color = kChipColor})
					 .set_name("MoltLabel")
					 .child_of(moltBar_);
	// Children of the bar, so they draw over it: siblings draw in storage order, not creation order.
	const auto chip = [&](const float x, const float width, const char* name) {
		return world.entity(name)
			.set<ui::UIRect>({{.x = x, .y = kMoltBarTop - kChipRise, .w = width, .h = kMoltBarHeight + 2.0F * kChipRise}})
			.set<ui::Panel>({.color = kChipColor, .border_color = kChipBorderColor, .border_thickness = 2.0F, .roundness = 0.5F})
			.set<ui::Label>({.font_size = 20.0F, .color = kChipTextColor})
			.child_of(moltBar_);
	};
	moltChip_ = chip(barX + kMoltBarWidth - kChipOverlap, kMoltChipWidth, "MoltChip");
	BuildStatsPanel(world, w, h);
	const ui::Label alertLabel{.font_size = 26.0F, .color = kTelegraphColor, .outline = kHudOutline, .outline_color = kHudOutlineColor};
	telegraph_ = ui::CreateLabel(world, {.x = barX, .y = 52.0F, .w = kMoltBarWidth, .h = 28.0F}, "", 26.0F)
					 .set<ui::Label>(alertLabel)
					 .set_name("Telegraph")
					 .child_of(uiRoot_);
	bossLabel_ = ui::CreateLabel(world, {.x = barX, .y = 84.0F, .w = kMoltBarWidth, .h = 28.0F}, "", 24.0F)
					 .set<ui::Label>({.font_size = 24.0F, .color = kBossColor, .outline = kHudOutline, .outline_color = kHudOutlineColor})
					 .set_name("BossLabel")
					 .child_of(uiRoot_);
	bannerLabel_ = ui::CreateLabel(world, {.x = 0.0F, .y = h * 0.28F, .w = w, .h = 44.0F}, "", 38.0F)
					   .set<ui::Label>({.font_size = 38.0F, .color = kBannerColor, .outline = 3.0F, .outline_color = kHudOutlineColor})
					   .set_name("BannerLabel")
					   .child_of(uiRoot_);
	countdownLabel_ = ui::CreateLabel(world, {.x = 0.0F, .y = h * 0.3F, .w = w, .h = kCountdownFontSize * 2.0F}, "", kCountdownFontSize)
						  .set<ui::Label>({.font_size = kCountdownFontSize, .color = kBannerColor, .outline = 6.0F, .outline_color = kHudOutlineColor})
						  .set_name("CountdownLabel")
						  .child_of(uiRoot_);
	BuildLoadout(world, h);
	BuildMoltModal(world, w, h);

	// Top right. The WindCompassDraw system draws it into the UI draw list.
	const glm::vec2 compassCenter{w - kCompassMargin - kCompassRadius, kCompassTop + kCompassRadius};
	std::ignore = world.entity("WindCompass")
					  .child_of(uiRoot_)
					  .set<WindCompass>({.center = compassCenter, .radius = kCompassRadius});
	// The sea state, under the compass (the compass draws its shift warning below this).
	waveLabel_ = ui::CreateLabel(
					 world,
					 {.x = compassCenter.x - 110.0F, .y = compassCenter.y + kCompassRadius + 6.0F, .w = 220.0F, .h = 26.0F},
					 "",
					 20.0F
	)
					 .set<ui::Label>({.font_size = 20.0F, .color = kHudTextColor, .outline = kHudOutline, .outline_color = kHudOutlineColor})
					 .set_name("WaveLabel")
					 .child_of(uiRoot_);

	BuildResultsModal(world, w, h);
	BuildPauseModal(world, w, h);
}

void ArenaScene::BuildLoadout(const flecs::world& world, const float height) {
	const float slotY = height - kLoadoutMargin - kSlotSize;
	constexpr std::array<std::string_view, kWeaponCount> kIcons{
		"textures/crabnado/weapons/claw_snap.png", "textures/crabnado/weapons/bubbles.png", "textures/crabnado/weapons/riptide.png"
	};
	for (int i = 0; i < kWeaponCount; ++i) {
		const float x = kLoadoutMargin + static_cast<float>(i) * (kSlotSize + kSlotGap);
		WeaponSlot& slot = weaponSlots_[static_cast<size_t>(i)];
		slot.slot = ui::CreatePanel(
						world,
						{.x = x, .y = slotY, .w = kSlotSize, .h = kSlotSize},
						{.color = kStatsPanelColor, .border_color = kChipBorderColor, .border_thickness = 2.0F, .roundness = 0.25F}
		)
						.set<ui::UIElement>({})
						.child_of(uiRoot_);
		// Each piece is the child of the one under it, so they draw in this order (siblings draw in storage order).
		const float inset = (kSlotSize - kWeaponIconSize) / 2.0F;
		const flecs::entity icon =
			ui::CreatePanel(
				world,
				{.x = x + inset, .y = slotY + inset, .w = kWeaponIconSize, .h = kWeaponIconSize},
				{.color = core::colors::White, .border_thickness = 0.0F, .texture = AcquireTexture(world, kIcons[static_cast<size_t>(i)])}
			)
				.child_of(slot.slot);
		// UpdateWeapons sizes the shade each frame.
		slot.shade = ui::CreatePanel(world, {}, {.color = kCooldownShade, .border_thickness = 0.0F, .roundness = 0.25F})
						 .child_of(icon);
		slot.level = world.entity()
						 .set<ui::UIRect>(
							 {{.x = x + kSlotSize - kLevelChipWidth + 6.0F,
							   .y = slotY + kSlotSize - kLevelChipHeight + 6.0F,
							   .w = kLevelChipWidth,
							   .h = kLevelChipHeight}}
						 )
						 .set<ui::Panel>({.color = kChipColor, .border_color = kChipBorderColor, .border_thickness = 2.0F, .roundness = 0.5F})
						 .set<ui::Label>({.font_size = 15.0F, .color = kChipTextColor})
						 .child_of(slot.shade);
	}

	// The dig-in bar sits above the slots, capped on the left by its chip like the molt bar.
	const float chipY = slotY - 14.0F - kDigInChipHeight;
	const core::Rect barRect{
		.x = kLoadoutMargin + kDigInChipWidth - kChipOverlap,
		.y = chipY + (kDigInChipHeight - kDigInBarHeight) / 2.0F,
		.w = kDigInBarWidth,
		.h = kDigInBarHeight
	};
	staminaBar_ = ui::CreateProgressBar(
					  world,
					  barRect,
					  {.fill_color = kStaminaBarColor, .track_color = kMoltTrackColor, .border_color = kChipColor, .roundness = 0.5F}
	)
					  .set_name("StaminaBar")
					  .child_of(uiRoot_);
	digInChip_ = world.entity("DigInChip")
					 .set<ui::UIRect>({{.x = kLoadoutMargin, .y = chipY, .w = kDigInChipWidth, .h = kDigInChipHeight}})
					 .set<ui::Panel>({.color = kChipColor, .border_color = kChipBorderColor, .border_thickness = 2.0F, .roundness = 0.5F})
					 .set<ui::Label>({.text = "DIG IN", .font_size = 17.0F, .color = kChipTextColor})
					 .child_of(staminaBar_);
	// DUG IN or SURFING, right of the bar.
	stateLabel_ = ui::CreateLabel(world, {.x = barRect.x + barRect.w + 10.0F, .y = chipY, .w = 160.0F, .h = 26.0F}, "", 20.0F)
					  .set<ui::Label>(
						  {.font_size = 20.0F,
						   .color = kHudTextColor,
						   .align = ui::TextAlign::Left,
						   .outline = kHudOutline,
						   .outline_color = kHudOutlineColor}
					  )
					  .set_name("StateLabel")
					  .child_of(uiRoot_);
}

void ArenaScene::UpdateWeapons(const flecs::entity player) const {
	struct Charge {
		float level;
		float timer;
		float cooldown;
	};
	const auto& claw = player.get<ClawSnap>();
	const auto& bubbles = player.get<Bubbles>();
	const auto& riptide = player.get<Riptide>();
	const std::array<Charge, kWeaponCount> charges{
		Charge{claw.level, claw.timer, claw.cooldown},
		Charge{bubbles.level, bubbles.timer, bubbles.cooldown},
		Charge{riptide.level, riptide.timer, riptide.cooldown},
	};
	// A locked weapon (level 0: Riptide before its upgrade) is hidden. It's the last slot, so no gap opens.
	for (int i = 0; i < kWeaponCount; ++i) {
		const WeaponSlot& slot = weaponSlots_[static_cast<size_t>(i)];
		if (!slot.slot.has<ui::UIElement>() || !slot.shade.has<ui::UIRect>() || !slot.level.has<ui::Label>()) {
			continue;
		}
		const Charge& charge = charges[static_cast<size_t>(i)];
		const bool unlocked = charge.level > 0.0F;
		slot.slot.get_mut<ui::UIElement>().visible = unlocked;
		if (!unlocked) {
			continue;
		}
		// The shade covers the top share of the icon still to recharge, shrinking upward as the weapon readies.
		const float remaining = charge.cooldown > 0.0F ? std::clamp(charge.timer / charge.cooldown, 0.0F, 1.0F) : 0.0F;
		const core::Rect icon = slot.shade.parent().get<ui::UIRect>().rect;
		slot.shade.get_mut<ui::UIRect>().rect = {.x = icon.x, .y = icon.y, .w = icon.w, .h = icon.h * remaining};
		slot.level.get_mut<ui::Label>().text = std::format("{}", static_cast<int>(charge.level));
	}
}

void ArenaScene::BuildStatsPanel(const flecs::world& world, const float width, const float height) {
	const float panelHeight = 2.0F * kStatsRowHeight + 2.0F * kStatsPadding;
	const float x = width - kStatsMargin - kStatsPanelWidth;
	const float y = height - kStatsMargin - panelHeight;
	const flecs::entity panel =
		ui::CreatePanel(
			world,
			{.x = x, .y = y, .w = kStatsPanelWidth, .h = panelHeight},
			{.color = kStatsPanelColor, .border_color = kChipBorderColor, .border_thickness = 2.0F, .roundness = 0.25F}
		)
			.set_name("StatsPanel")
			.child_of(uiRoot_);
	const auto row = [&](const int index, const std::string_view icon, const char* name) {
		const float rowY = y + kStatsPadding + static_cast<float>(index) * kStatsRowHeight;
		std::ignore = ui::CreatePanel(
						  world,
						  {.x = x + 12.0F, .y = rowY + (kStatsRowHeight - kStatsIconSize) / 2.0F, .w = kStatsIconSize, .h = kStatsIconSize},
						  {.color = core::colors::White, .border_thickness = 0.0F, .texture = AcquireTexture(world, icon)}
		)
						  .child_of(panel);
		return ui::CreateLabel(world, {.x = x + 44.0F, .y = rowY, .w = kStatsPanelWidth - 56.0F, .h = kStatsRowHeight}, "", 22.0F)
			.set<ui::Label>({.font_size = 22.0F, .color = kChipBorderColor, .align = ui::TextAlign::Left})
			.set_name(name)
			.child_of(panel);
	};
	timeLabel_ = row(0, kTimeIconPath, "TimeLabel");
	killsLabel_ = row(1, kKillsIconPath, "KillsLabel");
}

int ArenaScene::AcquireTexture(const flecs::world& world, const std::string_view path) {
	std::string resolved = assets::ResolveAsset(world, path);
	const int handle = assets::LoadTexture(world, resolved);
	textures_.push_back(std::move(resolved));
	return handle;
}

void ArenaScene::BuildMoltModal(const flecs::world& world, const float width, const float height) {
	const float x = (width - kMoltModalWidth) / 2.0F;
	const float y = (height - kMoltModalHeight) / 2.0F;
	moltModal_ =
		ui::CreateModal(world, {.x = x, .y = y, .w = kMoltModalWidth, .h = kMoltModalHeight}, {.open = false})
			.set_name("MoltModal")
			.child_of(uiRoot_);
	std::ignore = ui::CreateLabel(world, {.x = x, .y = y + 14.0F, .w = kMoltModalWidth, .h = 40.0F}, "Molt! Choose an upgrade", 34.0F)
					  .child_of(moltModal_);

	const int arrowTexture = AcquireTexture(world, kArrowIconPath);
	const float rowWidth = kMoltChoices * kCardWidth + (kMoltChoices - 1) * kCardGap;
	const float top = y + kCardsTop;
	for (int i = 0; i < kMoltChoices; ++i) {
		const float cx = x + (kMoltModalWidth - rowWidth) / 2.0F + static_cast<float>(i) * (kCardWidth + kCardGap);
		MoltCard& card = moltCards_[i];
		card.card = ui::CreateButton(
						world,
						{.x = cx, .y = top, .w = kCardWidth, .h = kCardHeight},
						"",
						[i](const flecs::entity button) { SelectUpgrade(button.world(), i); }
		)
						.set<ui::Button>(kCardStyle)
						.set<ui::UIElement>({})
						.set_name(std::format("Card{}", i + 1).c_str())
						.child_of(moltModal_);
		card.icon = ui::CreatePanel(
						world,
						{.x = cx + (kCardWidth - kIconSize) / 2.0F, .y = top + 16.0F, .w = kIconSize, .h = kIconSize},
						{.color = core::colors::White, .border_thickness = 0.0F}
		)
						.child_of(card.card);
		card.title = ui::CreateLabel(world, {.x = cx, .y = top + 118.0F, .w = kCardWidth, .h = 32.0F}, "", 26.0F)
						 .child_of(card.card);
		card.description =
			ui::CreateLabel(world, {.x = cx + kCardPadding, .y = top + 154.0F, .w = kCardWidth - 2.0F * kCardPadding, .h = 66.0F}, "", 18.0F)
				.set<ui::Label>({.font_size = 18.0F, .color = kPanelTextColor, .wrap = true})
				.child_of(card.card);
		for (StatLine& line : card.stats) {
			// UpdateMoltCard sets the text, colour, position and visibility each frame.
			const auto statLabel = [&](const float size) {
				return ui::CreateLabel(world, {}, "", size)
					.set<ui::Label>({.font_size = size, .color = kPanelTextColor, .align = ui::TextAlign::Left})
					.child_of(card.card);
			};
			line.before = statLabel(kStatFontSize);
			line.arrow = ui::CreatePanel(world, {}, {.color = core::colors::White, .border_thickness = 0.0F, .texture = arrowTexture})
							 .set<ui::UIElement>({})
							 .child_of(card.card);
			line.after = statLabel(kStatFontSize);
			line.delta = statLabel(kDeltaFontSize);
		}
	}

	confirmButton_ = ui::CreateButton(
						 world,
						 {.x = x + (kMoltModalWidth - kConfirmWidth) / 2.0F, .y = top + kCardHeight + 20.0F, .w = kConfirmWidth, .h = kModalButtonHeight},
						 "Confirm",
						 [](const flecs::entity button) { ConfirmUpgrade(button.world()); }
	)
						 .set_name("ConfirmButton")
						 .child_of(moltModal_);
}

void ArenaScene::BuildResultsModal(const flecs::world& world, const float width, const float height) {
	const float x = (width - kResultsModalWidth) / 2.0F;
	const float y = (height - kResultsModalHeight) / 2.0F;
	resultsModal_ =
		ui::CreateModal(world, {.x = x, .y = y, .w = kResultsModalWidth, .h = kResultsModalHeight}, {.open = false})
			.set_name("ResultsModal")
			.child_of(uiRoot_);
	resultsHeading_ = ui::CreateLabel(world, {.x = x, .y = y + 24.0F, .w = kResultsModalWidth, .h = 56.0F}, "", 48.0F)
						  .set<ui::Label>({.font_size = 48.0F, .color = kResultsHeadingColor})
						  .child_of(resultsModal_);
	resultsStats_ =
		ui::CreateLabel(world, {.x = x, .y = y + 96.0F, .w = kResultsModalWidth, .h = 120.0F}, "", 26.0F)
			.set<ui::Label>({.font_size = 26.0F, .color = kPanelTextColor})
			.child_of(resultsModal_);

	const float rowWidth = 2.0F * kModalButtonWidth + kChoiceGap;
	const float bx = x + (kResultsModalWidth - rowWidth) / 2.0F;
	const float by = y + kResultsModalHeight - kModalButtonHeight - 28.0F;
	std::ignore = ui::CreateButton(
					  world,
					  {.x = bx, .y = by, .w = kModalButtonWidth, .h = kModalButtonHeight},
					  "Retry",
					  [](const flecs::entity button) {
						  // Outside the click's deferred frame: there the reload's remove and re-add of Active
						  // collapse into nothing and the scene is never unloaded.
						  button.world().get<EngineContextRef>().ptr->GetDeferredTaskQueue().Enqueue(
							  [world = flecs::world(button.world())] { scene::ReloadScene<ArenaSceneTag>(world); }
						  );
					  }
	)
					  .set_name("RetryButton")
					  .child_of(resultsModal_);
	std::ignore = ui::CreateButton(
					  world,
					  {.x = bx + kModalButtonWidth + kChoiceGap, .y = by, .w = kModalButtonWidth, .h = kModalButtonHeight},
					  "Title",
					  [onTitle = onTitlePressed](flecs::entity) {
						  if (onTitle) {
							  onTitle();
						  }
					  }
	)
					  .set_name("ResultsTitleButton")
					  .child_of(resultsModal_);
}

void ArenaScene::BuildPauseModal(const flecs::world& world, const float width, const float height) {
	const float x = (width - kPauseModalWidth) / 2.0F;
	const float y = (height - kPauseModalHeight) / 2.0F;
	pauseModal_ = ui::CreateModal(world, {.x = x, .y = y, .w = kPauseModalWidth, .h = kPauseModalHeight}, {.open = false})
					  .set_name("PauseModal")
					  .child_of(uiRoot_);
	std::ignore =
		ui::CreateLabel(world, {.x = x, .y = y + 20.0F, .w = kPauseModalWidth, .h = 44.0F}, "Paused", 40.0F)
			.child_of(pauseModal_);
	const float bx = x + (kPauseModalWidth - kModalButtonWidth) / 2.0F;
	std::ignore = ui::CreateButton(
					  world,
					  {.x = bx, .y = y + 88.0F, .w = kModalButtonWidth, .h = kModalButtonHeight},
					  "Resume",
					  [](const flecs::entity button) { SetMenuPaused(button.world(), false); }
	)
					  .set_name("ResumeButton")
					  .child_of(pauseModal_);
	std::ignore = ui::CreateButton(
					  world,
					  {.x = bx, .y = y + 232.0F, .w = kModalButtonWidth, .h = kModalButtonHeight},
					  "Quit to title",
					  [onTitle = onTitlePressed](flecs::entity) {
						  if (onTitle) {
							  onTitle();
						  }
					  }
	)
					  .set_name("QuitButton")
					  .child_of(pauseModal_);
	std::ignore = ui::CreateButton(
					  world,
					  {.x = bx, .y = y + 160.0F, .w = kModalButtonWidth, .h = kModalButtonHeight},
					  "Settings",
					  [this](flecs::entity) { settingsModal_.get_mut<ui::Modal>().open = true; }
	)
					  .set_name("PauseSettingsButton")
					  .child_of(pauseModal_);
	settingsModal_ = BuildSettingsModal(world, uiRoot_, width, height);
}

void ArenaScene::UpdateResults(const flecs::world& world) const {
	if (!resultsModal_.has<ui::Modal>() || !pauseModal_.has<ui::Modal>() || !settingsModal_.has<ui::Modal>() ||
		!resultsHeading_.has<ui::Label>() || !resultsStats_.has<ui::Label>()) {
		return;
	}
	const auto& run = world.get<RunState>();
	const bool won = run.phase == RunPhase::Won;
	const bool ended = won || run.phase == RunPhase::Lost;
	resultsModal_.get_mut<ui::Modal>().open = ended;
	// Settings opens from the pause menu and stands in for it; unpausing (Esc) closes both.
	auto& settings = settingsModal_.get_mut<ui::Modal>();
	settings.open = settings.open && run.menu_paused;
	pauseModal_.get_mut<ui::Modal>().open = run.menu_paused && !settings.open;
	const bool lost = run.phase == RunPhase::Lost;
	if (music_.has<MusicLevel>()) {
		music_.get_mut<MusicLevel>().volume =
			lost ? 0.0F : (ended ? constants::kEndedMusicVolume : constants::kMusicVolume);
	}
	if (lost && !sceneRoot_.lookup("GameOverMusic")) {
		std::ignore = world.entity("GameOverMusic")
						  .child_of(sceneRoot_)
						  .set<audio::Music>({.path = assets::ResolveAsset(world, "audio/crabnado/gameover.ogg"), .loop = true})
						  .set<MusicLevel>({constants::kGameOverMusicVolume});
	}
	if (!ended) {
		return;
	}
	const int seconds = static_cast<int>(run.clock);
	auto& heading = resultsHeading_.get_mut<ui::Label>();
	heading.text = won ? "Victory!" : "Swept away";
	heading.color = won ? kVictoryHeadingColor : kResultsHeadingColor;
	resultsStats_.get_mut<ui::Label>().text = std::format(
		"{}Time {}  {}:{:02}\nKills  {}\nRock slams  {}\nMolt level  {}", won ? "The crabnado breaks apart!\n" : "",
		won ? "to win" : "survived", seconds / 60, seconds % 60, run.kills, run.rock_kills, run.molt_level
	);
}

void ArenaScene::Tick(const flecs::world& world, float) {
	auto* platform = world.get<platform::PlatformRef>().ptr;
	const int enemyCount = world.count<Enemy>();
	world.set<HordeReadout>({.enemy_count = enemyCount, .fps = platform->Fps()});

	UpdateHud(world);

	bool spawn = false;
	bool clear = false;
	auto& debugUi = platform->GetDebugUi();
	if (debugUi.BeginPanel("Horde")) {
		debugUi.Text(std::format("FPS {}   enemies {}", platform->Fps(), enemyCount));
		spawn = debugUi.Button(std::format("Spawn {}", constants::kHordeDebugBatch));
		debugUi.SameLine();
		clear = debugUi.Button("Clear");
	}
	debugUi.EndPanel();

	if (spawn) {
		SpawnBatch(enemyCount);
	}
	if (clear) {
		ClearHorde(world);
	}
}

void ArenaScene::UpdateHud(const flecs::world& world) const {
	const auto& run = world.get<RunState>();
	const flecs::entity player = FindPlayer(world);
	if (!player || !hud_.has<ui::Label>()) {
		return;
	}
	const auto& stats = player.get<PlayerStats>();
	const int seconds = static_cast<int>(run.clock);
	hud_.get_mut<ui::Label>().text = std::format(
		"HP {:.0f}/{:.0f}", std::ceil(stats.hp), stats.max_hp
	);
	if (timeLabel_.has<ui::Label>() && killsLabel_.has<ui::Label>() && waveLabel_.has<ui::Label>()) {
		timeLabel_.get_mut<ui::Label>().text = std::format("{}:{:02}", seconds / 60, seconds % 60);
		killsLabel_.get_mut<ui::Label>().text = std::format("{}", run.kills);
		waveLabel_.get_mut<ui::Label>().text = WaveStageName(run.clock);
	}
	UpdateResults(world);
	UpdateMolt(world);
	UpdateTelegraph(world, player);
	UpdateBoss(world);
	UpdateCountdown(world);
	UpdateStamina(player);
	UpdateWeapons(player);
}

void ArenaScene::UpdateStamina(const flecs::entity player) const {
	if (!staminaBar_.has<ui::ProgressBar>() || !stateLabel_.has<ui::Label>()) {
		return;
	}
	const auto& stamina = player.get<Stamina>();
	auto& bar = staminaBar_.get_mut<ui::ProgressBar>();
	bar.max = stamina.max;
	bar.value = stamina.value;
	bar.fill_color = stamina.exhausted ? kExhaustedBarColor : kStaminaBarColor;
	// Dimmed while exhausted: dig in is recharging and can't be used until the bar refills to a quarter.
	if (digInChip_.has<ui::Panel>() && digInChip_.has<ui::Label>()) {
		digInChip_.get_mut<ui::Panel>().color = stamina.exhausted ? kDimmedChipColor : kChipColor;
		digInChip_.get_mut<ui::Label>().color = stamina.exhausted ? kDimmedTextColor : kChipTextColor;
	}
	const bool surfing = glm::length(player.get<SurfKnock>().velocity) > constants::kSurfingSpeed;
	stateLabel_.get_mut<ui::Label>().text = stamina.dug_in ? "DUG IN" : (surfing ? "SURFING" : "");
}

void ArenaScene::UpdateBoss(const flecs::world& world) const {
	if (!bossLabel_.has<ui::Label>() || !bannerLabel_.has<ui::Label>()) {
		return;
	}
	const auto& run = world.get<RunState>();
	const bool boss = run.phase == RunPhase::Boss;
	bossLabel_.get_mut<ui::Label>().text =
		boss ? std::format("CRABNADO \xC2\xB7 slams left {}", constants::kBossSlamsToWin - run.slams) : "";
	std::string banner;
	if (boss && run.banner_timer > 0.0F) {
		banner = run.slams == 0 ? "The crabnado comes ashore! Slam it into the rocks"
								: std::format("SLAM! {}/{}", run.slams, constants::kBossSlamsToWin);
	}
	bannerLabel_.get_mut<ui::Label>().text = banner;
}

void ArenaScene::UpdateCountdown(const flecs::world& world) const {
	if (!countdownLabel_.has<ui::Label>()) {
		return;
	}
	const auto& run = world.get<RunState>();
	const float countdown = run.countdown;
	auto& label = countdownLabel_.get_mut<ui::Label>();
	if (countdown <= -constants::kGoTime) {
		label.text.clear();
		return;
	}
	// Each number lands big and settles over its second; "Go!" does the same over kGoTime.
	const float settle = countdown > 0.0F ? countdown - std::ceil(countdown) + 1.0F : 1.0F + countdown / constants::kGoTime;
	// The clock only stands at 0 before the first countdown ends, so a later countdown is the resume after a molt.
	if (countdown <= 0.0F) {
		label.text = "Go!";
	}
	else {
		label.text = run.clock > 0.0F ? "Ready..." : std::format("{}", static_cast<int>(std::ceil(countdown)));
	}
	label.font_size = kCountdownFontSize * (1.0F + kCountdownPop * settle * settle * settle);
}

void ArenaScene::UpdateTelegraph(const flecs::world& world, const flecs::entity player) const {
	if (!telegraph_.has<ui::Label>()) {
		return;
	}
	const glm::vec3 p = player.get<spatial::Transform>().position;
	const auto& field = world.get<WaveField>();
	const glm::vec2 position{p.x, p.z};
	auto& label = telegraph_.get_mut<ui::Label>();
	label.color = kTelegraphColor;
	const bool crestComing = NextBigCrestIn(field, position, constants::kBigCrestWarning).has_value();
	if (crestComing && !crestCued_) {
		PlaySfx(world, Sfx::Crest);
	}
	crestCued_ = crestComing;
	const auto rogue = NextRogueIn(field, position, constants::kRogueWarning);
	if (rogue && !rogueCued_) {
		PlaySfx(world, Sfx::RogueWarning);
	}
	rogueCued_ = rogue.has_value();
	if (rogue) {
		label.text = std::format("ROGUE WAVE in {:.1f} s", *rogue);
		label.color = kRogueColor;
	} else if (const auto riptide = BoostedCrestIn(field, position, constants::kBigCrestWarning)) {
		label.text = std::format("RIPTIDE CREST in {:.1f} s", *riptide);
	} else if (const auto crest = NextBigCrestIn(
				   field, position, world.get<RunState>().phase == RunPhase::Boss ? kBossCrestLookahead : constants::kBigCrestWarning
			   )) {
		label.text = std::format("BIG CREST in {:.1f} s", *crest);
	} else {
		label.text = field.notice_timer > 0.0F ? "A cross-swell is building" : "";
	}
}

void ArenaScene::UpdateMolt(const flecs::world& world) const {
	if (!moltBar_.has<ui::ProgressBar>() || !moltLabel_.has<ui::Label>() || !moltModal_.has<ui::Modal>()) {
		return;
	}
	const auto& run = world.get<RunState>();
	auto& bar = moltBar_.get_mut<ui::ProgressBar>();
	bar.max = static_cast<float>(run.shells_needed);
	bar.value = static_cast<float>(run.shells);
	moltLabel_.get_mut<ui::Label>().text = std::format("{}/{}", run.shells, run.shells_needed);
	if (moltChip_.has<ui::Label>()) {
		moltChip_.get_mut<ui::Label>().text = std::format("{}", run.molt_level);
	}

	const bool molting = run.phase == RunPhase::Molting;
	moltModal_.get_mut<ui::Modal>().open = molting;
	if (!molting) {
		return;
	}
	const auto& offer = world.get<MoltOffer>();
	for (int i = 0; i < kMoltChoices; ++i) {
		UpdateMoltCard(world, moltCards_[i], FindUpgrade(world, offer.ids[i]), offer.selected == i);
	}
	if (confirmButton_.has<ui::Button>()) {
		// Greyed out until a card is selected; ConfirmUpgrade ignores the click until then.
		const ui::Button& theme = world.get<ui::UIConfig>().button;
		const bool ready = offer.selected >= 0;
		auto& confirm = confirmButton_.get_mut<ui::Button>();
		confirm.normal = ready ? theme.normal : kDisabledButtonColor;
		confirm.hover = ready ? theme.hover : kDisabledButtonColor;
		confirm.pressed_color = ready ? theme.pressed_color : kDisabledButtonColor;
	}
}

void ArenaScene::UpdateMoltCard(
	const flecs::world& world,
	const MoltCard& card,
	const Upgrade* upgrade,
	const bool selected
) const {
	if (!card.card.has<ui::UIElement>() || !card.icon.has<ui::Panel>() || !card.title.has<ui::Label>()) {
		return;
	}
	card.card.get_mut<ui::UIElement>().visible = upgrade != nullptr;
	if (upgrade == nullptr) {
		return;
	}
	auto& button = card.card.get_mut<ui::Button>();
	button.normal = selected ? kSelectedCardColor : kCardStyle.normal;
	button.border_color = selected ? kCoralColor : kCardStyle.border_color;
	button.border_thickness = selected ? kSelectedBorder : kCardStyle.border_thickness;
	card.icon.get_mut<ui::Panel>().texture = upgrade->icon_texture;
	card.title.get_mut<ui::Label>().text = upgrade->name;
	card.description.get_mut<ui::Label>().text = upgrade->description;

	// Each line is centred on the card: "Label before", the arrow, "after", then the change, smaller and coloured
	// by whether it helps (a falling cooldown helps).
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const core::Rect rect = card.card.get<ui::UIRect>().rect;
	const std::vector<StatPreview> previews = PreviewUpgrade(world, *upgrade);
	for (int i = 0; i < kMaxStatLines; ++i) {
		const StatLine& line = card.stats[static_cast<size_t>(i)];
		if (!line.arrow.has<ui::UIElement>()) {
			continue;
		}
		auto& before = line.before.get_mut<ui::Label>();
		auto& after = line.after.get_mut<ui::Label>();
		auto& delta = line.delta.get_mut<ui::Label>();
		const bool shown = i < static_cast<int>(previews.size());
		line.arrow.get_mut<ui::UIElement>().visible = shown;
		if (!shown) {
			before.text.clear();
			after.text.clear();
			delta.text.clear();
			continue;
		}
		const StatPreview& p = previews[static_cast<size_t>(i)];
		const float change = p.after - p.before;
		before.text = std::format("{} {}{}", p.label, FormatStat(p.before), p.unit);
		after.text = std::format("{}{}", FormatStat(p.after), p.unit);
		delta.text = std::format("{}{}{}", change >= 0.0F ? "+" : "", FormatStat(change), p.unit);
		delta.color = (change >= 0.0F) != p.lower_is_better ? kGainColor : kLossColor;

		const float beforeWidth = platform->MeasureText(before.text, kStatFontSize);
		const float afterWidth = platform->MeasureText(after.text, kStatFontSize);
		const float deltaWidth = platform->MeasureText(delta.text, kDeltaFontSize);
		const float total = beforeWidth + kArrowWidth + afterWidth + deltaWidth + 4.0F * kStatGap;
		float x = rect.x + (rect.w - total) / 2.0F;
		const float y = rect.y + kStatsTop + static_cast<float>(i) * kStatLineHeight;
		const auto place = [&](const flecs::entity e, const float w, const float dy, const float h) {
			e.get_mut<ui::UIRect>().rect = {.x = x, .y = y + dy, .w = w, .h = h};
			x += w + kStatGap;
		};
		place(line.before, beforeWidth, 0.0F, kStatLineHeight);
		place(line.arrow, kArrowWidth, (kStatLineHeight - kArrowHeight) / 2.0F, kArrowHeight);
		place(line.after, afterWidth, 0.0F, kStatLineHeight);
		x += kStatGap;
		// Nudged down so the smaller text shares the line's baseline (each label centres its text in its rect).
		place(line.delta, deltaWidth, (kStatFontSize - kDeltaFontSize) / 2.0F, kStatLineHeight);
	}
}

void ArenaScene::SpawnBatch(const int enemyCount) const {
	const int count = std::min(constants::kHordeDebugBatch, constants::kHordeDebugMax - enemyCount);
	if (count > 0 && crabnado_) {
		ThrowBurst(crabnado_, count);
	}
}

void ArenaScene::ClearHorde(const flecs::world& world) {
	horde_.destruct();
	horde_ = world.entity().child_of(sceneRoot_);
	crabnado_.add<ThrowsInto>(horde_);
}

void ArenaScene::Unload(flecs::world& world) {
	// A lost run paused the scene; the next scene starts unpaused.
	world.remove<scene::Paused>();
	ClearArena(world);
	for (flecs::entity* root : {&sceneRoot_, &uiRoot_}) {
		if (*root) {
			root->destruct();
			*root = {};
		}
	}
	for (const std::string& path : textures_) {
		assets::Release(world, assets::AssetType::Texture, path);
	}
	textures_.clear();
	horde_ = {};
	crabnado_ = {};
	music_ = {};
	hud_ = {};
	resultsModal_ = {};
	resultsHeading_ = {};
	resultsStats_ = {};
	pauseModal_ = {};
	settingsModal_ = {};
	moltBar_ = {};
	moltLabel_ = {};
	moltChip_ = {};
	digInChip_ = {};
	weaponSlots_ = {};
	timeLabel_ = {};
	killsLabel_ = {};
	waveLabel_ = {};
	telegraph_ = {};
	bossLabel_ = {};
	bannerLabel_ = {};
	countdownLabel_ = {};
	moltModal_ = {};
	moltCards_ = {};
	confirmButton_ = {};
}

} // namespace crabnado
