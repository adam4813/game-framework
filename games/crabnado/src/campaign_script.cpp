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

constexpr std::array<CampaignEventSpec, 8> kEvents{{
	{"OnActStart", "void OnActStart()", ""},
	{"OnStageStart", "void OnStageStart()", ""},
	{"OnStageEnd", "void OnStageEnd(const string &in)", "s"},
	{"OnBossStart", "void OnBossStart(const string &in)", "s"},
	{"OnBossPhase", "void OnBossPhase(const string &in, int)", "si"},
	{"OnBossDefeated", "void OnBossDefeated(const string &in)", "s"},
	{"OnKill", "void OnKill(const string &in, const string &in)", "ss"},
	{"OnTimer", "void OnTimer(const string &in)", "s"},
}};

constexpr int kOnTimer = 7;

// The two script slots: the Act's, loaded first, and the Stage's.
constexpr int kActSlot = 0;
constexpr int kStageSlot = 1;
constexpr std::array<std::string_view, 2> kSlotNames{"act", "stage"};

// One loaded script. `handles[i]` is whether it defines event i's handler, looked up once at load.
struct LoadedScript {
	std::string id;
	scripting::IScriptInstance* instance{nullptr};
	std::array<bool, kEvents.size()> handles{};
	bool dead{false};
};

enum class CallKind { EndStage, SpawnBoss, Spawn, Banner, Sting };

// A world-changing call a handler made, waiting for the end of the tick. `label` is how last_script_call shows it.
struct ScriptCall {
	CallKind kind{CallKind::EndStage};
	std::string label;
	std::string text;
	std::string source;
	int count{0};
};

struct ScriptTimer {
	float left{0.0F};
	std::string name;
	int slot{0};
};

// Singleton, not reflected: the loaded scripts and what they've asked for. `dispatching` is the slot whose handler is
// running (-1 outside a handler), so a call knows who made it and a call from anywhere else is refused.
struct CampaignScriptHost {
	flecs::entity host;
	std::array<LoadedScript, 2> slots;
	std::vector<ScriptCall> queue;
	std::vector<ScriptTimer> timers;
	int dispatching{-1};
};

std::string StripSpaces(std::string_view text) {
	std::string out;
	for (const char c : text) {
		if (c != ' ') {
			out += c;
		}
	}
	return out;
}

int EventIndex(const std::string_view name) {
	const auto found = std::ranges::find(kEvents, name, &CampaignEventSpec::name);
	return found == kEvents.end() ? -1 : static_cast<int>(found - kEvents.begin());
}

scripting::IScriptBackend* Backend(const flecs::world& world) {
	if (!world.has<scripting::ScriptBackendSingleton>()) {
		return nullptr;
	}
	return world.get_mut<scripting::ScriptBackendSingleton>().backend.get();
}

// Lists the ids of the live scripts in RunState, for the Remote API.
void RefreshActive(const flecs::world& world, const CampaignScriptHost& host) {
	std::string ids;
	for (const LoadedScript& script : host.slots) {
		if (script.instance != nullptr && !script.dead) {
			ids += (ids.empty() ? "" : ",") + script.id;
		}
	}
	world.get_mut<RunState>().active_scripts = std::move(ids);
}

void UnloadSlot(const flecs::world& world, CampaignScriptHost& host, const int slot) {
	LoadedScript& script = host.slots[static_cast<size_t>(slot)];
	if (script.instance != nullptr) {
		if (scripting::IScriptBackend* backend = Backend(world)) {
			backend->DestroyInstance(script.instance);
		}
	}
	script = {};
	std::erase_if(host.timers, [slot](const ScriptTimer& timer) { return timer.slot == slot; });
	RefreshActive(world, host);
}

