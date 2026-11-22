#include "campaign_script.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <utility>

#include "campaign.hpp"
#include "crabnado.hpp"
#include "crabnado_constants.hpp"
#include "enemy_types.hpp"
#include "run.hpp"
#include "sfx.hpp"

using namespace engine;

namespace crabnado {

namespace {

using enum scripting::ScriptValueKind;

const std::vector<scripting::ScriptEvent> kEvents{
	{.name = "OnActStart", .params = {}},
	{.name = "OnStageStart", .params = {}},
	{.name = "OnStageEnd", .params = {String}},
	{.name = "OnBossStart", .params = {String}},
	{.name = "OnBossPhase", .params = {String, Int}},
	{.name = "OnBossDefeated", .params = {String}},
	{.name = "OnKill", .params = {String, String}},
	{.name = "OnTimer", .params = {String}},
};

// The two script slots: the Act's, loaded first, and the Stage's.
constexpr size_t kActSlot = 0;
constexpr size_t kStageSlot = 1;

// Singleton, not reflected: the entity the scripts run on (their `self`) and the event host that runs them.
struct CampaignScriptHost {
	flecs::entity host;
	scripting::EventHost events{{
		.log_tag = "[Campaign]",
		.events = kEvents,
		.slots = {"act", "stage"},
		.timer_event = "OnTimer",
		.max_timers = constants::kMaxScriptTimers,
		.max_apply_rounds = constants::kMaxScriptApplyRounds,
	}};
};

scripting::IScriptBackend* Backend(const flecs::world& world) {
	if (!world.has<scripting::ScriptBackendSingleton>()) {
		return nullptr;
	}
	return world.get_mut<scripting::ScriptBackendSingleton>().backend.get();
}

// Lists the ids of the live scripts in RunState, for the Remote API.
void RefreshActive(const flecs::world& world) {
	std::string ids;
	for (const std::string& id : world.get<CampaignScriptHost>().events.AliveIds()) {
		ids += (ids.empty() ? "" : ",") + id;
	}
	world.get_mut<RunState>().active_scripts = std::move(ids);
}

void LoadSlot(const flecs::world& world, const size_t slot, const std::string_view id) {
	auto& host = world.get_mut<CampaignScriptHost>();
	host.events.Unload(world, slot);
	scripting::IScriptBackend* backend = Backend(world);
	if (!id.empty() && backend != nullptr) {
		if (!host.host.is_alive()) {
			host.host = world.entity();
		}
		const std::string path = ScriptPath(world, id);
		scripting::IScriptInstance* instance = backend->CreateInstanceFromFile(path, host.host);
		if (instance == nullptr) {
			ecs::Log(world).error(
				"[Campaign] {} script '{}' ('{}') didn't load, skipped",
				slot == kActSlot ? "act" : "stage",
				id,
				path
			);
		}
		std::ignore = host.events.Load(world, slot, std::string{id}, instance);
	}
	RefreshActive(world);
}

// Queues a world-changing call for the end of the director's tick; `apply` returns why it was refused, or nothing.
void QueueCall(const flecs::world& world, std::string label, scripting::EventHost::ApplyFn apply) {
	std::ignore = world.get_mut<CampaignScriptHost>().events.Queue(world, std::move(label), std::move(apply));
}

// The reason a Spawn() is refused, or an empty string after throwing the enemies.
std::string
SpawnForScript(const flecs::world& world, const std::string& type_id, const int count, const std::string& source) {
	const auto& run = world.get<RunState>();
	if (!IsRunning(run.phase)) {
		if (IsMolting(run.phase, run.step)) {
			return "in a molt";
		}
		return run.phase == RunPhase::Transition ? "between Acts" : "the run isn't live";
	}
	const auto& types = world.get<EnemyRegistry>().types;
	const auto type = std::ranges::find(types, type_id, &EnemyType::id);
	if (type == types.end()) {
		return "unknown enemy type";
	}
	if (std::ranges::find(SpawnerIds(), source) == SpawnerIds().end()) {
		return "unknown spawner";
	}
	if (count <= 0) {
		return "count must be positive";
	}
	ThrowEnemies(world, static_cast<int>(type - types.begin()), count);
	return {};
}

std::string ShowBanner(const flecs::world& world, const std::string& text) {
	world.get_mut<CampaignScripts>().banner = text;
	auto& cooldown = world.lookup(kScriptBannerName).get_mut<timer::Cooldown>();
	cooldown.duration = constants::kScriptBannerTime;
	cooldown.remaining = constants::kScriptBannerTime;
	return {};
}

std::string PlaySting(const flecs::world& world, const std::string& id) {
	const std::optional<Sfx> sfx = FindSfx(id);
	if (!sfx) {
		return "unknown sting";
	}
	PlaySfx(world, *sfx);
	return {};
}

// Applies the queued calls in call order and records the last one in RunState.
void ApplyQueue(const flecs::world& world) {
	for (const auto& call : world.get_mut<CampaignScriptHost>().events.ApplyQueue(world)) {
		world.get_mut<RunState>().last_script_call =
			call.refusal.empty() ? call.label : std::format("{} (refused: {})", call.label, call.refusal);
	}
}

// Reads a Remote API argument per its kind: an int parsed (0 if it isn't one), anything else as the string.
scripting::ScriptArg ParseArg(const scripting::ScriptValueKind kind, const std::string& text) {
	if (kind != Int) {
		return text;
	}
	int value = 0;
	std::from_chars(text.data(), text.data() + text.size(), value);
	return value;
}

void FireRequested(const flecs::world& world) {
	auto& scripts = world.get_mut<CampaignScripts>();
	if (scripts.fire_event.empty()) {
		return;
	}
	const std::string event = std::exchange(scripts.fire_event, {});
	const scripting::ScriptEvent* spec = world.get<CampaignScriptHost>().events.FindEvent(event);
	if (spec == nullptr) {
		ecs::Log(world).warn("[Campaign] fire_event '{}' is not a campaign event", event);
		return;
	}
	std::vector<scripting::ScriptArg> args;
	const std::array<const std::string*, 2> raw{&scripts.fire_arg0, &scripts.fire_arg1};
	for (size_t i = 0; i < spec->params.size(); ++i) {
		args.push_back(ParseArg(spec->params[i], *raw[i]));
	}
	FireCampaignEvent(world, event, args);
}

void RegisterApi(const flecs::world& world) {
	using scripting::ScriptCallContext;
	using scripting::ScriptMethodParam;
	using scripting::ScriptMethodSignature;
	using scripting::ScriptValueType;
	const auto text = [](const char* name) { return ScriptMethodParam{ScriptValueType::MakeString(), true, name}; };
	const auto number = [](const char* name) { return ScriptMethodParam{ScriptValueType::MakeInt(), false, name}; };
	const auto seconds = [](const char* name) { return ScriptMethodParam{ScriptValueType::MakeFloat(), false, name}; };
	const auto reg = [&world](ScriptMethodSignature sig, scripting::GlobalFunctionThunk thunk) {
		scripting::RegisterGlobalFunctionForScripts(world, sig, std::move(thunk));
	};

	reg({.name = "EndStage", .return_type = ScriptValueType::MakeVoid()}, [](ScriptCallContext&, flecs::world& w) {
		QueueCall(w, "EndStage()", [](const flecs::world& world) { return std::string{EndStageEarly(world)}; });
	});
	reg({.name = "SpawnBoss", .return_type = ScriptValueType::MakeVoid(), .params = {text("id")}},
		[](ScriptCallContext& ctx, flecs::world& w) {
			std::string id = ctx.GetArgString(0);
			QueueCall(w, std::format("SpawnBoss(\"{}\")", id), [id](const flecs::world& world) {
				return std::string{CallBossNow(world, id)};
			});
		});
	reg({.name = "Spawn",
		 .return_type = ScriptValueType::MakeVoid(),
		 .params = {text("type"), number("count"), text("source")}},
		[](ScriptCallContext& ctx, flecs::world& w) {
			std::string type = ctx.GetArgString(0);
			const int count = ctx.GetArgInt(1);
			std::string source = ctx.GetArgString(2);
			QueueCall(
				w,
				std::format("Spawn(\"{}\", {}, \"{}\")", type, count, source),
				[type, count, source](const flecs::world& world) { return SpawnForScript(world, type, count, source); }
			);
		});
	reg({.name = "Banner", .return_type = ScriptValueType::MakeVoid(), .params = {text("text")}},
		[](ScriptCallContext& ctx, flecs::world& w) {
			std::string banner = ctx.GetArgString(0);
			QueueCall(w, std::format("Banner(\"{}\")", banner), [banner](const flecs::world& world) {
				return ShowBanner(world, banner);
			});
		});
	reg({.name = "Sting", .return_type = ScriptValueType::MakeVoid(), .params = {text("id")}},
		[](ScriptCallContext& ctx, flecs::world& w) {
			std::string id = ctx.GetArgString(0);
			QueueCall(w, std::format("Sting(\"{}\")", id), [id](const flecs::world& world) {
				return PlaySting(world, id);
			});
		});
	// After() isn't world-changing, so it registers at once, for the script that called it.
	reg({.name = "After", .return_type = ScriptValueType::MakeVoid(), .params = {seconds("seconds"), text("name")}},
		[](ScriptCallContext& ctx, flecs::world& w) {
			std::ignore = w.get_mut<CampaignScriptHost>().events.After(w, ctx.GetArgFloat(0), ctx.GetArgString(1));
		});
	reg({.name = "StageClock", .return_type = ScriptValueType::MakeFloat()}, [](ScriptCallContext& ctx, flecs::world& w) {
		ctx.SetReturnFloat(w.get<RunState>().stage_clock);
	});
	reg({.name = "Kills", .return_type = ScriptValueType::MakeInt()}, [](ScriptCallContext& ctx, flecs::world& w) {
		ctx.SetReturnInt(w.get<RunState>().kills);
	});
	reg({.name = "Act", .return_type = ScriptValueType::MakeInt()}, [](ScriptCallContext& ctx, flecs::world& w) {
		ctx.SetReturnInt(w.get<RunState>().act);
	});
	reg({.name = "Stage", .return_type = ScriptValueType::MakeInt()}, [](ScriptCallContext& ctx, flecs::world& w) {
		ctx.SetReturnInt(w.get<RunState>().stage);
	});
}

} // namespace

std::span<const scripting::ScriptEvent> CampaignEvents() { return kEvents; }

std::string ScriptPath(const flecs::world& world, const std::string_view id) {
	const std::string& directory = world.get<CampaignSource>().scripts_path;
	return std::format("{}{}.as", ResolveSourcePath(world.get<platform::PlatformRef>().ptr->AssetDirectory(), directory), id);
}

void RegisterCampaignScripts(const flecs::world& world) {
	world.component<CampaignScripts>()
		.member<std::string>("banner")
		.member<std::string>("fire_event")
		.member<std::string>("fire_arg0")
		.member<std::string>("fire_arg1");
	world.component<CampaignScriptHost>();
	world.set<CampaignScripts>({});
	std::ignore = world.entity(kScriptBannerName).set<timer::Cooldown>({});
	world.set<CampaignScriptHost>({});
	RegisterApi(world);
}

void ResetCampaignScripts(const flecs::world& world, const flecs::entity sceneRoot) {
	UnloadCampaignScripts(world);
	world.set<CampaignScripts>({});
	world.get_mut<CampaignScriptHost>().host = world.entity("CampaignScripts").child_of(sceneRoot);
}

void UnloadCampaignScripts(const flecs::world& world) {
	world.get_mut<CampaignScriptHost>().events.UnloadAll(world);
	RefreshActive(world);
}

void LoadActScript(const flecs::world& world, const std::string_view id) {
	LoadSlot(world, kActSlot, id);
}

void LoadStageScript(const flecs::world& world, const std::string_view id) {
	LoadSlot(world, kStageSlot, id);
}

bool StageScriptAlive(const flecs::world& world) { return world.get<CampaignScriptHost>().events.Alive(kStageSlot); }

void EndActScripts(const flecs::world& world) {
	FireCampaignEvent(world, "OnStageEnd", {std::string{"boss"}});
	auto& events = world.get_mut<CampaignScriptHost>().events;
	events.Unload(world, kStageSlot);
	events.Unload(world, kActSlot);
	RefreshActive(world);
}

void FireCampaignEvent(const flecs::world& world, const std::string_view event, const std::span<const scripting::ScriptArg> args) {
	world.get_mut<CampaignScriptHost>().events.Fire(world, event, args);
	RefreshActive(world);
}

void FireCampaignEvent(const flecs::world& world, const std::string_view event, const std::initializer_list<scripting::ScriptArg> args) {
	FireCampaignEvent(world, event, std::span<const scripting::ScriptArg>{args.begin(), args.size()});
}

void StepCampaignScripts(const flecs::world& world, const float dt) {
	auto& scripts = world.get_mut<CampaignScripts>();
	if (world.lookup(kScriptBannerName).get<timer::Cooldown>().Ready()) {
		scripts.banner.clear();
	}
	FireRequested(world);
	if (IsRunning(world.get<RunState>().phase)) {
		world.get_mut<CampaignScriptHost>().events.StepTimers(world, dt);
		RefreshActive(world);
	}
	ApplyQueue(world);
}

} // namespace crabnado
