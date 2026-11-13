#include "football_scene.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <spdlog/fmt/fmt.h>

#include "title_scene.hpp"

using namespace engine;
namespace fc = game::football::constants;

namespace game {

using football::Card;
using football::CardKind;
using football::CurrentSegment;
using football::Deck;
using football::FootballState;
using football::GameMode;
using football::Hand;
using football::HandModifiers;
using football::IsBoss;
using football::LastOutcome;
using football::ModifierDef;
using football::ModifierRegistry;
using football::PlayDef;
using football::PlayRegistry;
using football::RebuildUITag;
using football::SegmentDef;
using football::ShopState;
namespace phase = game::football::phase;

namespace {

constexpr float kHudHeight = 64.0F;
constexpr float kBarX = 22.0F;
constexpr float kBarWidth = 44.0F;
constexpr float kBarTopMargin = 96.0F;
constexpr float kBarBottomMargin = 60.0F;
constexpr float kContentLeft = 120.0F;
constexpr float kAnimDuration = 1.25F;

// Move the flow entity to a new phase: drop every phase tag, add the target. Fires that phase's
// OnAdd observer (its entry logic + a UI rebuild request).
template<class Tag>
void GoPhase(flecs::entity flow) {
	flow.remove<phase::PlaySelect>()
		.remove<phase::Resolve>()
		.remove<phase::SegmentEnd>()
		.remove<phase::Shop>()
		.remove<phase::Results>()
		.add<Tag>();
}

flecs::entity Label(
	const flecs::world& world,
	const core::Rect rect,
	const std::string& text,
	const float size,
	const core::Rgba color,
	const ui::TextAlign align = ui::TextAlign::Center,
	const bool wrap = false
) {
	return ui::CreateLabel(world, rect, text, size)
		.set<ui::Label>({.text = text, .font_size = size, .color = color, .align = align, .wrap = wrap});
}

flecs::entity Token(const flecs::world& world, const core::Rect rect, const core::Rgba color) {
	return world.entity().set<ui::UIRect>({rect}).set<ui::Panel>(
		{.color = color, .border_color = {20, 20, 20, 255}, .border_thickness = 2.0F, .roundness = 1.0F}
	);
}

std::string SegmentLabel(const flecs::world& world) {
	const auto& st = world.get<FootballState>();
	if (IsBoss(world)) {
		return "BOSS DRIVE";
	}
	int normal = 0;
	for (const auto& seg : world.get<GameMode>().segments) {
		if (!seg.boss) {
			++normal;
		}
	}
	return fmt::format("Segment {}/{}", st.segment_index + 1, normal);
}

std::string ActiveModsText(const HandModifiers& m) {
	std::vector<std::string> parts;
	if (m.bonus_yards != 0) {
		parts.push_back(fmt::format("+{} yds", m.bonus_yards));
	}
	if (m.turnover_delta > 0.0F) {
		parts.push_back(fmt::format("-{:.0f}% TO", m.turnover_delta * 100.0F));
	}
	if (m.flag_delta > 0.0F) {
		parts.push_back(fmt::format("-{:.0f}% flag", m.flag_delta * 100.0F));
	}
	if (m.extra_turnover > 0.0F) {
		parts.push_back(fmt::format("+{:.0f}% TO risk", m.extra_turnover * 100.0F));
	}
	if (m.no_turnover) {
		parts.emplace_back("no turnover");
	}
	if (m.no_flag) {
		parts.emplace_back("no flag");
	}
	std::string out;
	for (std::size_t i = 0; i < parts.size(); ++i) {
		out += (i ? ", " : "") + parts[i];
	}
	return out;
}

float EaseOut(const float t) { return 1.0F - (1.0F - t) * (1.0F - t); }

struct FieldGeom {
	float cx{0}, x0{0}, x1{0}, top{0}, bottom{0}, qbx{0}, qby{0};
};
FieldGeom FieldOf(const float w, const float h) {
	FieldGeom g;
	g.cx = (kContentLeft + w) / 2.0F;
	constexpr float half = 300.0F;
	g.x0 = g.cx - half;
	g.x1 = g.cx + half;
	g.top = h * 0.17F;    // downfield / goal end
	g.bottom = h * 0.66F; // line of scrimmage
	g.qbx = g.cx;
	g.qby = g.bottom - 16.0F;
	return g;
}

// One selectable slot in the hand row: a drawn card, or an always-available basic play.
struct Slot {
	CardKind kind{CardKind::Play};
	std::string id;
	bool basic{false};
};

} // namespace

flecs::entity FootballScene::Btn(
	const flecs::world& world,
	const core::Rect rect,
	std::string label,
	std::function<void(flecs::entity)> on_click
) const {
	return ui::CreateButton(world, rect, std::move(label), std::move(on_click), texButton_);
}

void FootballScene::InitializePipeline(const flecs::world& world) {
	pipeline_ = ecs::FramePipeline(world).without<scene::MenuScene>().build();
	fixedStepPipeline_ = ecs::FixedStepPipeline(world).without<scene::MenuScene>().build();
}

void FootballScene::Load(flecs::world& world) {
	auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());

	football::LoadContent(world);
	football::InitRun(world);

	texField_ = platform->LoadTexture(assets::ResolveAsset(world, "textures/football/field.png"));
	texPanel_ = platform->LoadTexture(assets::ResolveAsset(world, "textures/football/panel.png"));
	texButton_ = platform->LoadTexture(assets::ResolveAsset(world, "textures/football/button.png"));
	texBoard_ = platform->LoadTexture(assets::ResolveAsset(world, "textures/football/scoreboard.png"));
	texMenu_ = platform->LoadTexture(assets::ResolveAsset(world, "textures/football/menu_bg.png"));

