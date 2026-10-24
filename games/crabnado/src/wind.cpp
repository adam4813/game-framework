#include "wind.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>
#include <string>

#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "player.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr float kTau = 2.0F * std::numbers::pi_v<float>;
// The direction meanders slowly between shifts (the prototype's sin(t * 0.37) * 0.04).
constexpr float kMeanderRate = 0.37F;
constexpr float kMeanderAmount = 0.04F;
constexpr float kSwellRate = 0.31F;

// Streaks: short unlit bars scattered over a square of half-size kStreakSpread around the player, wrapping
// at its edges. Speed and length grow with strength, so gusts show.
constexpr int kStreakCount = 140;
constexpr float kStreakSpread = 45.0F;
constexpr float kStreakSpeed = 18.0F;
constexpr float kStreakBaseLength = 1.5F;
constexpr float kStreakLengthPerStrength = 3.0F;
constexpr float kStreakThickness = 0.14F;
constexpr core::Rgba kStreakColor{.r = 250, .g = 252, .b = 255, .a = 255};

// Compass look, from the prototype.
constexpr core::Rgba kCompassFace{.r = 8, .g = 28, .b = 40, .a = 190};
constexpr core::Rgba kCompassRim{.r = 60, .g = 90, .b = 105, .a = 200};
constexpr core::Rgba kCompassNorth{.r = 169, .g = 195, .b = 204, .a = 255};
constexpr core::Rgba kWindColor{.r = 155, .g = 231, .b = 255, .a = 255};
constexpr core::Rgba kWarnColor{.r = 255, .g = 209, .b = 102, .a = 255};
constexpr float kArrowHead = 10.0F;
constexpr float kArrowHeadAngle = 2.5F; // radians off the arrow's direction

float WrapAround(const float value, const float center) {
	return center + std::remainder(value - center, 2.0F * kStreakSpread);
}

void PushCenteredText(ui::UIDrawList& list, const platform::Platform& platform, const std::string& text,
	const glm::vec2 center, const float size, const core::Rgba color) {
	const float width = platform.MeasureText(text, size);
	list.PushText(text, center.x - width / 2.0F, center.y - size / 2.0F, size, color);
}

} // namespace

