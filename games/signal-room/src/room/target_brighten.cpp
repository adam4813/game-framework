#include "target_brighten.hpp"

#include <cmath>
#include <numbers>

#include "engine/engine.hpp"
#include "operator_components.hpp"
#include "palette.hpp"
#include "room_components.hpp"
#include "room_tuning.hpp"

using namespace engine;

namespace signal_room {

namespace {

// The pulse swings the blend between full strength and this fraction below it.
constexpr float kPulseDepth = 0.5F;

constexpr core::Rgba kWhite{.r = 255, .g = 255, .b = 255, .a = 255};

core::Rgba Mix(const core::Rgba from, const core::Rgba to, const float t) {
	const auto channel = [t](const uint8_t a, const uint8_t b) {
		return static_cast<uint8_t>(
			std::lround(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t)
		);
	};
	return {.r = channel(from.r, to.r), .g = channel(from.g, to.g), .b = channel(from.b, to.b), .a = from.a};
}

// Call `paint(material, partColour)` on every greybox part under `owner`, at any depth.
template<typename Paint>
void PaintParts(const flecs::entity owner, const Paint& paint) {
	owner.children([&](const flecs::entity part) {
		if (part.has<PartColour>() && part.has<render::Material>()) {
			paint(part.get_mut<render::Material>(), part.get<PartColour>().colour);
		}
		PaintParts(part, paint);
	});
}

} // namespace

void RegisterTargetBrighten(const flecs::world& world, const flecs::entity roomRoot) {
	// A per-frame check over the few Brightened placements rather than an OnRemove observer on
	// (Targets, *): it runs after every Operator's Target has changed this frame, so a placement two
	// Operators share never flickers, and it never paints a placement the Room's Unload is destroying.
	world.system("TargetBrightenRestore")
		.with<Brightened>()
		.kind(flecs::PreStore)
		.each([](const flecs::entity placement) {
			if (placement.world().count(placement.world().pair<Targets>(placement)) > 0) {
				return;
			}
			PaintParts(placement, [](render::Material& material, const core::Rgba base) { material.color = base; });
			placement.remove<Brightened>();
		})
		.child_of(roomRoot);

	world.system<const Operator>("TargetBrighten")
		.with<Targets>(flecs::Wildcard)
		.kind(flecs::PreStore)
		.each([](flecs::iter& it, size_t, const Operator& op) {
			const auto world = it.world();
			const auto target = it.pair(1).second();
			const auto* identity = world.get<Palette>().Get(op.colour);
			if (identity == nullptr) {
				return;
			}
			const auto& tuning = world.get<TargetingTuning>();
			const auto seconds = static_cast<float>(world.get_info()->world_time_total);
			const float wave = 0.5F - 0.5F * std::cos(2.0F * std::numbers::pi_v<float> * tuning.brightenRate * seconds);
			const float blend = tuning.brightenStrength * (1.0F - kPulseDepth * wave);
			const auto tint = Mix(*identity, kWhite, tuning.brightenLift);

			PaintParts(target, [&](render::Material& material, const core::Rgba base) {
				material.color = Mix(base, tint, blend);
			});
			if (!target.has<Brightened>()) {
				target.add<Brightened>();
			}
		})
		.child_of(roomRoot);
}

} // namespace signal_room