	uiRoot_ = world.entity("FootballUI");
	uiRoot_.set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = w, .h = h}});
	std::ignore = ui::CreatePanel(
					  world,
					  {.x = 0.0F, .y = 0.0F, .w = w, .h = h},
					  {.color = {255, 255, 255, 255}, .border_thickness = 0.0F, .texture = texMenu_}
	)
					  .child_of(uiRoot_);

	resultRevealed_ = false;
	deckOpen_ = false;
	phaseTime_ = 0.0F;
	contentRoot_ = flecs::entity{};

	systemsRoot_ = world.entity("FootballSystems");
	flowEntity_ = world.entity("FootballFlow");
	RegisterFlowSystems(world);

	// Enter the first phase; its OnAdd observer draws a hand and requests the first UI build.
	flowEntity_.add<phase::PlaySelect>();
}

void FootballScene::Unload(flecs::world& world) {
	if (uiRoot_) {
		uiRoot_.destruct();
	}
	if (systemsRoot_) {
		systemsRoot_.destruct();
	}
	if (flowEntity_) {
		flowEntity_.destruct();
	}
	uiRoot_ = contentRoot_ = systemsRoot_ = flowEntity_ = flecs::entity{};
	animBall_ = animReceiver_ = animDefender_ = flecs::entity{};
}

void FootballScene::RegisterFlowSystems(const flecs::world& world) {
	// Phase-entry observers: run the phase's entry logic and request a UI rebuild.
	std::ignore = world.observer("FootballEnterPlaySelect")
					  .with<phase::PlaySelect>()
					  .event(flecs::OnAdd)
					  .each([this](const flecs::entity e) {
						  OnEnterPlaySelect(e.world());
						  flowEntity_.add<RebuildUITag>();
					  })
					  .child_of(systemsRoot_);
	std::ignore = world.observer("FootballEnterResolve")
					  .with<phase::Resolve>()
					  .event(flecs::OnAdd)
					  .each([this](const flecs::entity e) {
						  OnEnterResolve(e.world());
						  flowEntity_.add<RebuildUITag>();
					  })
					  .child_of(systemsRoot_);
	std::ignore = world.observer("FootballEnterSegmentEnd")
					  .with<phase::SegmentEnd>()
					  .event(flecs::OnAdd)
					  .each([this](const flecs::entity e) {
						  OnEnterSegmentEnd(e.world());
						  flowEntity_.add<RebuildUITag>();
					  })
					  .child_of(systemsRoot_);
	std::ignore = world.observer("FootballEnterShop")
					  .with<phase::Shop>()
					  .event(flecs::OnAdd)
					  .each([this](const flecs::entity e) {
						  OnEnterShop(e.world());
						  flowEntity_.add<RebuildUITag>();
					  })
					  .child_of(systemsRoot_);
	std::ignore = world.observer("FootballEnterResults")
					  .with<phase::Results>()
					  .event(flecs::OnAdd)
					  .each([this](const flecs::entity e) {
						  const auto world2 = e.world();
						  newHighScore_ = football::RecordScore(world2, world2.get<FootballState>().total_points);
						  flowEntity_.add<RebuildUITag>();
					  })
					  .child_of(systemsRoot_);

	// Rebuild the current phase's UI whenever RebuildUITag is present (entry + intra-phase changes).
	std::ignore = world.system("FootballRebuildUI")
					  .with<RebuildUITag>()
					  .kind(flecs::OnUpdate)
					  .run([this](flecs::iter& it) {
						  while (it.next()) {
							  RebuildCurrentPhase(it.world());
							  flowEntity_.remove<RebuildUITag>();
						  }
					  })
					  .child_of(systemsRoot_);

	// Drive the pick animation each frame while in the Resolve phase; reveal the result when done.
	std::ignore = world.system("FootballResolveAnim")
					  .with<phase::Resolve>()
					  .kind(flecs::OnUpdate)
					  .run([this](flecs::iter& it) {
						  while (it.next()) {
							  phaseTime_ += it.delta_time();
							  AnimateResolve(it.world(), std::clamp(phaseTime_ / kAnimDuration, 0.0F, 1.0F));
							  if (!resultRevealed_ && phaseTime_ >= kAnimDuration) {
								  resultRevealed_ = true;
								  flowEntity_.add<RebuildUITag>();
							  }
						  }
					  })
					  .child_of(systemsRoot_);
}

void FootballScene::OnEnterPlaySelect(const flecs::world& world) {
	deckOpen_ = false;
	football::DrawHand(world); // persistent whole-game draw pile; resets HandModifiers + selection
}

void FootballScene::OnEnterResolve(const flecs::world& world) {
	phaseTime_ = 0.0F;
	resultRevealed_ = false;
	auto& st = world.get_mut<FootballState>();
	auto& outcome = world.get_mut<LastOutcome>();
	outcome = football::ResolvePlay(world, st.selected_play);
	st.yards += outcome.yards;
	if (st.yards < 0) {
		st.yards = 0;
	}
}

void FootballScene::OnEnterSegmentEnd(const flecs::world& world) {
	auto& st = world.get_mut<FootballState>();
	const auto& outcome = world.get<LastOutcome>();
	const SegmentDef seg = CurrentSegment(world);
	const int capped_yards = std::min(st.yards, seg.target + fc::kOverageCap);
	st.coins += std::max(0, capped_yards);

	if (!seg.boss) {
		st.last_converted = !outcome.turnover && st.yards >= seg.target;
		if (st.last_converted) {
			st.segments_converted += 1;
			st.total_points += std::min(st.yards - seg.target, fc::kOverageCap);
		}
	}
	else {
		const bool touchdown = !outcome.turnover && st.yards >= seg.target;
		const bool field_goal = !outcome.turnover && !touchdown && st.yards >= seg.fg_yards;
		st.boss_touchdown = touchdown;
		st.boss_field_goal = field_goal;
		st.total_points += touchdown ? fc::kTouchdownPoints + std::min(st.yards - seg.target, fc::kOverageCap)
									 : (field_goal ? fc::kFieldGoalPoints : 0);
	}
}