void RegisterWind(const flecs::world& world) {
	world.component<Wind>()
		.member<float>("direction")
		.member<float>("strength")
		.member<float>("gust")
		.member<float>("turn")
		.member<float>("shift_timer")
		.member<float>("gust_timer")
		.member<float>("time")
		.member<bool>("warning")
		.member<int>("shifts");
	world.set<Wind>({});
	world.component<WindStreak>().member<float>("pace");
	world.component<WindCompass>().member<glm::vec2>("center").member<float>("radius");

	world.system<Wind>("WindBlow")
		.term_at(0)
		.singleton()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, Wind& wind) {
			const float dt = it.delta_time();
			auto& rng = it.world().get_mut<ecs::RngState>();
			wind.time += dt;

			wind.shift_timer -= dt;
			if (wind.shift_timer <= 0.0F) {
				const float sign = rng.Chance(0.5F) ? 1.0F : -1.0F;
				wind.turn = sign * rng.NextFloatRange(constants::kWindTurnMin, constants::kWindTurnMax);
				wind.shift_timer = rng.NextFloatRange(constants::kWindShiftMin, constants::kWindShiftMax);
				++wind.shifts;
			}
			wind.warning = wind.shift_timer <= constants::kWindWarningTime;

			const float step = std::copysign(std::min(std::abs(wind.turn), constants::kWindTurnRate * dt), wind.turn);
			wind.turn -= step;
			wind.direction += step + std::sin(wind.time * kMeanderRate) * kMeanderAmount * dt;
			wind.direction -= kTau * std::floor(wind.direction / kTau);

			wind.gust_timer -= dt;
			if (wind.gust_timer <= 0.0F) {
				wind.gust = constants::kWindGust;
				wind.gust_timer = rng.NextFloatRange(constants::kWindGustMin, constants::kWindGustMax);
			}
			wind.gust = std::max(0.0F, wind.gust - constants::kWindGustFade * dt);
			wind.strength =
				constants::kWindBaseStrength + constants::kWindSwell * std::sin(wind.time * kSwellRate) + wind.gust;
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// Streaks ride the wind and wrap around the player, so the screen is always full of them.
	world.system<const WindStreak, spatial::Transform>("WindStreakMove")
		.kind(flecs::OnUpdate)
		.run([](flecs::iter& it) {
			const flecs::world world = it.world();
			const auto& wind = world.get<Wind>();
			const glm::vec2 direction = WindDirection(wind);
			const float heading = std::atan2(direction.x, direction.y);
			glm::vec2 center{0.0F};
			if (const flecs::entity player = FindPlayer(world)) {
				const glm::vec3 p = player.get<spatial::Transform>().position;
				center = {p.x, p.z};
			}
			const float dt = it.delta_time();
			while (it.next()) {
				const auto streaks = it.field<const WindStreak>(0);
				auto transforms = it.field<spatial::Transform>(1);
				for (const auto i : it) {
					const float pace = streaks[i].pace;
					spatial::Transform& transform = transforms[i];
					const glm::vec2 moved =
						glm::vec2{transform.position.x, transform.position.z} +
						direction * wind.strength * kStreakSpeed * pace * dt;
					transform.position.x = WrapAround(moved.x, center.x);
					transform.position.z = WrapAround(moved.y, center.y);
					transform.rotation.y = heading;
					transform.scale.z = (kStreakBaseLength + kStreakLengthPerStrength * wind.strength) * pace;
				}
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// Drawn into the UI draw list after UIBuildDrawList (same phase, registered later), at the base layer
	// so the molt modal still covers it. North-up: screen x is world x and screen y is world z.
	world.system<const WindCompass>("WindCompassDraw")
		.kind(flecs::PreStore)
		.each([](const flecs::iter& it, size_t, const WindCompass& compass) {
			const flecs::world world = it.world();
			const auto& wind = world.get<Wind>();
			const auto& platform = *world.get<platform::PlatformRef>().ptr;
			auto& list = world.get_mut<ui::UIDrawList>();
			list.SetLayer(0);

			const glm::vec2 c = compass.center;
			const float r = compass.radius;
			list.PushCircle(c, r, kCompassRim);
			list.PushCircle(c, r - 3.0F, kCompassFace);
			PushCenteredText(list, platform, "N", {c.x, c.y - r + 13.0F}, 14.0F, kCompassNorth);

			const glm::vec2 direction = WindDirection(wind);
			const glm::vec2 tip = c + direction * r * (0.35F + 0.25F * wind.strength);
			const float angle = std::atan2(direction.y, direction.x);
			list.PushLine(c, tip, 4.0F, kWindColor);
			for (const float side : {kArrowHeadAngle, -kArrowHeadAngle}) {
				list.PushLine(tip, tip + glm::vec2{std::cos(angle + side), std::sin(angle + side)} * kArrowHead, 4.0F,
					kWindColor);
			}

			const core::Rgba labelColor = wind.warning ? kWarnColor : kWindColor;
			PushCenteredText(list, platform, std::format("WIND F{}", WindForce(wind)), {c.x, c.y + r + 14.0F}, 18.0F,
				labelColor);
			if (wind.warning) {
				PushCenteredText(list, platform, "Wind shifting!", {c.x, c.y + r + 36.0F}, 20.0F, kWarnColor);
			}
		})
		.add<scene::GameScene>();
}

void StartWind(const flecs::world& world) {
	auto& rng = world.get_mut<ecs::RngState>();
	world.set<Wind>(
		{.direction = rng.NextFloatRange(0.0F, kTau),
		 .strength = constants::kWindBaseStrength,
		 .shift_timer = rng.NextFloatRange(constants::kWindShiftMin, constants::kWindShiftMax),
		 .gust_timer = rng.NextFloatRange(constants::kWindGustMin, constants::kWindGustMax)}
	);
}

glm::vec2 BearingDirection(const float bearing) {
	return {std::sin(bearing), -std::cos(bearing)};
}

glm::vec2 WindDirection(const Wind& wind) {
	return BearingDirection(wind.direction);
}

glm::vec2 WindPush(const Wind& wind, const float scale) {
	return WindDirection(wind) * wind.strength * scale;
}

int WindForce(const Wind& wind) {
	return static_cast<int>(std::lround(2.0F + wind.strength * 3.0F));
}

void SpawnWindStreaks(const flecs::world& world, const flecs::entity parent) {
	auto& rng = world.get_mut<ecs::RngState>();
	const flecs::entity streaks = world.entity("WindStreaks").child_of(parent);
	for (int i = 0; i < kStreakCount; ++i) {
		std::ignore = world.entity()
						  .child_of(streaks)
						  .set<WindStreak>({.pace = rng.NextFloatRange(0.7F, 1.3F)})
						  .set<spatial::Transform>(
							  {.position =
								   {rng.NextFloatRange(-kStreakSpread, kStreakSpread), rng.NextFloatRange(0.6F, 5.0F),
									rng.NextFloatRange(-kStreakSpread, kStreakSpread)}}
						  )
						  .set<render::CubePrimitive>({.size = {kStreakThickness, kStreakThickness, 1.0F}})
						  .set<render::Material>({.color = kStreakColor, .cast_shadow = false, .unlit = true});
	}
}

} // namespace crabnado