void LoadSlot(const flecs::world& world, const int slot, const std::string_view id) {
	auto& host = world.get_mut<CampaignScriptHost>();
	UnloadSlot(world, host, slot);
	scripting::IScriptBackend* backend = Backend(world);
	if (id.empty() || backend == nullptr) {
		return;
	}
	if (!host.host.is_alive()) {
		host.host = world.entity();
	}
	const std::string path = ScriptPath(world, id);
	LoadedScript script{.id = std::string{id}};
	script.instance = backend->CreateInstanceFromFile(path, host.host);
	auto& log = ecs::WorldLog(world);
	if (script.instance == nullptr) {
		log.error("[Campaign] {} script '{}' ('{}') didn't load, skipped", kSlotNames[static_cast<size_t>(slot)], id, path);
		return;
	}
	const HandlerReport report = AnalyzeHandlers(script.instance->FunctionDeclarations());
	for (const std::string& name : report.defined) {
		script.handles[static_cast<size_t>(EventIndex(name))] = true;
	}
	for (const std::string& declaration : report.unmatched) {
		log.warn("[Campaign] script '{}': '{}' matches no campaign event, so it never runs", id, declaration);
	}
	host.slots[static_cast<size_t>(slot)] = std::move(script);
	RefreshActive(world, host);
}

// Calls `slot`'s handler for event `index`, if it has one. An exception disables the script.
void DispatchTo(
	const flecs::world& world, CampaignScriptHost& host, const int slot, const int index, const std::span<const scripting::ScriptArg> args
) {
	LoadedScript& script = host.slots[static_cast<size_t>(slot)];
	if (script.instance == nullptr || script.dead || !script.handles[static_cast<size_t>(index)]) {
		return;
	}
	host.dispatching = slot;
	std::string error;
	const auto result = script.instance->CallFunction(kEvents[static_cast<size_t>(index)].declaration, args, &error);
	host.dispatching = -1;
	if (result == scripting::ScriptCallResult::Failed) {
		script.dead = true;
		ecs::WorldLog(world).error("[Campaign] script '{}' disabled: {} failed: {}", script.id, kEvents[static_cast<size_t>(index)].name, error);
		RefreshActive(world, host);
	}
}

// What a handler's call does, then refuses or not. Called outside a handler, a call is ignored.
void QueueCall(flecs::world& world, ScriptCall call) {
	auto& host = world.get_mut<CampaignScriptHost>();
	if (host.dispatching < 0) {
		ecs::WorldLog(world).warn("[Campaign] {} called outside a campaign event handler, ignored", call.label);
		return;
	}
	call.label = std::format("{}: {}", host.slots[static_cast<size_t>(host.dispatching)].id, call.label);
	host.queue.push_back(std::move(call));
}

// The reason a Spawn() is refused, or an empty string after throwing the enemies.
std::string_view SpawnForScript(const flecs::world& world, const ScriptCall& call) {
	const auto& run = world.get<RunState>();
	if (!IsRunning(run.phase)) {
		if (IsMolting(run.phase, run.step)) {
			return "in a molt";
		}
		return run.phase == RunPhase::Transition ? "between Acts" : "the run isn't live";
	}
	const auto& types = world.get<EnemyRegistry>().types;
	const auto type = std::ranges::find(types, call.text, &EnemyType::id);
	if (type == types.end()) {
		return "unknown enemy type";
	}
	if (std::ranges::find(SpawnerIds(), call.source) == SpawnerIds().end()) {
		return "unknown spawner";
	}
	if (call.count <= 0) {
		return "count must be positive";
	}
	ThrowEnemies(world, static_cast<int>(type - types.begin()), call.count);
	return {};
}