void FootballScene::OnEnterShop(const flecs::world& world) { football::GenerateShopOffers(world); }

void FootballScene::RebuildCurrentPhase(const flecs::world& world) {
	if (contentRoot_) {
		contentRoot_.destruct();
	}
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());

	contentRoot_ = world.entity().child_of(uiRoot_);
	contentRoot_.set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = w, .h = h}});
	animBall_ = animReceiver_ = animDefender_ = flecs::entity{};

	BuildHud(world, contentRoot_);
	BuildDistanceBar(world, contentRoot_);

	if (flowEntity_.has<phase::PlaySelect>()) {
		BuildHand(world, contentRoot_);
	}
	else if (flowEntity_.has<phase::Resolve>()) {
		BuildResult(world, contentRoot_);
	}
	else if (flowEntity_.has<phase::SegmentEnd>()) {
		BuildSegmentEnd(world, contentRoot_);
	}
	else if (flowEntity_.has<phase::Shop>()) {
		BuildShop(world, contentRoot_);
	}
	else if (flowEntity_.has<phase::Results>()) {
		BuildResults(world, contentRoot_);
	}
}

void FootballScene::AnimateResolve(const flecs::world& world, const float raw_t) const {
	if (!animBall_ || !animBall_.has<ui::UIRect>()) {
		return;
	}
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());
	const auto& outcome = world.get<LastOutcome>();
	const FieldGeom g = FieldOf(w, h);

	const float t = EaseOut(raw_t);
	const float travel = g.bottom - g.top;
	const float gained = std::clamp(static_cast<float>(outcome.yards) / 30.0F, -0.15F, 1.0F);
	const float end_y = g.bottom - gained * travel;
	const bool success = !outcome.turnover && !outcome.flagged && outcome.yards > 0;

	auto place = [](flecs::entity e, const float x, const float y, const float s) {
		if (e && e.has<ui::UIRect>()) {
			e.get_mut<ui::UIRect>().rect = {.x = x - s * 0.5F, .y = y - s * 0.5F, .w = s, .h = s};
		}
	};

	if (outcome.category == "pass" || outcome.category == "trick") {
		const float lateral = outcome.category == "trick" ? 70.0F : 120.0F;
		const float catch_x = g.cx + lateral;
		// Ball thrown from the QB up to the catch point.
		place(animBall_, g.qbx + (catch_x - g.qbx) * t, g.qby + (end_y - g.qby) * t, 20.0F);
		// Receiver runs up to the catch point from the line of scrimmage.
		place(animReceiver_, catch_x, g.qby + (end_y - g.qby) * t, 26.0F);
		// Defender closes from the top: undercuts on a turnover (INT), else arrives late/off.
		const float def_x = success ? catch_x + 46.0F : catch_x;
		const float def_end_y = outcome.turnover ? end_y : end_y - 26.0F;
		place(animDefender_, def_x, g.top + (def_end_y - g.top) * t, 26.0F);
	}
	else {
		// Run: the carrier drives straight upfield; a defender closes from the top for the tackle.
		place(animBall_, g.qbx, g.qby + (end_y - g.qby) * t, 22.0F);
		place(animReceiver_, g.cx - 34.0F, g.qby + ((end_y + 26.0F) - g.qby) * t, 24.0F); // lead blocker
		place(animDefender_, g.cx + 16.0F, g.top + (end_y - g.top) * t, 26.0F);           // tackler from top
	}
}

void FootballScene::BuildHud(const flecs::world& world, flecs::entity parent) const {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto& st = world.get<FootballState>();

	std::ignore = ui::CreatePanel(
					  world,
					  {.x = 0.0F, .y = 0.0F, .w = w, .h = kHudHeight},
					  {.color = {255, 255, 255, 255}, .border_color = core::colors::Border, .texture = texBoard_}
	)
					  .child_of(parent);
	std::ignore = Label(
					  world,
					  {.x = 20.0F, .y = 10.0F, .w = 320.0F, .h = 44.0F},
					  SegmentLabel(world),
					  26.0F,
					  core::colors::Title,
					  ui::TextAlign::Left
	)
					  .child_of(parent);

	const SegmentDef seg = CurrentSegment(world);
	const std::string center =
		seg.boss ? fmt::format(
					   "Down {}/{}   {} yds to the end zone",
					   st.down,
					   fc::kDownsPerSegment,
					   std::max(0, seg.target - st.yards)
				   )
				 : fmt::format("Down {}/{}   {} of {} yds", st.down, fc::kDownsPerSegment, st.yards, seg.target);
	std::ignore =
		Label(world, {.x = w / 2.0F - 250.0F, .y = 18.0F, .w = 500.0F, .h = 30.0F}, center, 22.0F, core::colors::Text)
			.child_of(parent);
	std::ignore = Label(
					  world,
					  {.x = w - 324.0F, .y = 18.0F, .w = 300.0F, .h = 30.0F},
					  fmt::format("Score {}    Coins {}", st.total_points, st.coins),
					  22.0F,
					  core::colors::Boost,
					  ui::TextAlign::Right
	)
					  .child_of(parent);
}

