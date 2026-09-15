#include "ai_module.hpp"

#include <flecs.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "engine/level/level.hpp"
#include "engine/scripting/scripting.hpp"

#include "ai_components.hpp"
#include "bt/bt_module.hpp"
#include "fsm/fsm_module.hpp"
#include "goap/goap_module.hpp"
#include "steering/steering_module.hpp"
#include "utility/utility_module.hpp"

namespace engine::ai {

AIModule::AIModule(flecs::world& world) {
	// === Shared Blackboard ===
	// Register with full reflection and scripting so it is visible in the Explorer
	// and accessible from scripts regardless of which sub-modules are imported.
	world.component<Blackboard>();
	scripting::RegisterComponentForScripts(world, world.component<Blackboard>());
	scripting::RegisterComponentMethodForScripts<&Blackboard::GetFloat>(world, "GetFloat");
	scripting::RegisterComponentMethodForScripts<&Blackboard::SetFloat>(world, "SetFloat");
	scripting::RegisterComponentMethodForScripts<&Blackboard::GetBool>(world, "GetBool");
	scripting::RegisterComponentMethodForScripts<&Blackboard::SetBool>(world, "SetBool");
	scripting::RegisterComponentMethodForScripts<&Blackboard::GetString>(world, "GetString");
	scripting::RegisterComponentMethodForScripts<&Blackboard::SetString>(world, "SetString");

	level::RegisterComponentLoader(world, "blackboard", [](const flecs::entity e, const nlohmann::json& j) {
		Blackboard bb;
		if (j.contains("floats") && j.at("floats").is_object()) {
			for (const auto& [k, v] : j.at("floats").items()) {
				if (v.is_number()) bb.floats[k] = v.get<float>();
			}
		}
		if (j.contains("bools") && j.at("bools").is_object()) {
			for (const auto& [k, v] : j.at("bools").items()) {
				if (v.is_boolean()) bb.bools[k] = v.get<bool>();
			}
		}
		if (j.contains("strings") && j.at("strings").is_object()) {
			for (const auto& [k, v] : j.at("strings").items()) {
				if (v.is_string()) bb.strings[k] = v.get<std::string>();
			}
		}
		e.set<Blackboard>(std::move(bb));
	});

	// === Sub-modules ===
	world.import<BTModule>();
	world.import<GOAPModule>();
	world.import<FSMModule>();
	world.import<UtilityModule>();
	world.import<SteeringModule>();

	spdlog::info("[AIModule] Registered all AI systems (BT, GOAP, FSM, Utility, Steering) + Blackboard");
}

} // namespace engine::ai
