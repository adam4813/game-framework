#include "hud.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <vector>

#include "engine/engine.hpp"
#include "hud_layout.hpp"
#include "palette.hpp"
#include "score_components.hpp"
#include "ticket_components.hpp"
#include "ticket_rules.hpp"

using namespace engine;

namespace signal_room {

namespace {

constexpr float kClockWidth = 220.0F;
constexpr float kClockHeight = 48.0F;
constexpr float kTagHeight = 24.0F;
constexpr float kScoreHeight = 38.0F;
constexpr float kChainHeight = 32.0F;
constexpr float kCallsignHeight = 26.0F;
constexpr float kTierHeight = 16.0F;
constexpr float kFrequencyWidth = 90.0F;
constexpr float kSwatchGap = 6.0F;
constexpr float kGaugeBorder = 1.0F;
constexpr float kExitFlashAlpha = 220.0F;

core::Rgba WithAlpha(core::Rgba colour, const float alpha) {
	colour.a = static_cast<std::uint8_t>(std::clamp(alpha, 0.0F, 255.0F));
	return colour;
}

core::Rect Rect(const float x, const float y, const float w, const float h) { return {.x = x, .y = y, .w = w, .h = h}; }

flecs::entity MakeLabel(
	const flecs::world& world,
	const flecs::entity parent,
	std::string text,
	const float size,
	const core::Rgba colour,
	const ui::TextAlign align = ui::TextAlign::Left
) {
	return world.entity()
		.child_of(parent)
		.set(ui::UIRect{})
		.set(ui::Label{.text = std::move(text), .font_size = size, .color = colour, .align = align});
}

// A Ticket card: panel, callsign, tier, frequency swatch and label, and the gauge (On Air bar plus the
// dim Stand-by segment). Positions are written by the per-frame update.
void BuildCard(
	const flecs::world& world, const flecs::entity roomRoot, const flecs::entity ticketEntity, const Ticket& ticket
) {
	const auto& hud = world.get<HudLayout>();
	const auto* tier = world.get<TicketRules>().tiers.Get(ticket.tier);
	const auto* frequency = world.get<FrequencyTable>().Find(ticket.frequency);
	const auto* swatchColour = frequency == nullptr ? nullptr : world.get<Palette>().Get(frequency->colour);

	auto card = world.entity().child_of(roomRoot);
	card.set(ui::UIRect{}).set(ui::Panel{.color = hud.panel, .border_color = hud.border, .border_thickness = 2.0F});
	const auto callsign = MakeLabel(world, card, ticket.callsign, hud.callsignFont, hud.text);
	const auto tierLabel =
		MakeLabel(world, card, tier == nullptr ? ticket.tier : tier->label, hud.tierFont, hud.textDim);
	const auto swatch = world.entity().child_of(card).set(ui::UIRect{}).set(ui::Panel{
		.color = swatchColour == nullptr ? hud.textDim : *swatchColour,
		.border_color = hud.border,
		.border_thickness = 1.0F,
	});
	const auto frequencyLabel = MakeLabel(world, card, ticket.frequency, hud.frequencyFont, hud.text);
	const auto gauge = world.entity()
						   .child_of(card)
						   .set(ui::UIRect{})
						   .set(ui::ProgressBar{
							   .value = 1.0F,
							   .fill_color = hud.text,
							   .track_color = hud.track,
							   .border_color = hud.border,
							   .border_thickness = kGaugeBorder,
							   .roundness = 0.0F,
						   })
						   .set(HudGauge{});
	const auto standbySegment = world.entity().child_of(card).set(ui::UIRect{}).set(ui::UIElement{.visible = false}).set(
		ui::Panel{.color = hud.standby, .border_color = hud.border, .border_thickness = kGaugeBorder}
	);
	card.set(HudCard{
		.ticket = ticketEntity.id(),
		.order = ticket.order,
		.callsign = callsign,
		.tier = tierLabel,
		.swatch = swatch,
		.frequency = frequencyLabel,
		.gauge = gauge,
		.standbySegment = standbySegment,
	});
}

std::string ClockText(const float windowLeft) {
	const int seconds = std::max(0, static_cast<int>(std::lround(windowLeft)));
	return std::format("{}:{:02}", seconds / 60, seconds % 60);
}

void SetText(const flecs::entity label, const std::string& text) {
	auto& ui = label.get_mut<ui::Label>();
	if (ui.text != text) {
		ui.text = text;
	}
}

// Scale pop: the font starts at popScale times its size and shrinks back over popSeconds.
void SetPop(const flecs::entity label, const float baseFont, const float left, const HudLayout& hud) {
	const float t = std::clamp(left / hud.popSeconds, 0.0F, 1.0F);
	label.get_mut<ui::Label>().font_size = baseFont * (1.0F + (hud.popScale - 1.0F) * t);
}

void UpdateGauge(
	const flecs::world& world,
	const HudLayout& hud,
	const HudCard& card,
	const Ticket& ticket,
	const float x,
	const float y
) {
	const auto& rules = world.get<TicketRules>();
	const auto* tier = rules.tiers.Get(ticket.tier);
	const bool standby = ticket.state == TicketState::StandBy;
	auto& gauge = card.gauge.get_mut<HudGauge>();
	gauge.standby = standby && tier != nullptr ? ticket.standbyLeft / tier->onAirTime : 0.0F;
	if (standby || ticket.onAirTotal <= 0.0F) {
		// On Stand-by the bar is full: it is the line the Ticket will start from.
		gauge.onAir = 1.0F;
		gauge.band = rules.bands.empty() ? std::string{} : rules.bands.front().id;
	} else {
		gauge.onAir = std::clamp(ticket.onAirLeft / ticket.onAirTotal, 0.0F, 1.0F);
		const auto* band = FindBand(rules, ticket);
		gauge.band = band == nullptr ? std::string{} : band->id;
	}

	// The bar spans 0-100%; the Stand-by segment continues past it up to the tier's largest overfill.
	const float width = hud.cardWidth - 2.0F * hud.cardPadding;
	const float maxOverfill = std::max(tier == nullptr ? 0.0F : tier->overfillMax, gauge.standby);
	const float barWidth = width / (1.0F + maxOverfill);
	card.gauge.get_mut<ui::UIRect>().rect = Rect(x, y, barWidth, hud.gaugeHeight);
	auto& bar = card.gauge.get_mut<ui::ProgressBar>();
	bar.value = gauge.onAir;
	const auto colour = hud.bands.find(gauge.band);
	bar.fill_color = colour == hud.bands.end() ? hud.text : colour->second;
	// A red On Air gauge flashes between its colour and the dark track.
	if (!standby && gauge.band == "red" && std::fmod(card.age * hud.redFlashHz, 1.0F) >= 0.5F) {
		bar.fill_color = hud.track;
	}
	card.standbySegment.get_mut<ui::UIElement>().visible = gauge.standby > 0.0F;
	card.standbySegment.get_mut<ui::UIRect>().rect = Rect(x + barWidth, y, gauge.standby * barWidth, hud.gaugeHeight);
}

void LayoutCard(
	const flecs::world& world, const HudLayout& hud, const flecs::entity cardEntity, const int slot, const float dt
) {
	auto& card = cardEntity.get_mut<HudCard>();
	card.age += dt;
	const float x = hud.margin + static_cast<float>(slot) * (hud.cardWidth + hud.cardSpacing);
	const float y = hud.margin;
	const float inner = hud.cardWidth - 2.0F * hud.cardPadding;
	const float left = x + hud.cardPadding;
	const float gaugeY = y + hud.cardHeight - hud.cardPadding - hud.gaugeHeight;
	const float frequencyY = y + hud.cardPadding + kCallsignHeight + kTierHeight + 2.0F;

	cardEntity.get_mut<ui::UIRect>().rect = Rect(x, y, hud.cardWidth, hud.cardHeight);
	card.callsign.get_mut<ui::UIRect>().rect = Rect(left, y + hud.cardPadding, inner, kCallsignHeight);
	card.tier.get_mut<ui::UIRect>().rect = Rect(left, y + hud.cardPadding + kCallsignHeight, inner, kTierHeight);
	card.swatch.get_mut<ui::UIRect>().rect = Rect(left, frequencyY + 2.0F, hud.swatchSize, hud.swatchSize);
	card.frequency.get_mut<ui::UIRect>().rect =
		Rect(left + hud.swatchSize + kSwatchGap, frequencyY, kFrequencyWidth, hud.swatchSize + 4.0F);

	auto& panel = cardEntity.get_mut<ui::Panel>();
	if (card.exiting) {
		const auto& flash = card.cleared ? hud.flashClear : hud.flashLost;
		panel.color = WithAlpha(flash, kExitFlashAlpha * card.exitLeft / std::max(hud.exitFlash, 0.001F));
		panel.border_color = flash;
		return;
	}
	panel.color = hud.panel;
	panel.border_color = hud.border;
	if (const auto ticketEntity = world.entity(card.ticket); ticketEntity.is_alive() && ticketEntity.has<Ticket>()) {
		UpdateGauge(world, hud, card, ticketEntity.get<Ticket>(), left, gaugeY);
	}
}

// The per-frame HUD: lays the cards out oldest first, runs their exit flashes and gauges, and refreshes
// the clock, score and chain labels.
void RegisterHudUpdate(const flecs::world& world, const flecs::entity roomRoot) {
	world.system("HudUpdate")
		.kind(flecs::OnUpdate)
		.run([cards = world.query<HudCard>(),
			  clock = world.query_builder<>().with<HudClock>().build(),
			  clockTag = world.query_builder<>().with<HudClockTag>().build(),
			  score = world.query_builder<>().with<HudScore>().build(),
			  chain = world.query_builder<>().with<HudChain>().build()](flecs::iter& it) {
			while (it.next()) {
				const auto world = it.world();
				const auto& hud = world.get<HudLayout>();
				const auto& rules = world.get<TicketRules>();
				const float dt = it.delta_time();

				std::vector<flecs::entity> rack;
				cards.each([&](const flecs::entity entity, const HudCard&) { rack.push_back(entity); });
				std::ranges::sort(rack, {}, [](const flecs::entity entity) { return entity.get<HudCard>().order; });

				// A card whose Ticket vanished without Clear or Lost (the hard stop) goes at once; the rest
				// leave when their flash ends, and the oldest leavers make room if the rack would show too many.
				std::vector<flecs::entity> shown;
				for (const auto entity : rack) {
					auto& card = entity.get_mut<HudCard>();
					if (card.exiting) {
						card.exitLeft -= dt;
					}
					const bool gone = card.exiting ? card.exitLeft <= 0.0F : !world.entity(card.ticket).is_alive();
					if (gone) {
						entity.destruct();
					} else {
						shown.push_back(entity);
					}
				}
				for (auto at = shown.begin(); static_cast<int>(shown.size()) > rules.rackCap && at != shown.end();) {
					if (at->get<HudCard>().exiting) {
						at->destruct();
						at = shown.erase(at);
					} else {
						++at;
					}
				}
				for (size_t slot = 0; slot < shown.size(); ++slot) {
					LayoutCard(world, hud, shown[slot], static_cast<int>(slot), dt);
				}

				const auto& window = world.get<ShiftWindow>();
				const auto& points = world.get<Score>();
				auto& pulse = world.get_mut<HudPulse>();
				pulse.score = std::max(0.0F, pulse.score - dt);
				pulse.chain = std::max(0.0F, pulse.chain - dt);
				const float screenWidth = static_cast<float>(world.get<platform::PlatformRef>().ptr->Width());
				const float right = screenWidth - hud.margin - kClockWidth;
				const float scoreY = hud.margin + kClockHeight + kTagHeight + 6.0F;
				const auto step = static_cast<size_t>(points.chainStep);
				const float multiplier = step < rules.handlerChain.size() ? rules.handlerChain[step] : 1.0F;

				clock.each([&](const flecs::entity entity) {
					entity.get_mut<ui::UIRect>().rect = Rect(right, hud.margin, kClockWidth, kClockHeight);
					SetText(entity, ClockText(window.windowLeft));
					entity.get_mut<ui::Label>().color = window.lastCall ? hud.lastCall : hud.text;
				});
				clockTag.each([&](const flecs::entity entity) {
					entity.get_mut<ui::UIRect>().rect = Rect(right, hud.margin + kClockHeight, kClockWidth, kTagHeight);
					SetText(entity, window.lastCall ? hud.lastCallLabel : std::string{});
				});
				score.each([&](const flecs::entity entity) {
					entity.get_mut<ui::UIRect>().rect = Rect(right, scoreY, kClockWidth, kScoreHeight);
					SetText(entity, std::to_string(points.score));
					SetPop(entity, hud.scoreFont, pulse.score, hud);
				});
				chain.each([&](const flecs::entity entity) {
					entity.get_mut<ui::UIRect>().rect = Rect(right, scoreY + kScoreHeight, kClockWidth, kChainHeight);
					SetText(entity, hud.chainPrefix + std::format("{:.2f}", multiplier));
					SetPop(entity, hud.chainFont, pulse.chain, hud);
				});
			}
		})
		.child_of(roomRoot);
}

void RegisterHudObservers(const flecs::world& world, const flecs::entity roomRoot) {
	world.observer<const Ticket>("HudTicketArrived")
		.event<TicketArrived>()
		.each([roomRoot, cards = world.query<const HudCard>()](flecs::iter& it, const size_t i, const Ticket& ticket) {
			const auto ticketEntity = it.entity(i);
			bool known = false;
			cards.each([&](const HudCard& card) { known = known || card.ticket == ticketEntity.id(); });
			if (!known) {
				BuildCard(it.world(), roomRoot, ticketEntity, ticket);
			}
		})
		.child_of(roomRoot);

	// The Ticket is destroyed right after these events, so the card takes over: it flashes, then goes.
	world.observer<const Ticket>("HudTicketCleared")
		.event<TicketCleared>()
		.each([cards = world.query<HudCard>()](flecs::iter& it, const size_t i, const Ticket&) {
			const float flash = it.world().get<HudLayout>().exitFlash;
			cards.each([&](HudCard& card) {
				if (card.ticket == it.entity(i).id()) {
					card.exiting = true;
					card.cleared = true;
					card.exitLeft = flash;
				}
			});
		})
		.child_of(roomRoot);
	world.observer<const Ticket>("HudLostIntercept")
		.event<LostIntercept>()
		.each([cards = world.query<HudCard>()](flecs::iter& it, const size_t i, const Ticket&) {
			const float flash = it.world().get<HudLayout>().exitFlash;
			cards.each([&](HudCard& card) {
				if (card.ticket == it.entity(i).id()) {
					card.exiting = true;
					card.cleared = false;
					card.exitLeft = flash;
				}
			});
		})
		.child_of(roomRoot);

	world.observer<Score>("HudScorePop")
		.event<ScoreChanged>()
		.each([](flecs::iter& it, size_t, Score&) {
			it.world().get_mut<HudPulse>().score = it.world().get<HudLayout>().popSeconds;
		})
		.child_of(roomRoot);
	world.observer<Score>("HudChainPop")
		.event<ChainChanged>()
		.each([](flecs::iter& it, size_t, Score&) {
			it.world().get_mut<HudPulse>().chain = it.world().get<HudLayout>().popSeconds;
		})
		.child_of(roomRoot);
}

} // namespace

void RegisterHud(const flecs::world& world, const flecs::entity roomRoot) {
	const auto& hud = world.get<HudLayout>();
	world.set<HudPulse>({});
	const auto right = ui::TextAlign::Right;
	MakeLabel(world, roomRoot, "", hud.clockFont, hud.text, right).add<HudClock>();
	MakeLabel(world, roomRoot, "", hud.tagFont, hud.lastCall, right).add<HudClockTag>();
	MakeLabel(world, roomRoot, "", hud.scoreFont, hud.text, right).add<HudScore>();
	MakeLabel(world, roomRoot, "", hud.chainFont, hud.textDim, right).add<HudChain>();
	RegisterHudObservers(world, roomRoot);
	RegisterHudUpdate(world, roomRoot);
}

} // namespace signal_room