void FootballScene::BuildDistanceBar(const flecs::world& world, flecs::entity parent) const {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto h = static_cast<float>(platform->Height());
	const auto& st = world.get<FootballState>();
	const auto& segments = world.get<GameMode>().segments;
	const auto count = static_cast<int>(segments.size());
	if (count == 0) {
		return;
	}

	const float top = kBarTopMargin;
	const float bottom = h - kBarBottomMargin;
	const float bar_h = bottom - top;
	const float seg_h = bar_h / static_cast<float>(count);

	std::ignore = Label(
					  world,
					  {.x = kBarX - 6.0F, .y = top - 26.0F, .w = kBarWidth + 12.0F, .h = 20.0F},
					  "GOAL",
					  14.0F,
					  core::colors::Title
	)
					  .child_of(parent);

	for (int i = 0; i < count; ++i) {
		const float band_bottom = bottom - static_cast<float>(i) * seg_h;
		const float band_top = band_bottom - seg_h;
		const bool completed = i < st.segment_index;
		const bool current = i == st.segment_index;

		core::Rgba track =
			segments[static_cast<std::size_t>(i)].boss ? core::Rgba{80, 60, 30, 255} : core::Rgba{30, 40, 30, 255};
		if (completed) {
			track = core::colors::Accent;
		}
		std::ignore = ui::CreatePanel(
						  world,
						  {.x = kBarX, .y = band_top, .w = kBarWidth, .h = seg_h - 2.0F},
						  {.color = track, .border_color = core::colors::Border, .border_thickness = 2.0F}
		)
						  .child_of(parent);

		if (current) {
			const int target = segments[static_cast<std::size_t>(i)].target;
			const float frac =
				target > 0 ? std::clamp(static_cast<float>(st.yards) / static_cast<float>(target), 0.0F, 1.0F) : 0.0F;
			const float fill_h = frac * (seg_h - 2.0F);
			if (fill_h > 1.0F) {
				std::ignore = ui::CreatePanel(
								  world,
								  {.x = kBarX, .y = band_bottom - fill_h, .w = kBarWidth, .h = fill_h},
								  {.color = core::colors::Boost, .border_thickness = 0.0F}
				)
								  .child_of(parent);
			}
		}

		const std::string tag = segments[static_cast<std::size_t>(i)].boss ? "B" : std::to_string(i + 1);
		std::ignore = Label(
						  world,
						  {.x = kBarX, .y = band_top + seg_h * 0.5F - 12.0F, .w = kBarWidth, .h = 22.0F},
						  tag,
						  18.0F,
						  core::colors::Text
		)
						  .child_of(parent);
	}

	const SegmentDef seg = CurrentSegment(world);
	const float cur_frac =
		seg.target > 0 ? std::clamp(static_cast<float>(st.yards) / static_cast<float>(seg.target), 0.0F, 1.0F) : 0.0F;
	const float progressed = (static_cast<float>(st.segment_index) + cur_frac) * seg_h;
	const float token_y = std::clamp(bottom - progressed, top, bottom);
	std::ignore = Token(
					  world,
					  {.x = kBarX + kBarWidth + 6.0F, .y = token_y - 11.0F, .w = 22.0F, .h = 22.0F},
					  core::colors::Pollinator
	)
					  .child_of(parent);
}

