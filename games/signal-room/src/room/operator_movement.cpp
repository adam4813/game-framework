#include "operator_movement.hpp"

#include <algorithm>
#include <numbers>
#include <vector>

#include "engine/engine.hpp"
#include "floor_collision.hpp"
#include "item_components.hpp"
#include "operator_components.hpp"
#include "room_components.hpp"
#include "room_tuning.hpp"

using namespace engine;

namespace signal_room {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

float YawOf(const glm::vec2 direction) { return std::atan2(-direction.x, -direction.y); }

// Wrap an angle into [-pi, pi).
float WrapAngle(const float angle) { return angle - 2.0F * kPi * std::floor((angle + kPi) / (2.0F * kPi)); }

// Move `from` toward `to` by at most `maxDelta`.
glm::vec2 MoveToward(const glm::vec2 from, const glm::vec2 to, const float maxDelta) {
	const glm::vec2 delta = to - from;
	const float distance = glm::length(delta);
	return distance <= maxDelta ? to : from + delta / distance * maxDelta;
}

// Tick the dash's timers, then start a dash on a press when neither is running. The cooldown starts
// when a dash ends.
void UpdateDash(
	Dash& dash,
	const MovementTuning& tuning,
	const bool pressed,
	const glm::vec2 wish,
	const float facing,
	const float dt
) {
	if (dash.timeLeft > 0.0F) {
		dash.timeLeft -= dt;
		if (dash.timeLeft <= 0.0F) {
			dash.timeLeft = 0.0F;
			dash.cooldownLeft = tuning.dashCooldown;
		}
	}
	else if (dash.cooldownLeft > 0.0F) {
		dash.cooldownLeft = std::max(0.0F, dash.cooldownLeft - dt);
	}

	if (pressed && dash.timeLeft <= 0.0F && dash.cooldownLeft <= 0.0F) {
		dash.timeLeft = tuning.dashDuration;
		dash.direction = wish != glm::vec2{0.0F} ? wish : FacingVector(facing);
	}
}

// How much the carried Item scales its carrier's walk and dash (1 empty-handed).
float CarrySpeed(const flecs::entity op) {
	const auto item = op.target<Carries>();
	return item ? item.get<Item>().carrySpeed : 1.0F;
}

void Steer(
	const float carrySpeed,
	const OperatorActions& actions,
	OperatorMotion& motion,
	Dash& dash,
	const MovementTuning& tuning,
	const float dt
) {
	// The wish may be shorter than 1 (a future analog stick or AI), never longer.
	const float wishLength = glm::length(actions.move);
	const glm::vec2 wishDirection = wishLength > 0.0F ? actions.move / wishLength : glm::vec2{0.0F};
	const glm::vec2 wish = wishLength > 1.0F ? wishDirection : actions.move;
	UpdateDash(dash, tuning, actions.dash, wishDirection, motion.facing, dt);

	const float walk = tuning.walkSpeed * carrySpeed;
	const bool dashing = dash.timeLeft > 0.0F;
	if (dashing) {
		motion.velocity = dash.direction * walk * tuning.dashMultiplier;
	}
	else {
		const float rampTime = wish != glm::vec2{0.0F} ? tuning.accelTime : tuning.decelTime;
		// The ramps take their tuned time at any carry speed.
		motion.velocity = MoveToward(motion.velocity, wish * walk, walk / rampTime * dt);
	}

	const glm::vec2 heading = dashing ? dash.direction : wish;
	if (heading != glm::vec2{0.0F}) {
		const float turn = WrapAngle(YawOf(heading) - motion.facing);
		const float maxTurn = kPi / tuning.turnTime * dt;
		motion.facing = WrapAngle(motion.facing + std::clamp(turn, -maxTurn, maxTurn));
	}
}

} // namespace

void RegisterOperatorMovement(const flecs::world& world, const flecs::entity roomRoot) {
	world.system<const OperatorActions, OperatorMotion, Dash, spatial::Transform>("OperatorMovement")
		.with<Operator>()
		.kind(flecs::OnUpdate)
		.run([](flecs::iter& it) {
			const auto world = it.world();
			const float dt = it.delta_time();
			const auto& tuning = world.get<MovementTuning>();

			// Steer every Operator, then move them all through one collision step.
			std::vector<FloorCircle> circles;
			std::vector<std::pair<OperatorMotion*, spatial::Transform*>> movers;
			while (it.next()) {
				const auto actions = it.field<const OperatorActions>(0);
				auto motion = it.field<OperatorMotion>(1);
				auto dash = it.field<Dash>(2);
				auto transform = it.field<spatial::Transform>(3);
				for (const auto i : it) {
					Steer(CarrySpeed(it.entity(i)), actions[i], motion[i], dash[i], tuning, dt);
					const auto& position = transform[i].position;
					circles.push_back({
						.centre = {position.x, position.z},
						.displacement = motion[i].velocity * dt,
						.radius = tuning.radius,
					});
					movers.emplace_back(&motion[i], &transform[i]);
				}
			}
			if (circles.empty() || !world.has<FloorGrid>()) {
				return;
			}

			MoveCircles(world.get<FloorGrid>(), circles);
			for (size_t i = 0; i < circles.size(); ++i) {
				auto& [motion, transform] = movers[i];
				transform->position.x = circles[i].centre.x;
				transform->position.z = circles[i].centre.y;
				transform->rotation.y = motion->facing;
				// Drop the velocity into whatever it ran into, so it slides on along the surface.
				if (circles[i].push != glm::vec2{0.0F}) {
					const glm::vec2 normal = glm::normalize(circles[i].push);
					motion->velocity -= normal * std::min(0.0F, glm::dot(motion->velocity, normal));
				}
			}
		})
		.child_of(roomRoot);
}

} // namespace signal_room