// Applies one queued call, returning why it was refused or an empty string.
std::string_view ApplyCall(const flecs::world& world, const ScriptCall& call) {
	switch (call.kind) {
	case CallKind::EndStage: return EndStageEarly(world);
	case CallKind::SpawnBoss: return CallBossNow(world, call.text);
	case CallKind::Spawn: return SpawnForScript(world, call);
	case CallKind::Banner: {
		auto& scripts = world.get_mut<CampaignScripts>();
		scripts.banner = call.text;
		auto& cooldown = world.lookup(kScriptBannerName).get_mut<timer::Cooldown>();
		cooldown.duration = constants::kScriptBannerTime;
		cooldown.remaining = constants::kScriptBannerTime;
		return {};
	}
	case CallKind::Sting: {
		const std::optional<Sfx> sfx = FindSfx(call.text);
		if (!sfx) {
			return "unknown sting";
		}
		PlaySfx(world, *sfx);
		return {};
	}
	}
	return {};
}

// Applies the queue in call order. Applying can fire events whose handlers queue more, so it goes round again.
void ApplyQueue(const flecs::world& world) {
	auto& host = world.get_mut<CampaignScriptHost>();
	for (int round = 0; round < constants::kMaxScriptApplyRounds && !host.queue.empty(); ++round) {
		std::vector<ScriptCall> calls = std::move(host.queue);
		host.queue.clear();
		for (const ScriptCall& call : calls) {
			const std::string_view refusal = ApplyCall(world, call);
			auto& run = world.get_mut<RunState>();
			if (refusal.empty()) {
				run.last_script_call = call.label;
				continue;
			}
			run.last_script_call = std::format("{} (refused: {})", call.label, refusal);
			ecs::WorldLog(world).warn("[Campaign] {} refused: {}", call.label, refusal);
		}
	}
}