void FootballScene::BuildHand(const flecs::world& world, flecs::entity parent) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());
	const auto& plays = world.get<PlayRegistry>();
	const auto& mods = world.get<ModifierRegistry>();
	const auto& hand = world.get<Hand>();
	const auto& st = world.get<FootballState>();
	const auto& handmods = world.get<HandModifiers>();
	const auto flow = flowEntity_;

	std::ignore = Label(
					  world,
					  {.x = kContentLeft, .y = 76.0F, .w = w - kContentLeft - 200.0F, .h = 28.0F},
					  "Your hand - play modifiers, then Hike a play",
					  22.0F,
					  core::colors::Title,
					  ui::TextAlign::Left
	)
					  .child_of(parent);

	std::ignore = Btn(
					  world,
					  {.x = w - 190.0F, .y = 74.0F, .w = 160.0F, .h = 34.0F},
					  fmt::format("View Deck ({})", world.get<Deck>().cards.size()),
					  [this, flow](const flecs::entity) {
						  deckOpen_ = true;
						  flow.add<RebuildUITag>();
					  }
	).child_of(parent);

	if (const std::string active = ActiveModsText(handmods); !active.empty()) {
		std::ignore = Label(
						  world,
						  {.x = kContentLeft, .y = 110.0F, .w = w - kContentLeft - 20.0F, .h = 22.0F},
						  "Active this down: " + active,
						  17.0F,
						  core::colors::Boost,
						  ui::TextAlign::Left
		)
						  .child_of(parent);
	}

	// Display slots: the drawn hand followed by the two always-available basic plays.
	std::vector<Slot> slots;
	for (const auto& card : hand.cards) {
		slots.push_back({.kind = card.kind, .id = card.id, .basic = false});
	}
	slots.push_back({.kind = CardKind::Play, .id = std::string{fc::kBasicRun}, .basic = true});
	slots.push_back({.kind = CardKind::Play, .id = std::string{fc::kBasicPass}, .basic = true});

	const auto n = static_cast<int>(slots.size());
	constexpr float card_w = 200.0F;
	constexpr float card_h = 210.0F;
	constexpr float gap = 20.0F;
	const float total = static_cast<float>(n) * card_w + static_cast<float>(std::max(0, n - 1)) * gap;
	const float x0 = std::max(kContentLeft, kContentLeft + (w - kContentLeft - total) / 2.0F);
	const float y = h * 0.28F;

	for (int k = 0; k < n; ++k) {
		const Slot& slot = slots[static_cast<std::size_t>(k)];
		const float x = x0 + static_cast<float>(k) * (card_w + gap);

		if (slot.kind == CardKind::Play) {
			const PlayDef* def = plays.Get(slot.id);
			const std::string name = def ? def->name : slot.id;
			const football::PlayStats stats = def ? football::EffectivePlayStats(world, *def) : football::PlayStats{};
			const bool selected = st.selected_hand_index == k;

			std::ignore = ui::CreatePanel(
							  world,
							  {.x = x, .y = y, .w = card_w, .h = card_h},
							  {.color = slot.basic ? core::Rgba{225, 225, 235, 255} : core::Rgba{255, 255, 255, 255},
							   .border_color = selected ? core::colors::Boost : core::colors::Border,
							   .border_thickness = selected ? 5.0F : 2.0F,
							   .roundness = 0.08F,
							   .texture = texPanel_}
			)
							  .child_of(parent);
			std::ignore =
				Label(world, {.x = x, .y = y + 12.0F, .w = card_w, .h = 26.0F}, name, 21.0F, core::colors::Title)
					.child_of(parent);
			std::ignore = Label(
							  world,
							  {.x = x, .y = y + 42.0F, .w = card_w, .h = 20.0F},
							  slot.basic ? "basic (always available)" : (def ? def->category : ""),
							  14.0F,
							  core::colors::Subtle
			)
							  .child_of(parent);
			std::ignore = Label(
							  world,
							  {.x = x, .y = y + 76.0F, .w = card_w, .h = 26.0F},
							  fmt::format("~{:.1f} yds", stats.avg_yards),
							  22.0F,
							  core::colors::Text
			)
							  .child_of(parent);
			std::string risk = fmt::format("TO {:.0f}%", stats.turnover * 100.0F);
			if (stats.flag > 0.0F) {
				risk += fmt::format("   FLAG {:.0f}%", stats.flag * 100.0F);
			}
			std::ignore =
				Label(
					world,
					{.x = x, .y = y + 104.0F, .w = card_w, .h = 20.0F},
					risk,
					14.0F,
					(stats.turnover >= 0.1F || stats.flag >= 0.1F) ? core::colors::Hazard : core::colors::Subtle
				)
					.child_of(parent);

			const int index = k;
			const std::string play_id = slot.id;
			std::ignore = Btn(
							  world,
							  {.x = x + 22.0F, .y = y + card_h - 44.0F, .w = card_w - 44.0F, .h = 34.0F},
							  selected ? "Selected" : "Select",
							  [flow, index, play_id](const flecs::entity e) {
								  auto& state = e.world().get_mut<FootballState>();
								  state.selected_hand_index = index;
								  state.selected_play = play_id;
								  flow.add<RebuildUITag>();
							  }
			).child_of(parent);
		}
		else {
			const ModifierDef* def = mods.Get(slot.id);
			const std::string name = def ? def->name : slot.id;
			std::string cat = def ? def->category : "modifier";
			std::transform(cat.begin(), cat.end(), cat.begin(), [](unsigned char c) { return std::toupper(c); });

			std::ignore = ui::CreatePanel(
							  world,
							  {.x = x, .y = y, .w = card_w, .h = card_h},
							  {.color = core::Rgba{150, 120, 185, 255}, .roundness = 0.08F, .texture = texPanel_}
			)
							  .child_of(parent);
			std::ignore = Label(
							  world,
							  {.x = x, .y = y + 10.0F, .w = card_w, .h = 20.0F},
							  cat,
							  14.0F,
							  core::Rgba{240, 235, 255, 255}
			)
							  .child_of(parent);
			std::ignore =
				Label(world, {.x = x, .y = y + 34.0F, .w = card_w, .h = 26.0F}, name, 21.0F, core::colors::Title)
					.child_of(parent);
			std::ignore = Label(
							  world,
							  {.x = x + 12.0F, .y = y + 72.0F, .w = card_w - 24.0F, .h = 78.0F},
							  def ? def->description : "",
							  15.0F,
							  core::colors::Text,
							  ui::TextAlign::Left,
							  true
			)
							  .child_of(parent);

			const int index = k;
			std::ignore = Btn(
							  world,
							  {.x = x + 22.0F, .y = y + card_h - 44.0F, .w = card_w - 44.0F, .h = 34.0F},
							  "Use",
							  [flow, index](const flecs::entity e) {
								  football::ApplyModifier(e.world(), index);
								  flow.add<RebuildUITag>();
							  }
			).child_of(parent);
		}
	}

	if (st.selected_hand_index >= 0) {
		std::ignore = Btn(
						  world,
						  {.x = w / 2.0F - 130.0F, .y = h - 92.0F, .w = 260.0F, .h = 56.0F},
						  "HIKE!",
						  [flow](const flecs::entity) { GoPhase<phase::Resolve>(flow); }
		).child_of(parent);
	}
	else {
		std::ignore = Label(
						  world,
						  {.x = kContentLeft, .y = h - 80.0F, .w = w - kContentLeft, .h = 24.0F},
						  "Select a play to reveal the Hike button",
						  16.0F,
						  core::colors::Subtle
		)
						  .child_of(parent);
	}

	if (deckOpen_) {
		BuildDeckOverlay(world, parent);
	}
}

