#include "meta_save_example.hpp"

#include <string>

#include <spdlog/spdlog.h>

#include "cube_scene.hpp"
#include "engine/engine.hpp"

using namespace engine;

namespace game {
namespace {

// Find an existing upgrade entity by its id member, or an invalid entity if none.
flecs::entity FindUpgrade(const flecs::world& world, const std::string& id) {
	flecs::entity found;
	world.query_builder<const UnlockedUpgrade>().with<PersistTag>().build().each([&](const flecs::entity entity,
																					 const UnlockedUpgrade& upgrade) {
		if (!found && upgrade.id == id) {
			found = entity;
		}
	});
	return found;
}

// Expose the save flow to scripts as global verbs. These mirror what a menu / gameplay system would
// call in C++, so a designer can drive persistence from AngelScript (see save_demo.as).
void RegisterScriptGlobals(const flecs::world& world) {
	using namespace engine::scripting;

	// SaveGame(string name) — serialize the registered schema to a named slot (Platform storage).
	RegisterGlobalFunctionForScripts(
		world,
		ScriptMethodSignature{
			.name = "SaveGame",
			.return_type = ScriptValueType::MakeVoid(),
			.params = {{.type = ScriptValueType::MakeString(), .by_reference = false, .name = "name"}},
		},
		[](const ScriptCallContext& ctx, const flecs::world& w) { SaveGameToSlot(w, ctx.GetArgString(0)); }
	);

	// LoadGame(string name) — restore the world from a named slot (no-op if the slot is missing).
	RegisterGlobalFunctionForScripts(
		world,
		ScriptMethodSignature{
			.name = "LoadGame",
			.return_type = ScriptValueType::MakeVoid(),
			.params = {{.type = ScriptValueType::MakeString(), .by_reference = false, .name = "name"}},
		},
		[](const ScriptCallContext& ctx, const flecs::world& w) { LoadGameFromSlot(w, ctx.GetArgString(0)); }
	);

	// DeleteSave(string name)
	RegisterGlobalFunctionForScripts(
		world,
		ScriptMethodSignature{
			.name = "DeleteSave",
			.return_type = ScriptValueType::MakeVoid(),
			.params = {{.type = ScriptValueType::MakeString(), .by_reference = false, .name = "name"}},
		},
		[](const ScriptCallContext& ctx, const flecs::world& w) { DeleteSaveSlot(w, ctx.GetArgString(0)); }
	);

	// SaveExists(string name) -> bool
	RegisterGlobalFunctionForScripts(
		world,
		ScriptMethodSignature{
			.name = "SaveExists",
			.return_type = ScriptValueType::MakeBool(),
			.params = {{.type = ScriptValueType::MakeString(), .by_reference = false, .name = "name"}},
		},
		[](ScriptCallContext& ctx, const flecs::world& w) {
			ctx.SetReturnBool(w.get<platform::PlatformRef>().ptr->HasSave(ctx.GetArgString(0)));
		}
	);

	// AddParlorTokens(int amount)
	RegisterGlobalFunctionForScripts(
		world,
		ScriptMethodSignature{
			.name = "AddParlorTokens",
			.return_type = ScriptValueType::MakeVoid(),
			.params = {{.type = ScriptValueType::MakeInt(), .by_reference = false, .name = "amount"}},
		},
		[](const ScriptCallContext& ctx, const flecs::world& w) {
			w.get_mut<MetaProgress>().parlorTokens += ctx.GetArgInt(0);
		}
	);

	// AddScorePoints(int amount)
	RegisterGlobalFunctionForScripts(
		world,
		ScriptMethodSignature{
			.name = "AddScorePoints",
			.return_type = ScriptValueType::MakeVoid(),
			.params = {{.type = ScriptValueType::MakeInt(), .by_reference = false, .name = "amount"}},
		},
		[](const ScriptCallContext& ctx, const flecs::world& w) {
			w.get_mut<MetaProgress>().scorePoints += ctx.GetArgInt(0);
		}
	);

	// CompleteRun() — bumps the persistent run counter.
	RegisterGlobalFunctionForScripts(
		world,
		ScriptMethodSignature{.name = "CompleteRun", .return_type = ScriptValueType::MakeVoid(), .params = {}},
		[](ScriptCallContext&, const flecs::world& w) { ++w.get_mut<MetaProgress>().runsCompleted; }
	);

	// UnlockUpgrade(string id) — find-or-create; leveling up an existing unlock. Demonstrates the
	// tag-based entity path (PersistTag + UnlockedUpgrade).
	RegisterGlobalFunctionForScripts(
		world,
		ScriptMethodSignature{
			.name = "UnlockUpgrade",
			.return_type = ScriptValueType::MakeVoid(),
			.params = {{.type = ScriptValueType::MakeString(), .by_reference = false, .name = "id"}},
		},
		[](const ScriptCallContext& ctx, const flecs::world& w) {
			const std::string id = ctx.GetArgString(0);
			if (const flecs::entity existing = FindUpgrade(w, id)) {
				++existing.get_mut<UnlockedUpgrade>().level;
			}
			else {
				w.entity().add<PersistTag>().set<UnlockedUpgrade>({.id = id, .level = 1});
			}
		}
	);

	// UpgradeCount() -> int
	RegisterGlobalFunctionForScripts(
		world,
		ScriptMethodSignature{.name = "UpgradeCount", .return_type = ScriptValueType::MakeInt(), .params = {}},
		[](ScriptCallContext& ctx, const flecs::world& w) {
			int count = 0;
			w.query_builder().with<PersistTag>().build().each([&count](flecs::entity) { ++count; });
			ctx.SetReturnInt(count);
		}
	);
}

} // namespace

void RegisterMetaSaveExample(const flecs::world& world) {
	// (1) Singleton owned by the example.
	world.set<MetaProgress>({});

	// (2) Reflection so the components show up in the Flecs Explorer. MetaProgress is exposed to
	// scripts as a read-only singleton value type (like InputState) via RegisterSingletonForScripts
	// below — do NOT also RegisterComponentForScripts it, which would register the same name as a
	// ref type and invalidate the AngelScript configuration.
	world.component<MetaProgress>().member<int>("parlorTokens").member<int>("scorePoints").member<int>("runsCompleted");
	world.component<UnlockedUpgrade>().member<std::string>("id").member<int>("level");
	world.component<PersistTag>();

	scripting::RegisterSingletonForScripts(world, world.component<MetaProgress>()); // -> GetMetaProgress()

	// (3) Declare the save schema — exactly what persists and how it maps to/from ECS data.
	auto& save = world.get_mut<save::SaveRegistry>();

	// Meta-progression: abstract game-layer counters (persist between sessions).
	save.Singleton<MetaProgress>("meta")
		.Member("parlorTokens", &MetaProgress::parlorTokens)
		.Member("scorePoints", &MetaProgress::scorePoints)
		.Member("runsCompleted", &MetaProgress::runsCompleted);
	save.Entities<PersistTag>("upgrades").Member("id", &UnlockedUpgrade::id).Member("level", &UnlockedUpgrade::level);

	save.Field("scene.cube.transform")
		.Get([](const flecs::world& w) -> save::Json {
			const flecs::entity cube = w.lookup("CubeSceneRoot::FallingCube");
			if (!cube) {
				return nullptr;
			}
			const auto& t = cube.get<ecs::Transform>();
			return {
				{"px", t.position.x},
				{"py", t.position.y},
				{"pz", t.position.z},
				{"rx", t.rotation.x},
				{"ry", t.rotation.y},
				{"rz", t.rotation.z},
			};
		})
		.Set([](const flecs::world& w, const save::Json& j) {
			const flecs::entity cube = w.lookup("CubeSceneRoot::FallingCube");
			if (!cube || !cube.has<ecs::Transform>()) {
				return;
			}
			auto& t = cube.get_mut<ecs::Transform>();
			t.position = {j.value("px", 0.0F), j.value("py", 3.0F), j.value("pz", 0.0F)};
			t.rotation = {j.value("rx", 0.0F), j.value("ry", 0.0F), j.value("rz", 0.0F)};
			cube.modified<physics::RigidBody>(); // teleports the Jolt body to the new position
		});

	// Falling cube: linear and angular velocity (what the cube was DOING at save time).
	save.Field("scene.cube.velocity")
		.Get([](const flecs::world& w) -> save::Json {
			const flecs::entity cube = w.lookup("CubeSceneRoot::FallingCube");
			if (!cube || !cube.has<physics::PhysicsVelocity>()) {
				return nullptr;
			}
			const auto& [linear, angular] = cube.get<physics::PhysicsVelocity>();
			return {
				{"lx", linear.x},
				{"ly", linear.y},
				{"lz", linear.z},
				{"ax", angular.x},
				{"ay", angular.y},
				{"az", angular.z},
			};
		})
		.Set([](const flecs::world& w, const save::Json& j) {
			const flecs::entity cube = w.lookup("CubeSceneRoot::FallingCube");
			if (!cube) {
				return;
			}
			// PhysicsVelocityOverride is consumed by the physics module next frame — game code
			// never touches the backend directly.
			cube.set<physics::PhysicsVelocityOverride>({
				.linear = {j.value("lx", 0.0F), j.value("ly", 0.0F), j.value("lz", 0.0F)},
				.angular = {j.value("ax", 0.0F), j.value("ay", 0.0F), j.value("az", 0.0F)},
			});
		});

	// Sun light: the color the sun_cycle.as script has animated to at save time.
	save.Field("scene.sun.color")
		.Get([](const flecs::world& w) -> save::Json {
			const flecs::entity sun = w.lookup("CubeSceneRoot::SunLight");
			if (!sun || !sun.has<render::DirectionalLight>()) {
				return nullptr;
			}
			const auto& [r, g, b, a] = sun.get<render::DirectionalLight>().color;
			return {{"r", r}, {"g", g}, {"b", b}, {"a", a}};
		})
		.Set([](const flecs::world& w, const save::Json& j) {
			const flecs::entity sun = w.lookup("CubeSceneRoot::SunLight");
			if (!sun || !sun.has<render::DirectionalLight>()) {
				return;
			}
			auto& c = sun.get_mut<render::DirectionalLight>().color;
			c.r = static_cast<uint8_t>(j.value("r", 255));
			c.g = static_cast<uint8_t>(j.value("g", 244));
			c.b = static_cast<uint8_t>(j.value("b", 214));
			c.a = static_cast<uint8_t>(j.value("a", 255));
		});

	// (4) Script-facing verbs so the whole flow is drivable from AngelScript (see save_demo.as).
	RegisterScriptGlobals(world);

	spdlog::info("[SaveDemo] Registered meta-progression save example");
}

// ── Slot-based save API (shared by UI + scripts), backed by Platform persistence ──

std::string SaveGameToSlot(const flecs::world& world, const std::string& name) {
	auto* platform = world.get<platform::PlatformRef>().ptr;
	platform->WriteSave(name, save::SaveToString(world));
	spdlog::info("[SaveDemo] Saved slot '{}'", name);
	return name;
}

bool LoadGameFromSlot(const flecs::world& world, const std::string& name) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	if (!platform->HasSave(name)) {
		spdlog::warn("[SaveDemo] LoadGame: slot '{}' not found", name);
		return false;
	}