// Reads a Remote API argument per its kind: a string as is, an int parsed (0 if it isn't one).
scripting::ScriptArg ParseArg(const char kind, const std::string& text) {
	if (kind != 'i') {
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
	const int index = EventIndex(event);
	if (index < 0) {
		ecs::WorldLog(world).warn("[Campaign] fire_event '{}' is not a campaign event", event);
		return;
	}
	const std::string_view kinds = kEvents[static_cast<size_t>(index)].argKinds;
	std::vector<scripting::ScriptArg> args;
	const std::array<const std::string*, 2> raw{&scripts.fire_arg0, &scripts.fire_arg1};
	for (size_t i = 0; i < kinds.size(); ++i) {
		args.push_back(ParseArg(kinds[i], *raw[i]));
	}
	FireCampaignEvent(world, event, args);
}

void FireTimers(const flecs::world& world, const float dt) {
	auto& host = world.get_mut<CampaignScriptHost>();
	std::vector<ScriptTimer> due;
	for (ScriptTimer& timer : host.timers) {
		timer.left -= dt;
	}
	std::erase_if(host.timers, [&due](const ScriptTimer& timer) {
		if (timer.left > 0.0F) {
			return false;
		}
		due.push_back(timer);
		return true;
	});
	for (const ScriptTimer& timer : due) {
		const std::array<scripting::ScriptArg, 1> args{timer.name};
		DispatchTo(world, host, timer.slot, kOnTimer, args);
	}
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
		QueueCall(w, {.kind = CallKind::EndStage, .label = "EndStage()"});
	});
	reg({.name = "SpawnBoss", .return_type = ScriptValueType::MakeVoid(), .params = {text("id")}}, [](ScriptCallContext& ctx, flecs::world& w) {
		const std::string id = ctx.GetArgString(0);
		QueueCall(w, {.kind = CallKind::SpawnBoss, .label = std::format("SpawnBoss(\"{}\")", id), .text = id});
	});
	reg({.name = "Spawn", .return_type = ScriptValueType::MakeVoid(), .params = {text("type"), number("count"), text("source")}},
		[](ScriptCallContext& ctx, flecs::world& w) {
			ScriptCall call{.kind = CallKind::Spawn, .text = ctx.GetArgString(0), .source = ctx.GetArgString(2), .count = ctx.GetArgInt(1)};
			call.label = std::format("Spawn(\"{}\", {}, \"{}\")", call.text, call.count, call.source);
			QueueCall(w, std::move(call));
		});
	reg({.name = "Banner", .return_type = ScriptValueType::MakeVoid(), .params = {text("text")}}, [](ScriptCallContext& ctx, flecs::world& w) {
		const std::string banner = ctx.GetArgString(0);
		QueueCall(w, {.kind = CallKind::Banner, .label = std::format("Banner(\"{}\")", banner), .text = banner});
	});
	reg({.name = "Sting", .return_type = ScriptValueType::MakeVoid(), .params = {text("id")}}, [](ScriptCallContext& ctx, flecs::world& w) {
		const std::string id = ctx.GetArgString(0);
		QueueCall(w, {.kind = CallKind::Sting, .label = std::format("Sting(\"{}\")", id), .text = id});
	});
	// After() isn't world-changing, so it registers at once, for the script that called it.
	reg({.name = "After", .return_type = ScriptValueType::MakeVoid(), .params = {seconds("seconds"), text("name")}},
		[](ScriptCallContext& ctx, flecs::world& w) {
			auto& host = w.get_mut<CampaignScriptHost>();
			const float delay = ctx.GetArgFloat(0);
			const std::string name = ctx.GetArgString(1);
			if (host.dispatching < 0 || delay <= 0.0F || static_cast<int>(host.timers.size()) >= constants::kMaxScriptTimers) {
				ecs::WorldLog(w).warn("[Campaign] After({}, \"{}\") refused: outside a handler, a non-positive delay or too many timers", delay, name);
				return;
			}
			host.timers.push_back({.left = delay, .name = name, .slot = host.dispatching});
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

std::span<const CampaignEventSpec> CampaignEvents() {
	return kEvents;
}

HandlerReport AnalyzeHandlers(const std::span<const std::string> declarations) {
	HandlerReport report;
	for (const std::string& declaration : declarations) {
		const std::string flat = StripSpaces(declaration);
		const auto match = std::ranges::find_if(kEvents, [&flat](const CampaignEventSpec& spec) { return StripSpaces(spec.declaration) == flat; });
		if (match != kEvents.end()) {
			report.defined.emplace_back(match->name);
			continue;
		}
		// The function's name is the last word before its parameter list.
		const std::string_view head = std::string_view{declaration}.substr(0, declaration.find('('));
		if (head.substr(head.rfind(' ') + 1).starts_with("On")) {
			report.unmatched.push_back(declaration);
		}
	}
	return report;
}

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
	auto& host = world.get_mut<CampaignScriptHost>();
	for (int slot = 0; slot < static_cast<int>(host.slots.size()); ++slot) {
		UnloadSlot(world, host, slot);
	}
	host.queue.clear();
	host.timers.clear();
	host.dispatching = -1;
}

void LoadActScript(const flecs::world& world, const std::string_view id) {
	LoadSlot(world, kActSlot, id);
}

void LoadStageScript(const flecs::world& world, const std::string_view id) {
	LoadSlot(world, kStageSlot, id);
}

bool StageScriptAlive(const flecs::world& world) {
	const LoadedScript& script = world.get<CampaignScriptHost>().slots[kStageSlot];
	return script.instance != nullptr && !script.dead;
}

void EndActScripts(const flecs::world& world) {
	FireCampaignEvent(world, "OnStageEnd", {std::string{"boss"}});
	auto& host = world.get_mut<CampaignScriptHost>();
	UnloadSlot(world, host, kStageSlot);
	UnloadSlot(world, host, kActSlot);
}

void FireCampaignEvent(const flecs::world& world, const std::string_view event, const std::span<const scripting::ScriptArg> args) {
	const int index = EventIndex(event);
	if (index < 0) {
		ecs::WorldLog(world).warn("[Campaign] '{}' is not a campaign event", event);
		return;
	}
	auto& host = world.get_mut<CampaignScriptHost>();
	for (int slot = 0; slot < static_cast<int>(host.slots.size()); ++slot) {
		DispatchTo(world, host, slot, index, args);
	}
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
		FireTimers(world, dt);
	}
	ApplyQueue(world);
}

} // namespace crabnado