void FootballScene::BuildDeckOverlay(const flecs::world& world, flecs::entity parent) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());
	const auto& deck = world.get<Deck>();
	const auto& plays = world.get<PlayRegistry>();
	const auto& mods = world.get<ModifierRegistry>();
	const auto flow = flowEntity_;

	const float mw = 560.0F;
	const float mh = 480.0F;
	const float mx = (w - mw) / 2.0F;
	const float my = (h - mh) / 2.0F;
	const auto modal = ui::CreateModal(world, {.x = mx, .y = my, .w = mw, .h = mh}, {.open = true}).child_of(parent);

	std::ignore = Label(
					  world,
					  {.x = mx, .y = my + 16.0F, .w = mw, .h = 30.0F},
					  fmt::format("Your Deck ({} cards)", deck.cards.size()),
					  26.0F,
					  core::colors::Title
	)
					  .child_of(modal);

	std::vector<std::pair<std::string, int>> rows;
	std::unordered_map<std::string, int> index_of;
	for (const auto& card : deck.cards) {
		const std::string key = (card.kind == CardKind::Play ? "P:" : "M:") + card.id;
		if (const auto it = index_of.find(key); it != index_of.end()) {
			rows[static_cast<std::size_t>(it->second)].second += 1;
			continue;
		}
		std::string label;
		if (card.kind == CardKind::Play) {
			const PlayDef* def = plays.Get(card.id);
			label = fmt::format("{}  (play)", def ? def->name : card.id);
		}
		else {
			const ModifierDef* def = mods.Get(card.id);
			label = fmt::format("{}  ({})", def ? def->name : card.id, def ? def->category : "modifier");
		}
		index_of[key] = static_cast<int>(rows.size());
		rows.emplace_back(label, 1);
	}

	float ly = my + 60.0F;
	for (const auto& [label, count] : rows) {
		std::ignore = Label(
						  world,
						  {.x = mx + 30.0F, .y = ly, .w = mw - 60.0F, .h = 24.0F},
						  fmt::format("{}  x{}", label, count),
						  18.0F,
						  core::colors::Text,
						  ui::TextAlign::Left
		)
						  .child_of(modal);
		ly += 27.0F;
	}

	std::ignore = Btn(
					  world,
					  {.x = mx + mw / 2.0F - 90.0F, .y = my + mh - 56.0F, .w = 180.0F, .h = 44.0F},
					  "Close",
					  [this, flow](const flecs::entity) {
						  deckOpen_ = false;
						  flow.add<RebuildUITag>();
					  }
	).child_of(modal);
}

void FootballScene::BuildResult(const flecs::world& world, flecs::entity parent) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());
	const auto& outcome = world.get<LastOutcome>();
	const FieldGeom g = FieldOf(w, h);
	const auto flow = flowEntity_;

	std::ignore = ui::CreatePanel(
					  world,
					  {.x = g.x0, .y = g.top, .w = g.x1 - g.x0, .h = g.bottom - g.top},
					  {.color = {255, 255, 255, 255},
					   .border_color = {210, 225, 210, 255},
					   .border_thickness = 3.0F,
					   .texture = texField_}
	)
					  .child_of(parent);

	for (int i = 1; i <= 5; ++i) {
		const float ty = g.top + static_cast<float>(i) * (g.bottom - g.top) / 6.0F;
		std::ignore = ui::CreatePanel(
						  world,
						  {.x = g.x0 + 6.0F, .y = ty, .w = 18.0F, .h = 3.0F},
						  {.color = {235, 235, 235, 220}, .border_thickness = 0.0F}
		)
						  .child_of(parent);
		std::ignore = ui::CreatePanel(
						  world,
						  {.x = g.x1 - 24.0F, .y = ty, .w = 18.0F, .h = 3.0F},
						  {.color = {235, 235, 235, 220}, .border_thickness = 0.0F}
		)
						  .child_of(parent);
	}

	std::ignore = Label(
					  world,
					  {.x = g.x0, .y = g.top + 8.0F, .w = g.x1 - g.x0, .h = 24.0F},
					  outcome.play_name,
					  20.0F,
					  core::colors::Text
	)
					  .child_of(parent);

	std::ignore =
		Token(world, {.x = g.qbx - 9.0F, .y = g.qby - 9.0F, .w = 18.0F, .h = 18.0F}, core::Rgba{80, 120, 220, 255})
			.child_of(parent);

	const bool is_run = outcome.category != "pass" && outcome.category != "trick";
	const core::Rgba recv_color = is_run ? core::Rgba{210, 80, 80, 255} : core::colors::Accent;
	animBall_ =
		Token(world, {.x = g.qbx - 10.0F, .y = g.qby - 10.0F, .w = 20.0F, .h = 20.0F}, core::Rgba{150, 90, 40, 255})
			.child_of(parent);
	animReceiver_ =
		Token(world, {.x = g.cx - 13.0F, .y = g.qby - 13.0F, .w = 26.0F, .h = 26.0F}, recv_color).child_of(parent);
	animDefender_ = Token(world, {.x = g.cx + 30.0F, .y = g.top, .w = 26.0F, .h = 26.0F}, core::Rgba{210, 80, 80, 255})
						.child_of(parent);

	if (!resultRevealed_) {
		std::ignore = Label(
						  world,
						  {.x = 0.0F, .y = g.bottom + 12.0F, .w = w, .h = 28.0F},
						  "The play develops...",
						  20.0F,
						  core::colors::Subtle
		)
						  .child_of(parent);
		return;
	}

	std::string headline;
	core::Rgba color = core::colors::Title;
	if (outcome.turnover) {
		headline = "TURNOVER!";
		color = core::colors::Hazard;
	}
	else if (outcome.flagged) {
		headline = fmt::format("PENALTY FLAG!  {} yds", outcome.yards);
		color = core::colors::Hazard;
	}
	else {
		headline =
			outcome.yards >= 0 ? fmt::format("+{} YARDS!", outcome.yards) : fmt::format("{} YARDS", outcome.yards);
	}
	std::ignore =
		Label(world, {.x = 0.0F, .y = g.bottom + 10.0F, .w = w, .h = 56.0F}, headline, 48.0F, color).child_of(parent);

	std::ignore = Btn(
					  world,
					  {.x = w / 2.0F - 110.0F, .y = h - 84.0F, .w = 220.0F, .h = 50.0F},
					  "Continue",
					  [flow](const flecs::entity e) {
						  auto world2 = e.world();
						  auto& st = world2.get_mut<FootballState>();
						  const auto& out = world2.get<LastOutcome>();
						  const SegmentDef seg = CurrentSegment(world2);
						  const bool converted = !seg.boss && st.yards >= seg.target;
						  const bool touchdown = seg.boss && st.yards >= seg.target;
						  const bool last_down = st.down >= fc::kDownsPerSegment;
						  if (out.turnover || converted || touchdown || last_down) {
							  GoPhase<phase::SegmentEnd>(flow);
						  }
						  else {
							  st.down += 1;
							  GoPhase<phase::PlaySelect>(flow);
						  }
					  }
	).child_of(parent);
}

