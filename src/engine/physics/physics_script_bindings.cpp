#include "physics_script_bindings.hpp"

#include <array>
#include <new>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "engine/scripting/scripting.hpp"
#include "physics_queries.hpp"
#include "physics_types.hpp"

namespace engine::physics {

namespace {
using scripting::ScriptCallContext;
using scripting::ScriptEntityRef;
using scripting::ScriptMethodParam;
using scripting::ScriptValueType;

// Script mirror of RaycastResult. `hit` makes a miss a value (scripts have no optional); on a miss the rest is zero and
// `entity` is the null Entity.
struct ScriptRaycastHit {
	bool hit{false};
	ScriptEntityRef entity{flecs::entity{}};
	glm::vec3 point{0.0F};
	glm::vec3 normal{0.0F};
	float distance{0.0F};
};

// Script mirror of RaycastFilter: one entity to ignore (the null Entity ignores nothing) instead of a span.
struct ScriptRaycastFilter {
	uint32_t layer_mask{kDefaultRaycastMask};
	ScriptEntityRef ignore{flecs::entity{}};
};

ScriptRaycastHit ToScriptHit(const flecs::world& world, const RaycastResult& result) {
	return {
		.hit = true,
		.entity = ScriptEntityRef{world.entity(result.entity)},
		.point = result.hit_point,
		.normal = result.normal,
		.distance = result.distance
	};
}

// Runs `query` with the arguments Raycast and RaycastAll share: (origin, direction, max_distance, filter).
template<typename Query>
auto RunQuery(const ScriptCallContext& ctx, const flecs::world& world, Query query) {
	const Ray ray{
		.origin = *static_cast<const glm::vec3*>(ctx.GetArgObject(0)),
		.direction = *static_cast<const glm::vec3*>(ctx.GetArgObject(1)),
		.max_distance = ctx.GetArgFloat(2)
	};
	const auto& script_filter = *static_cast<const ScriptRaycastFilter*>(ctx.GetArgObject(3));
	const std::array ignore{script_filter.ignore.GetEntity().id()};
	const RaycastFilter filter{
		.layer_mask = script_filter.layer_mask,
		.ignore = ignore[0] != 0 ? std::span<const flecs::entity_t>{ignore} : std::span<const flecs::entity_t>{}
	};
	return query(world, ray, filter);
}

std::vector<ScriptMethodParam> QueryParams(const flecs::world& world) {
	const auto vec3 = ScriptValueType::MakeObject(world.component<glm::vec3>());
	return {
		{.type = vec3, .by_reference = true, .name = "origin"},
		{.type = vec3, .by_reference = true, .name = "direction"},
		{.type = ScriptValueType::MakeFloat(), .by_reference = false, .name = "max_distance"},
		{.type = ScriptValueType::MakeObject(world.component<ScriptRaycastFilter>()),
		 .by_reference = true,
		 .name = "filter"},
	};
}

void RegisterLayerConstants(const flecs::world& world) {
	std::vector<scripting::ScriptConstant> constants;
	for (const auto& [json, reflected, layer] : kCollisionLayerNames) {
		constants.push_back(
			{.name = std::string{"Layer_"} + reflected,
			 .type = ScriptValueType::MakeInt(),
			 .value_str = std::to_string(static_cast<int>(layer))}
		);
	}
	scripting::RegisterGlobalConstantsForScripts(world, std::move(constants));
}
} // namespace

void RegisterPhysicsScriptBindings(const flecs::world& world) {
	// The value types hold an Entity, which the backend reflects; without a backend there is nothing to bind to.
	if (!world.has<scripting::ScriptBackendSingleton>()) return;

	world.component<ScriptRaycastHit>("RaycastHit")
		.member("hit", &ScriptRaycastHit::hit)
		.member("entity", &ScriptRaycastHit::entity)
		.member("point", &ScriptRaycastHit::point)
		.member("normal", &ScriptRaycastHit::normal)
		.member("distance", &ScriptRaycastHit::distance);
	world.component<ScriptRaycastFilter>("RaycastFilter")
		.member("layer_mask", &ScriptRaycastFilter::layer_mask)
		.member("ignore", &ScriptRaycastFilter::ignore);
	scripting::RegisterValueTypeForScripts(world, world.component<ScriptRaycastHit>());
	scripting::RegisterValueTypeForScripts(world, world.component<ScriptRaycastFilter>());

	scripting::RegisterGlobalFunctionForScripts(
		world,
		{.name = "Raycast",
		 .return_type = ScriptValueType::MakeObject(world.component<ScriptRaycastHit>()),
		 .params = QueryParams(world)},
		[](ScriptCallContext& ctx, const flecs::world& w) {
			const auto hit = RunQuery(ctx, w, [](const auto&... args) { return Raycast(args...); });
			new (ctx.GetReturnObjectLocation()) ScriptRaycastHit{hit ? ToScriptHit(w, *hit) : ScriptRaycastHit{}};
		}
	);
	scripting::RegisterGlobalFunctionForScripts(
		world,
		{.name = "RaycastAll",
		 .return_type = ScriptValueType::MakeArray(world.component<ScriptRaycastHit>()),
		 .params = QueryParams(world)},
		[](ScriptCallContext& ctx, const flecs::world& w) {
			const auto results = RunQuery(ctx, w, [](const auto&... args) { return RaycastAll(args...); });
			std::vector<ScriptRaycastHit> hits;
			hits.reserve(results.size());
			for (const RaycastResult& result : results) {
				hits.push_back(ToScriptHit(w, result));
			}
			ctx.SetReturnArray(w.component<ScriptRaycastHit>().id(), hits.data(), hits.size());
		}
	);

	RegisterLayerConstants(world);
}

} // namespace engine::physics
