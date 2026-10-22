#include "shift_flow.hpp"

#include <algorithm>
#include <format>
#include <set>

#include "engine/engine.hpp"
#include "hud_layout.hpp"
#include "operator_components.hpp"
#include "room_components.hpp"
#include "room_scene.hpp"
#include "score_components.hpp"
#include "station_catalogue.hpp"
#include "ticket_components.hpp"
#include "ticket_rules.hpp"
#include "ticket_simulation.hpp"

using namespace engine;

namespace signal_room {

namespace {

// Longest route DeriveRoute follows, so a Station catalogue that loops cannot hang the load.
constexpr int kMaxRouteStations = 8;
// Seconds after the hard stop before `interact` presses Retry, so the press that made the last Clear can't skip the results.
constexpr float kRetryGuard = 0.6F;
constexpr float kLineGap = 6.0F;

core::Rect Rect(const float x, const float y, const float w, const float h) { return {.x = x, .y = y, .w = w, .h = h}; }

flecs::entity MakeLine(
	const flecs::world& world,
	const flecs::entity parent,
	const std::string& key,
	const float size,
	const core::Rgba colour,
	const bool wrap = false
) {
	return world.entity()
		.child_of(parent)
		.set(ui::UIRect{})
		.set(ui::Label{.font_size = size, .color = colour, .align = wrap ? ui::TextAlign::Left : ui::TextAlign::Center, .wrap = wrap})
		.set(FlowLine{.key = key});
}

void SetText(const flecs::entity label, const std::string& text) {
	if (auto& line = label.get_mut<ui::Label>(); line.text != text) {
		line.text = text;
	}
}

void Place(const flecs::entity entity, const core::Rect rect) { entity.get_mut<ui::UIRect>().rect = rect; }

flecs::entity MakeModal(const flecs::world& world, const flecs::entity roomRoot, const HudLayout& hud) {
	auto modal = ui::CreateModal(
		world,
		Rect(0.0F, 0.0F, hud.flowWidth, hud.flowHeight),
		ui::Modal{.open = false, .backdrop_color = {.r = 6, .g = 8, .b = 6, .a = static_cast<std::uint8_t>(hud.flowBackdropAlpha)}}
	);
	modal.child_of(roomRoot);
	modal.set(ui::Panel{.color = hud.panel, .border_color = hud.border, .border_thickness = 2.0F, .roundness = 0.0F});
	return modal;
}

std::string StarsText(const HudLayout& hud, const int stars) {
	std::string text;
	for (int i = 0; i < 3; ++i) {
		text += i < stars ? hud.starFull : hud.starEmpty;
	}
	return text;
}

std::string ChainText(const HudLayout& hud, const TicketRules& rules, const Score& score) {
	const auto step = static_cast<size_t>(score.bestChainStep);
	return hud.chainPrefix + std::format("{:.2f}", step < rules.handlerChain.size() ? rules.handlerChain[step] : 1.0F);
}

// The text of a results line for the final Score.
std::string ResultsText(const flecs::world& world, const HudLayout& hud, const std::string& key) {
	const auto& score = world.get<Score>();
	if (key == "score") {
		return std::format("{}: {}", hud.scoreLabel, score.score);
	}
	if (key == "par") {
		return std::format("{}: {}", hud.parLabel, score.par);
	}
	if (key == "stars") {
		return std::format("{}: {}", hud.starsLabel, StarsText(hud, score.stars));
	}
	if (key == "cleared") {
		return std::format("{}: {}", hud.clearedLabel, score.cleared);
	}
	if (key == "lost") {
		return std::format("{}: {}", hud.lostLabel, score.lost);
	}
	return std::format("{}: {}", hud.bestChainLabel, ChainText(hud, world.get<TicketRules>(), score));
}

constexpr const char* kResultKeys[] = {"score", "par", "stars", "cleared", "lost", "chain"};

// Retry reloads the Room after the frame: a reload inside a system builds the new Room before the old one is
// gone (duplicate named entities).
void RetryAfterFrame(const flecs::world& world) {
	auto* context = world.get<EngineContextRef>().ptr;
	context->GetDeferredTaskQueue().Enqueue([context] { ReloadRoom(*context->GetWorld()); });
}

void BuildBriefing(const flecs::world& world, const flecs::entity roomRoot, FlowUi& flow) {
	const auto& hud = world.get<HudLayout>();
	const auto& info = world.get<ShiftBriefing>();
	flow.briefing = MakeModal(world, roomRoot, hud);
	std::ignore = MakeLine(world, flow.briefing, "title", hud.flowTitleFont, hud.text);
	std::ignore = MakeLine(world, flow.briefing, "route", hud.flowBodyFont, hud.text);
	for (size_t i = 0; i < info.notes.size(); ++i) {
		std::ignore = MakeLine(world, flow.briefing, std::format("note{}", i), hud.flowBodyFont - 2.0F, hud.textDim, true);
	}
	ui::CreateButton(world, Rect(0.0F, 0.0F, hud.flowButtonWidth, hud.flowButtonHeight), hud.briefingStart, [](flecs::entity e) {
		SetShiftPhase(e.world(), ShiftPhase::Running);
	}).child_of(flow.briefing);
}

void BuildResults(const flecs::world& world, const flecs::entity roomRoot, FlowUi& flow) {
	const auto& hud = world.get<HudLayout>();
	flow.results = MakeModal(world, roomRoot, hud);
	std::ignore = MakeLine(world, flow.results, "title", hud.flowTitleFont, hud.text);
	for (const char* key : kResultKeys) {
		std::ignore = MakeLine(world, flow.results, key, hud.flowBodyFont, hud.text);
	}
	ui::CreateButton(world, Rect(0.0F, 0.0F, hud.flowButtonWidth, hud.flowButtonHeight), hud.retryLabel, [](flecs::entity e) {
		RetryAfterFrame(e.world());
	}).child_of(flow.results);
}

// Lay the lines of an open card out top to bottom, centred on the screen, with the button at the bottom.
void LayoutCard(const flecs::world& world, const flecs::entity card, const HudLayout& hud) {
	auto& platform = *world.get<platform::PlatformRef>().ptr;
	const float x = (static_cast<float>(platform.Width()) - hud.flowWidth) * 0.5F;
	const float y = (static_cast<float>(platform.Height()) - hud.flowHeight) * 0.5F;
	Place(card, Rect(x, y, hud.flowWidth, hud.flowHeight));
	const float inner = hud.flowWidth - 2.0F * hud.flowPadding;
	float cursor = y + hud.flowPadding;
	card.children([&](const flecs::entity child) {
		if (child.has<ui::Button>()) {
			Place(
				child,
				Rect(x + (hud.flowWidth - hud.flowButtonWidth) * 0.5F,
					 y + hud.flowHeight - hud.flowPadding - hud.flowButtonHeight,
					 hud.flowButtonWidth,
					 hud.flowButtonHeight)
			);
			return;
		}
		const auto& label = child.get<ui::Label>();
		// A wrapped note gets two lines.
		const float height = label.wrap ? 2.0F * (label.font_size + 4.0F) + 4.0F : label.font_size + 8.0F;
		Place(child, Rect(x + hud.flowPadding, cursor, inner, height));
		cursor += height + kLineGap;
	});
}

void RegisterFlowUpdate(const flecs::world& world, const flecs::entity roomRoot) {
	// Not Pausable: the cards and the `interact` press must work while the Room is held.
	world.system("ShiftFlowUpdate")
		.kind(flecs::OnUpdate)
		.run([actions = world.query<const OperatorActions>(), roomRoot](flecs::iter& it) {
			while (it.next()) {
				const auto world = it.world();
				const auto& hud = world.get<HudLayout>();
				const auto& info = world.get<ShiftBriefing>();
				auto& flow = world.get_mut<FlowUi>();
				const auto phase = world.get<ShiftWindow>().phase;
				flow.endedFor = phase == ShiftPhase::Ended ? flow.endedFor + it.delta_time() : 0.0F;

				// Queued before the dismiss below, so a press on this very frame still ends up Running.
				if (phase == ShiftPhase::Briefing && !world.has<scene::Paused>()) {
					world.add<scene::Paused>();
				}

				bool pressed = false;
				actions.each([&](const OperatorActions& action) { pressed = pressed || action.interact; });
				if (pressed && phase == ShiftPhase::Briefing) {
					SetShiftPhase(world, ShiftPhase::Running);
				} else if (pressed && phase == ShiftPhase::Ended && flow.endedFor >= kRetryGuard) {
					RetryAfterFrame(world);
					continue;
				}

				flow.briefing.get_mut<ui::Modal>().open = phase == ShiftPhase::Briefing;
				flow.results.get_mut<ui::Modal>().open = phase == ShiftPhase::Ended;
				if (phase == ShiftPhase::Briefing) {
					LayoutCard(world, flow.briefing, hud);
					size_t note = 0;
					flow.briefing.children([&](const flecs::entity child) {
						if (!child.has<FlowLine>()) {
							return;
						}
						const auto& key = child.get<FlowLine>().key;
						if (key == "title") {
							SetText(child, info.name);
						} else if (key == "route") {
							SetText(child, std::format("{}: {}", hud.routeLabel, DeriveRoute(world, hud.routeArrow)));
						} else if (note < info.notes.size()) {
							SetText(child, info.notes[note++]);
						}
					});
				} else if (phase == ShiftPhase::Ended) {
					LayoutCard(world, flow.results, hud);
					flow.results.children([&](const flecs::entity child) {
						if (!child.has<FlowLine>()) {
							return;
						}
						const auto& key = child.get<FlowLine>().key;
						SetText(child, key == "title" ? hud.resultsTitle : ResultsText(world, hud, key));
					});
				}
			}
		})
		.child_of(roomRoot);
}

} // namespace

std::string DeriveRoute(const flecs::world& world, const std::string& arrow) {
	const auto& catalogue = world.get<StationCatalogue>();
	std::set<std::string> inService;
	world.query_builder<const Station>().without<Tarped>().build().each([&](const Station& station) {
		inService.insert(station.kind);
	});

	struct Step {
		const StationKind* kind;
		const StationTransform* transform;
	};
	const auto find = [&](const auto& matches) -> Step {
		for (const auto& id : inService) {
			const auto* kind = catalogue.Get(id);
			if (kind == nullptr) {
				continue;
			}
			for (const auto& transform : kind->transforms) {
				if (matches(transform)) {
					return {kind, &transform};
				}
			}
		}
		return {};
	};
	const auto nameOf = [](const Step& step) { return step.kind->displayName; };

	// The source takes nothing; each next Station takes what the last one made, until a Transform transmits.
	auto step = find([](const StationTransform& transform) { return transform.inputs.empty(); });
	std::string route;
	for (int i = 0; step.kind != nullptr && i < kMaxRouteStations; ++i) {
		route += (route.empty() ? "" : arrow) + nameOf(step);
		if (step.transform->transmit || step.transform->outputs.empty()) {
			break;
		}
		const auto item = step.transform->outputs.front();
		step = find([&](const StationTransform& transform) { return std::ranges::contains(transform.inputs, item); });
	}
	return route;
}

void RegisterShiftFlow(const flecs::world& world, const flecs::entity roomRoot) {
	FlowUi flow;
	BuildBriefing(world, roomRoot, flow);
	BuildResults(world, roomRoot, flow);
	world.set<FlowUi>(flow);
	RegisterFlowUpdate(world, roomRoot);
}

} // namespace signal_room