void FootballScene::BuildSegmentEnd(const flecs::world& world, flecs::entity parent) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());
	const auto& st = world.get<FootballState>();
	const SegmentDef seg = CurrentSegment(world);
	const auto flow = flowEntity_;

	std::string headline;
	std::string detail;
	core::Rgba color = core::colors::Title;
	if (seg.boss) {
		if (st.boss_touchdown) {
			headline = "TOUCHDOWN!";
			detail = fmt::format("+{} points", fc::kTouchdownPoints + std::min(st.yards - seg.target, fc::kOverageCap));
		}
		else if (st.boss_field_goal) {
			headline = "FIELD GOAL";
			detail = fmt::format("+{} points", fc::kFieldGoalPoints);
			color = core::colors::Boost;
		}
		else {
			headline = "DRIVE STALLED";
			detail = "No score";
			color = core::colors::Hazard;
		}
	}
	else if (st.last_converted) {
		headline = "FIRST DOWN!";
		detail = fmt::format("Bonus +{} points", std::min(st.yards - seg.target, fc::kOverageCap));
	}
	else {
		headline = "STOPPED SHORT";
		detail = "0 points this segment";
		color = core::colors::Hazard;
	}

	std::ignore =
		Label(world, {.x = 0.0F, .y = h / 2.0F - 80.0F, .w = w, .h = 60.0F}, headline, 56.0F, color).child_of(parent);
	std::ignore = Label(world, {.x = 0.0F, .y = h / 2.0F - 6.0F, .w = w, .h = 30.0F}, detail, 24.0F, core::colors::Text)
					  .child_of(parent);
	std::ignore = Label(
					  world,
					  {.x = 0.0F, .y = h / 2.0F + 28.0F, .w = w, .h = 24.0F},
					  fmt::format("Coins earned: {}", std::min(st.yards, seg.target + fc::kOverageCap)),
					  18.0F,
					  core::colors::Boost
	)
					  .child_of(parent);

	const std::string next_label = seg.boss ? "See Results" : "To the Shop";
	std::ignore = Btn(
					  world,
					  {.x = w / 2.0F - 120.0F, .y = h / 2.0F + 70.0F, .w = 240.0F, .h = 54.0F},
					  next_label,
					  [flow](const flecs::entity e) {
						  if (IsBoss(e.world())) {
							  GoPhase<phase::Results>(flow);
						  }
						  else {
							  GoPhase<phase::Shop>(flow);
						  }
					  }
	).child_of(parent);
}

