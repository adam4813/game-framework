#include "steering_module.hpp"

#include <cmath>
#include <string>

#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "engine/core/core.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/level/level.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scripting/scripting.hpp"
#include "engine/spatial/spatial.hpp"

#include "steering_components.hpp"

namespace engine::ai {

namespace {

SteeringBehaviorType ParseSteeringType(const std::string& s) {
	if (s == "flee") return SteeringBehaviorType::Flee;
	if (s == "arrive") return SteeringBehaviorType::Arrive;
	if (s == "wander") return SteeringBehaviorType::Wander;
	if (s == "separation") return SteeringBehaviorType::Separation;
	if (s == "cohesion") return SteeringBehaviorType::Cohesion;
	if (s == "alignment") return SteeringBehaviorType::Alignment;
	return SteeringBehaviorType::Seek; // default
}

glm::vec3 Seek(const glm::vec3& pos, const glm::vec3& target, const float max_speed) {
	const glm::vec3 to = target - pos;
	const float dist = glm::length(to);
	return dist > 0.0001F ? (to / dist) * max_speed : glm::vec3{0.0F};
}

glm::vec3 Arrive(
	const glm::vec3& pos,
	const glm::vec3& target,
	const glm::vec3& vel,
	const float max_speed,
	const float slow_r,
	const float stop_r
) {
	const glm::vec3 to = target - pos;
	const float dist = glm::length(to);
	if (dist < stop_r) {
		return -vel;
	}
	const float speed = dist < slow_r ? max_speed * (dist / slow_r) : max_speed;
	return (to / dist) * speed;
}

glm::vec3 Wander(
	SteeringAgent& agent,
	const glm::vec3& fwd,
	const float dt,
	const float jitter,
	const float radius,
	const float distance
) {
	glm::vec3 perp =
		std::abs(fwd.y) < 0.999F ? glm::cross(fwd, glm::vec3(0, 1, 0)) : glm::cross(fwd, glm::vec3(1, 0, 0));
	if (const float plen = glm::length(perp); plen > 0.0001F) {
		perp /= plen;
	}
	const float angle = jitter * dt;
	agent.wander_target = glm::normalize(agent.wander_target * std::cos(angle) + perp * std::sin(angle));
	return fwd * distance + agent.wander_target * radius;
}

} // namespace

SteeringModule::SteeringModule(const flecs::world& world) {
	// === Reflection ===
	world.component<SteeringAgent>()
		.member("max_speed", &SteeringAgent::max_speed)
		.member("max_force", &SteeringAgent::max_force)
		.member("mass", &SteeringAgent::mass);
	world.component<SteeringTarget>().member("position", &SteeringTarget::position);

	// === Scripting ===
	scripting::RegisterComponentForScripts(world, world.component<SteeringAgent>());
	scripting::RegisterComponentForScripts(world, world.component<SteeringTarget>());

	// === Level loaders ===
	level::RegisterComponentLoader(world, "steering_agent", [](const flecs::entity e, const nlohmann::json& j) {
		SteeringAgent agent;
		agent.max_speed = j.value("max_speed", agent.max_speed);
		agent.max_force = j.value("max_force", agent.max_force);
		agent.mass = j.value("mass", agent.mass);
		if (j.contains("behaviors") && j.at("behaviors").is_array()) {
			for (const auto& b : j.at("behaviors")) {
				SteeringBehavior beh;
				beh.type = ParseSteeringType(b.value("type", std::string{"seek"}));
				beh.weight = b.value("weight", 1.0F);
				beh.slow_radius = b.value("slow_radius", beh.slow_radius);
				beh.stop_radius = b.value("stop_radius", beh.stop_radius);
				beh.wander_jitter = b.value("wander_jitter", beh.wander_jitter);
				beh.wander_radius = b.value("wander_radius", beh.wander_radius);
				beh.wander_distance = b.value("wander_distance", beh.wander_distance);
				beh.neighbor_radius = b.value("neighbor_radius", beh.neighbor_radius);
				agent.behaviors.push_back(beh);
			}
		}
		e.set<SteeringAgent>(std::move(agent));
	});

	level::RegisterComponentLoader(world, "steering_target", [](const flecs::entity e, const nlohmann::json& j) {
		SteeringTarget t;
		t.position = core::JVec3(j, "position", t.position);
		if (j.is_array() && j.size() >= 3) {
			t.position = {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
		}
		e.set<SteeringTarget>(t);
	});

	// === System ===
	auto neighbor_q = world.query<const SteeringAgent, const spatial::WorldTransform>();

	std::ignore =
		world.system<SteeringAgent, const spatial::WorldTransform, spatial::Transform>("SteeringSystem")
			.kind(flecs::OnUpdate)
			.each([nq = std::move(neighbor_q)](
					  const flecs::iter& it,
					  size_t i,
					  SteeringAgent& agent,
					  const spatial::WorldTransform& wt,
					  spatial::Transform& transform
				  ) {
				const float dt = it.delta_time();
				const flecs::entity self = it.entity(i);
				const glm::vec3 pos = wt.position;

				glm::vec3 total{0.0F};

				for (
					const auto& [type, weight, slow_radius, stop_radius, wander_jitter, wander_radius, wander_distance, neighbor_radius] :
					agent.behaviors) {
					glm::vec3 force{0.0F};
					const bool has_target = self.has<SteeringTarget>();

					switch (type) {
					case SteeringBehaviorType::Seek:
						if (has_target) {
							force = Seek(pos, self.get<SteeringTarget>().position, agent.max_speed) - agent.velocity;
						}
						break;
					case SteeringBehaviorType::Flee:
						if (has_target) {
							force = -Seek(pos, self.get<SteeringTarget>().position, agent.max_speed) - agent.velocity;
						}
						break;
					case SteeringBehaviorType::Arrive:
						if (has_target) {
							force = Arrive(
										pos,
										self.get<SteeringTarget>().position,
										agent.velocity,
										agent.max_speed,
										slow_radius,
										stop_radius
									)
									- agent.velocity;
						}
						break;
					case SteeringBehaviorType::Wander:
					{
						const glm::vec3 fwd = glm::length2(agent.velocity) > 0.0001F ? glm::normalize(agent.velocity)
																					 : glm::vec3(0, 0, 1);
						const glm::vec3 wp = Wander(agent, fwd, dt, wander_jitter, wander_radius, wander_distance);
						force = Seek(pos, pos + wp, agent.max_speed) - agent.velocity;
						break;
					}
					case SteeringBehaviorType::Separation:
					{
						glm::vec3 sep{0.0F};
						int n = 0;
						nq.each(
							[&](const flecs::entity other, const SteeringAgent&, const spatial::WorldTransform& owt) {
								if (other == self) return;
								const glm::vec3 diff = pos - owt.position;
								if (const float d = glm::length(diff); d > 0.0001F && d < neighbor_radius) {
									sep += diff / d;
									++n;
								}
							}
						);
						if (n > 0) {
							force = glm::normalize(sep) * agent.max_speed - agent.velocity;
						}
						break;
					}
					case SteeringBehaviorType::Cohesion:
					{
						glm::vec3 centroid{0.0F};
						int n = 0;
						nq.each(
							[&](const flecs::entity other, const SteeringAgent&, const spatial::WorldTransform& owt) {
								if (other == self) return;
								if (glm::distance(pos, owt.position) < neighbor_radius) {
									centroid += owt.position;
									++n;
								}
							}
						);
						if (n > 0) {
							centroid /= static_cast<float>(n);
							force = Seek(pos, centroid, agent.max_speed) - agent.velocity;
						}
						break;
					}
					case SteeringBehaviorType::Alignment:
					{
						glm::vec3 avg{0.0F};
						int n = 0;
						nq.each([&](const flecs::entity other,
									const SteeringAgent& oa,
									const spatial::WorldTransform& owt) {
							if (other == self) return;
							if (glm::distance(pos, owt.position) < neighbor_radius) {
								avg += oa.velocity;
								++n;
							}
						});
						if (n > 0) {
							force = glm::normalize(avg / static_cast<float>(n)) * agent.max_speed - agent.velocity;
						}
						break;
					}
					}

					// Clamp individual behavior force, then blend with weight.
					if (const float flen2 = glm::length2(force); flen2 > agent.max_force * agent.max_force) {
						force = glm::normalize(force) * agent.max_force;
					}
					total += force * weight;
				}

				// Clamp total force and integrate velocity.
				if (const float tlen2 = glm::length2(total); tlen2 > agent.max_force * agent.max_force) {
					total = glm::normalize(total) * agent.max_force;
				}
				agent.steering_output = total;

				const glm::vec3 accel = total / std::max(0.001F, agent.mass);
				agent.velocity += accel * dt;
				if (const float spd2 = glm::length2(agent.velocity); spd2 > agent.max_speed * agent.max_speed) {
					agent.velocity = glm::normalize(agent.velocity) * agent.max_speed;
				}
				transform.position += agent.velocity * dt;
			})
			.add<ecs::Pausable>()
			.add<scene::GameScene>();

	spdlog::info("[SteeringModule] Registered steering system");
}

} // namespace engine::ai
