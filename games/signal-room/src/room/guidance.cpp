#include "guidance.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "engine/engine.hpp"
#include "greybox_catalogue.hpp"
#include "greybox_model.hpp"
#include "item_components.hpp"
#include "operator_components.hpp"
#include "palette.hpp"
#include "room_constants.hpp"
#include "room_tuning.hpp"
#include "station_components.hpp"
#include "station_job.hpp"

using namespace engine;

namespace signal_room {

namespace {

constexpr const char* kGlyphPrefix = "glyph_";
constexpr const char* kRefuseGlyph = "refuse";
constexpr float kBubbleScale = 0.75F; // the Bubble's size as authored in the catalogue

// Guidance is flat art standing up in the world (facing +z): lean its top back so it faces the camera, which
// looks down at the angle kCarryTiltToCamera leaves from straight up.
constexpr glm::vec3 kTilt{-(std::numbers::pi_v<float> * 0.5F - kCarryTiltToCamera), 0.0F, 0.0F};

// One chevron the carrying `op` should have over `station`, `slot` of `count` side by side.
struct Wanted {
	flecs::entity_t op;
	flecs::entity_t station;
	int slot;
	int count;
	bool seen{false};
};

glm::vec3 ChevronPosition(
	const GuidanceTuning& tuning,
	const spatial::Transform& station,
	const Wanted& wanted,
	const float age
) {
	const float side = (static_cast<float>(wanted.slot) - static_cast<float>(wanted.count - 1) * 0.5F) * tuning.chevronSpacing;
	const float bob = std::sin(age * tuning.chevronBobRate * 2.0F * std::numbers::pi_v<float>) * tuning.chevronBob;
	return {station.position.x + side, tuning.chevronHeight + bob, station.position.z};
}

std::vector<Wanted> WantedChevrons(
	const flecs::world& world,
	const flecs::query<const Operator>& operators,
	const flecs::query<const Job>& stations
) {
	std::vector<Wanted> wanted;
	stations.each([&](const flecs::entity station, const Job&) {
		std::vector<flecs::entity_t> takers;
		operators.each([&](const flecs::entity op, const Operator&) {
			const auto carried = op.target<Carries>();
			if (carried && TransformTaking(world, station, carried) != nullptr) {
				takers.push_back(op.id());
			}
		});
		for (size_t slot = 0; slot < takers.size(); ++slot) {
			wanted.push_back({takers[slot], station.id(), static_cast<int>(slot), static_cast<int>(takers.size())});
		}
	});
	return wanted;
}

void SpawnChevron(const flecs::world& world, const flecs::entity roomRoot, const Wanted& wanted) {
	const auto* colour = world.get<Palette>().Get(world.entity(wanted.op).get<Operator>().colour);
	const auto chevron = world.entity()
							 .child_of(roomRoot)
							 .set<Chevron>({.operator_ = wanted.op, .station = wanted.station})
							 .set<spatial::Transform>({
								 .position = ChevronPosition(
									 world.get<GuidanceTuning>(),
									 world.entity(wanted.station).get<spatial::Transform>(),
									 wanted,
									 0.0F
								 ),
								 .rotation = kTilt,
							 });
	BuildGreyboxModel(world, chevron, world.get<GreyboxCatalogue>().chevron, glm::vec3{1.0F}, colour != nullptr ? *colour : core::Rgba{});
}

void RegisterChevrons(const flecs::world& world, const flecs::entity roomRoot) {
	world.system("GuidanceChevrons")
		.kind(flecs::OnUpdate)
		.run([roomRoot,
			  operators = world.query<const Operator>(),
			  stations = world.query<const Job>(),
			  chevrons = world.query<Chevron, spatial::Transform>()](flecs::iter& it) {
			while (it.next()) {
				const auto world = it.world();
				auto wanted = WantedChevrons(world, operators, stations);
				const auto& tuning = world.get<GuidanceTuning>();
				chevrons.each([&](const flecs::entity e, Chevron& chevron, spatial::Transform& transform) {
					const auto match = std::ranges::find_if(wanted, [&](const Wanted& w) {
						return !w.seen && w.op == chevron.operator_ && w.station == chevron.station;
					});
					if (match == wanted.end()) {
						e.destruct();
						return;
					}
					match->seen = true;
					chevron.age += it.delta_time();
					const auto& station = world.entity(chevron.station).get<spatial::Transform>();
					transform.position = ChevronPosition(tuning, station, *match, chevron.age);
				});
				for (const auto& w : wanted) {
					if (!w.seen) {
						SpawnChevron(world, roomRoot, w);
					}
				}
			}
		})
		.child_of(roomRoot);
}

// Draw `root`'s parts at their exact palette colour: a Bubble should read white, not lit grey.
void SetUnlit(const flecs::entity root) {
	root.children([](const flecs::entity part) {
		if (part.has<render::Material>()) {
			part.get_mut<render::Material>().unlit = true;
		}
	});
}

// Show or hide the Bubble: scaled to nothing when idle, and only the current glyph's child scaled up.
void ShowBubble(const flecs::entity bubble, const Bubble& state) {
	const bool shown = state.timeLeft > 0.0F;
	bubble.get_mut<spatial::Transform>().scale = glm::vec3{shown ? kBubbleScale : 0.0F};
	bubble.children([&](const flecs::entity child) {
		const std::string name = child.name().c_str();
		if (name.starts_with(kGlyphPrefix)) {
			child.get_mut<spatial::Transform>().scale = glm::vec3{name == kGlyphPrefix + state.glyph ? 1.0F : 0.0F};
		}
	});
}

void RegisterBubbles(const flecs::world& world, const flecs::entity roomRoot) {
	world.system<Bubble, spatial::Transform>("GuidanceBubbles")
		.kind(flecs::OnUpdate)
		.each([](flecs::iter& it, const size_t i, Bubble& bubble, spatial::Transform& transform) {
			const auto world = it.world();
			const auto op = world.entity(bubble.operator_);
			if (!op.is_alive()) {
				return;
			}
			const auto& tuning = world.get<GuidanceTuning>();
			const bool wasShown = bubble.timeLeft > 0.0F;
			bubble.timeLeft = std::max(0.0F, bubble.timeLeft - it.delta_time());
			const auto& at = op.get<spatial::Transform>().position;
			transform.position = {at.x + tuning.bubbleSide, tuning.bubbleHeight, at.z};
			if (wasShown || bubble.timeLeft > 0.0F) {
				ShowBubble(it.entity(i), bubble);
			}
		})
		.child_of(roomRoot);

	world.observer<Operator>("BubbleRefusal")
		.event<Refusal>()
		.each([](flecs::iter& it, const size_t i, Operator&) {
			const auto world = it.world();
			const auto op = it.entity(i);
			world.query<Bubble>().each([&](const flecs::entity bubble, Bubble& state) {
				if (state.operator_ != op.id()) {
					return;
				}
				state.glyph = kRefuseGlyph;
				state.timeLeft = world.get<GuidanceTuning>().bubbleDuration;
				ShowBubble(bubble, state);
			});
		})
		.child_of(roomRoot);
}

} // namespace

void SpawnBubble(const flecs::world& world, const flecs::entity parent, const flecs::entity op) {
	const auto& catalogue = world.get<GreyboxCatalogue>();
	const auto bubble = world.entity()
							.child_of(parent)
							.set<Bubble>({.operator_ = op.id()})
							.set<spatial::Transform>({.position = op.get<spatial::Transform>().position, .rotation = kTilt, .scale = glm::vec3{0.0F}});
	BuildGreyboxModel(world, bubble, catalogue.bubble);
	SetUnlit(bubble);
	for (const auto& id : catalogue.glyphs.Ids()) {
		const auto glyph = world.entity()
							   .child_of(bubble)
							   .set_name((kGlyphPrefix + id).c_str())
							   .set<spatial::Transform>({.scale = glm::vec3{0.0F}});
		BuildGreyboxModel(world, glyph, *catalogue.glyphs.Get(id));
		SetUnlit(glyph);
	}
}

void RegisterGuidance(const flecs::world& world, const flecs::entity roomRoot) {
	RegisterChevrons(world, roomRoot);
	RegisterBubbles(world, roomRoot);
}

} // namespace signal_room