void FootballScene::BuildShop(const flecs::world& world, flecs::entity parent) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());
	const auto& plays = world.get<PlayRegistry>();
	const auto& mods = world.get<ModifierRegistry>();
	const auto& shop = world.get<ShopState>();
	const auto& st = world.get<FootballState>();
	const auto flow = flowEntity_;

	std::ignore = Label(
					  world,
					  {.x = 0.0F, .y = 92.0F, .w = w, .h = 32.0F},
					  fmt::format("Shop - buy a card for your deck  (Coins {})", st.coins),
					  24.0F,
					  core::colors::Title
	)
					  .child_of(parent);

	constexpr float card_w = 240.0F;
	constexpr float card_h = 236.0F;
	constexpr float gap = 30.0F;
	const auto n = static_cast<float>(shop.offers.size());
	const float total = n * card_w + (n - 1.0F) * gap;
	float x = kContentLeft + (w - kContentLeft - total) / 2.0F;
	const float y = h / 2.0F - 98.0F;

	for (std::size_t i = 0; i < shop.offers.size(); ++i) {
		const auto& offer = shop.offers[i];
		const bool is_play = offer.card.kind == CardKind::Play;
		const PlayDef* pdef = is_play ? plays.Get(offer.card.id) : nullptr;
		const ModifierDef* mdef = is_play ? nullptr : mods.Get(offer.card.id);
		const std::string name = is_play ? (pdef ? pdef->name : offer.card.id) : (mdef ? mdef->name : offer.card.id);
		std::string kind = is_play ? std::string{"PLAY"} : (mdef ? mdef->category : std::string{"MODIFIER"});
		std::transform(kind.begin(), kind.end(), kind.begin(), [](unsigned char c) { return std::toupper(c); });
		const std::string sub = is_play ? (pdef ? fmt::format("~{:.1f} yds", football::PlayAverageYards(*pdef)) : "")
										: (mdef ? mdef->description : "");

		std::ignore = ui::CreatePanel(
						  world,
						  {.x = x, .y = y, .w = card_w, .h = card_h},
						  {.color = is_play ? core::Rgba{255, 255, 255, 255} : core::Rgba{150, 120, 185, 255},
						   .roundness = 0.08F,
						   .texture = texPanel_}
		)
						  .child_of(parent);
		std::ignore = Label(world, {.x = x, .y = y + 12.0F, .w = card_w, .h = 20.0F}, kind, 14.0F, core::colors::Subtle)
						  .child_of(parent);
		std::ignore = Label(world, {.x = x, .y = y + 38.0F, .w = card_w, .h = 28.0F}, name, 23.0F, core::colors::Title)
						  .child_of(parent);
		std::ignore = Label(
						  world,
						  {.x = x + 12.0F, .y = y + 76.0F, .w = card_w - 24.0F, .h = 56.0F},
						  sub,
						  16.0F,
						  core::colors::Text,
						  ui::TextAlign::Left,
						  true
		)
						  .child_of(parent);
		std::ignore = Label(
						  world,
						  {.x = x, .y = y + 140.0F, .w = card_w, .h = 26.0F},
						  fmt::format("Cost {} coins", offer.cost),
						  20.0F,
						  core::colors::Boost
		)
						  .child_of(parent);

		const core::Rect btn{.x = x + 28.0F, .y = y + card_h - 52.0F, .w = card_w - 56.0F, .h = 40.0F};
		if (offer.purchased) {
			std::ignore = Label(world, btn, "Owned", 22.0F, core::colors::Accent).child_of(parent);
		}
		else if (st.coins < offer.cost) {
			std::ignore = Label(world, btn, "Not enough coins", 16.0F, core::colors::Subtle).child_of(parent);
		}
		else {
			const std::size_t index = i;
			std::ignore = Btn(world, btn, "Buy", [flow, index](const flecs::entity e) {
							  auto world2 = e.world();
							  auto& state = world2.get_mut<FootballState>();
							  auto& shop2 = world2.get_mut<ShopState>();
							  if (index >= shop2.offers.size()) {
								  return;
							  }
							  auto& off = shop2.offers[index];
							  if (off.purchased || state.coins < off.cost) {
								  return;
							  }
							  state.coins -= off.cost;
							  football::AddToDeck(world2, off.card);
							  off.purchased = true;
							  flow.add<RebuildUITag>();
						  }).child_of(parent);
		}
		x += card_w + gap;
	}

	const auto& segments = world.get<GameMode>().segments;
	const int next_index = st.segment_index + 1;
	const bool next_boss = next_index >= 0
						   && next_index < static_cast<int>(segments.size())
						   && segments[static_cast<std::size_t>(next_index)].boss;
	std::ignore = Btn(
					  world,
					  {.x = w / 2.0F - 120.0F, .y = h - 92.0F, .w = 240.0F, .h = 54.0F},
					  next_boss ? "Face the Boss" : "Next Segment",
					  [flow](const flecs::entity e) {
						  auto world2 = e.world();
						  auto& state = world2.get_mut<FootballState>();
						  state.segment_index += 1;
						  state.down = 1;
						  state.yards = 0;
						  state.target = CurrentSegment(world2).target;
						  GoPhase<phase::PlaySelect>(flow);
					  }
	).child_of(parent);
}

void FootballScene::BuildResults(const flecs::world& world, flecs::entity parent) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());
	const auto& st = world.get<FootballState>();
	const int normal_segments = std::max(0, static_cast<int>(world.get<GameMode>().segments.size()) - 1);
	const auto flow = flowEntity_;

	std::ignore = Label(
					  world,
					  {.x = 0.0F, .y = h / 2.0F - 160.0F, .w = w, .h = 60.0F},
					  "FINAL WHISTLE",
					  52.0F,
					  core::colors::Title
	)
					  .child_of(parent);
	std::ignore = Label(
					  world,
					  {.x = 0.0F, .y = h / 2.0F - 84.0F, .w = w, .h = 48.0F},
					  fmt::format("Final Score: {}", st.total_points),
					  40.0F,
					  core::colors::Boost
	)
					  .child_of(parent);

	const std::string best_line = newHighScore_
									  ? "New High Score!"
									  : fmt::format("High Score: {}", world.get<football::HighScore>().best_points);
	std::ignore = Label(
					  world,
					  {.x = 0.0F, .y = h / 2.0F - 30.0F, .w = w, .h = 30.0F},
					  best_line,
					  26.0F,
					  newHighScore_ ? core::colors::Boost : core::colors::Text
	)
					  .child_of(parent);

	const std::string boss_line =
		st.boss_touchdown ? "Boss: Touchdown!" : (st.boss_field_goal ? "Boss: Field goal" : "Boss: no score");
	std::ignore =
		Label(
			world,
			{.x = 0.0F, .y = h / 2.0F + 10.0F, .w = w, .h = 28.0F},
			fmt::format("First downs converted: {}/{}    {}", st.segments_converted, normal_segments, boss_line),
			22.0F,
			core::colors::Text
		)
			.child_of(parent);

	std::ignore = Btn(
					  world,
					  {.x = w / 2.0F - 250.0F, .y = h / 2.0F + 64.0F, .w = 230.0F, .h = 54.0F},
					  "Play Again",
					  [flow](const flecs::entity e) {
						  football::InitRun(e.world());
						  GoPhase<phase::PlaySelect>(flow);
					  }
	).child_of(parent);
	std::ignore = Btn(
					  world,
					  {.x = w / 2.0F + 20.0F, .y = h / 2.0F + 64.0F, .w = 230.0F, .h = 54.0F},
					  "Quit to Title",
					  [](const flecs::entity e) { scene::ActivateScene<TitleSceneTag>(e.world()); }
	).child_of(parent);
}

} // namespace game