	// Read the raw JSON once; don't apply it yet.
	const std::string json = platform->ReadSave(name);
	spdlog::info("[SaveDemo] Loaded slot '{}'", name);

	if (const flecs::entity cubeScene = scene::FindScene<CubeSceneTag>(world);
		cubeScene && cubeScene.has<scene::Active>()) {
		// flecs::world is reference-counted, so capturing a copy keeps the world alive for the
		// deferred task and gives LoadFromString the mutable world it needs.
		world.get<EngineContextRef>().ptr->GetDeferredTaskQueue().Enqueue([world = flecs::world(world),
																		   json]() mutable {
			scene::ReloadScene<CubeSceneTag>(world);
			save::LoadFromString(world, json);
		});
	}
	else {
		flecs::world mutable_world = world; // refcounted copy; LoadFromString requires a mutable world
		save::LoadFromString(mutable_world, json);
	}

	return true;
}

void DeleteSaveSlot(const flecs::world& world, const std::string& name) {
	world.get<platform::PlatformRef>().ptr->DeleteSave(name);
	spdlog::info("[SaveDemo] Deleted slot '{}'", name);
}

std::vector<std::string> ListSaveSlots(const flecs::world& world) {
	return world.get<platform::PlatformRef>().ptr->ListSaves();
}

} // namespace game
