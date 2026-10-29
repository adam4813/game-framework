#include "dialog_module.hpp"

#include <memory>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "engine/engine.hpp"

#include "dialog_components.hpp"
#include "dialog_runtime.hpp"
#include "dialog_ui.hpp"

namespace engine::dialog {

namespace {

// -----------------------------------------------------------------------------
// Default node-type handlers (Strategy). Games may override any of these (e.g.
// give_item/take_item) via RegisterDialogNodeHandler.
// -----------------------------------------------------------------------------

void RegisterDefaultHandlers(const flecs::world& world) {
	auto map = world.get<DialogNodeHandlers>().map;

	// line / choice: show the box and wait for the player.
	const DialogNodeHandler show = [](const DialogRunContext& ctx) -> NodeResult {
		ctx.world.set<DialogActive>({.runner = ctx.runner});
		return {.flow = DialogFlow::Wait};
	};
	(*map)["line"] = show;
	(*map)["choice"] = show;

	// move: tween a subject entity to a world position.
	(*map)["move"] = [](const DialogRunContext& ctx) -> NodeResult {
		if (!ctx.node.has<DialogMove>()) return {.flow = DialogFlow::Advance};
		const auto& mv = ctx.node.get<DialogMove>();
		flecs::entity subject = ctx.node.target<DialogSubject>();
		if (!subject.is_valid()) subject = ctx.actor;
		if (!subject.is_valid() || !subject.has<spatial::Transform>()) {
			spdlog::warn("[Dialog] move node has no subject with a Transform — skipping");
			return {.flow = DialogFlow::Advance};
		}
		const glm::vec3 from = subject.get<spatial::Transform>().position;
		const float dist = glm::length(mv.to - from);
		const float duration = mv.speed > 0.0F ? dist / mv.speed : 0.0F;
		subject.set<DialogTween>(
			{.from = from,
			 .to = mv.to,
			 .elapsed = 0.0F,
			 .duration = duration,
			 .runner = ctx.runner,
			 .resolve = mv.wait_for_arrival}
		);
		return {.flow = mv.wait_for_arrival ? DialogFlow::Wait : DialogFlow::Advance};
	};

	// follow: install an ongoing follow behaviour on the subject (does not block).
	(*map)["follow"] = [](const DialogRunContext& ctx) -> NodeResult {
		const flecs::entity follower = ctx.node.target<DialogSubject>();
		if (!follower.is_valid()) {
			spdlog::warn("[Dialog] follow node has no DialogSubject — skipping");
			return {.flow = DialogFlow::Advance};
		}
		const DialogFollow cfg = ctx.node.has<DialogFollow>() ? ctx.node.get<DialogFollow>() : DialogFollow{};
		follower.set<DialogFollower>({.leader = ctx.actor, .speed = cfg.speed, .stop_distance = cfg.stop_distance});
		return {.flow = DialogFlow::Advance};
	};

	// wait: pause the interaction for a fixed duration.
	(*map)["wait"] = [](const DialogRunContext& ctx) -> NodeResult {
		const float seconds = ctx.node.has<DialogWait>() ? ctx.node.get<DialogWait>().seconds : 0.0F;
		ctx.runner.set<DialogTimer>({.remaining = seconds, .runner = ctx.runner});
		return {.flow = DialogFlow::Wait};
	};

	// set_flag: mutate a story flag, then advance.
	(*map)["set_flag"] = [](const DialogRunContext& ctx) -> NodeResult {
		if (ctx.node.has<DialogSetFlag>()) {
			const auto& sf = ctx.node.get<DialogSetFlag>();
			SetDialogFlag(ctx.world, sf.flag, sf.value);
		}
		return {.flow = DialogFlow::Advance};
	};

	// condition: branch on a story flag.
	(*map)["condition"] = [](const DialogRunContext& ctx) -> NodeResult {
		const bool value =
			ctx.node.has<DialogCondition>() && GetDialogFlag(ctx.world, ctx.node.get<DialogCondition>().flag);
		flecs::entity chosen;
		ctx.node.each<graph::GraphEdge>([&](const flecs::entity child) {
			if (!chosen.is_valid() && child.has<DialogBranch>() && child.get<DialogBranch>().on == value)
				chosen = child;
		});
		if (chosen.is_valid()) return {.flow = DialogFlow::Branch, .next = chosen};
		return {.flow = DialogFlow::Advance}; // no matching branch: fall through
	};

	// end: finish the interaction.
	(*map)["end"] = [](const DialogRunContext&) -> NodeResult { return {.flow = DialogFlow::End}; };

	// give/take item + coins: no engine inventory — log and advance. Games override these.
	(*map)["give_item"] = [](const DialogRunContext& ctx) -> NodeResult {
		if (ctx.node.has<DialogGiveItem>()) {
			const auto& g = ctx.node.get<DialogGiveItem>();
			spdlog::info("[Dialog] give_item '{}' x{} (no handler registered)", g.item, g.count);
		}
		return {.flow = DialogFlow::Advance};
	};
	(*map)["take_item"] = [](const DialogRunContext& ctx) -> NodeResult {
		if (ctx.node.has<DialogGiveItem>()) {
			const auto& g = ctx.node.get<DialogGiveItem>();
			spdlog::info("[Dialog] take_item '{}' x{} (no handler registered)", g.item, g.count);
		}
		return {.flow = DialogFlow::Advance};
	};
	(*map)["give_coins"] = [](const DialogRunContext& ctx) -> NodeResult {
		if (ctx.node.has<DialogGiveCoins>())
			spdlog::info("[Dialog] give_coins {} (no handler registered)", ctx.node.get<DialogGiveCoins>().amount);
		return {.flow = DialogFlow::Advance};
	};
	(*map)["take_coins"] = [](const DialogRunContext& ctx) -> NodeResult {
		if (ctx.node.has<DialogGiveCoins>())
			spdlog::info("[Dialog] take_coins {} (no handler registered)", ctx.node.get<DialogGiveCoins>().amount);
		return {.flow = DialogFlow::Advance};
	};
}

// -----------------------------------------------------------------------------
// JSON authoring — one loader per payload; each also stamps DialogNode.type.
// Reuses the graph module's "graph_root"/"graph_node"/"graph_edge" loaders for topology.
// -----------------------------------------------------------------------------

void RegisterJsonLoaders(const flecs::world& world) {
	level::RegisterComponentLoader(world, "dialog_node", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<DialogNode>({.type = j.value("type", std::string{"line"})});
	});
	level::RegisterComponentLoader(world, "dialog_line", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<DialogLine>(
			{.speaker = j.value("speaker", std::string{}),
			 .text = j.value("text", std::string{}),
			 .portrait = j.value("portrait", std::string{})}
		);
		e.set<DialogNode>({.type = j.value("type", std::string{"line"})});
	});
	level::RegisterComponentLoader(world, "dialog_choice", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<DialogChoice>(
			{.label = j.value("label", std::string{}),
			 .requires_flag = j.value("requires_flag", std::string{}),
			 .order = j.value("order", 0)}
		);
	});
	level::RegisterComponentLoader(world, "dialog_move", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<DialogMove>(
			{.to = core::JVec3(j, "to", glm::vec3{0.0F}),
			 .speed = j.value("speed", 4.0F),
			 .wait_for_arrival = j.value("wait_for_arrival", true)}
		);
		e.set<DialogNode>({.type = "move"});
	});
	level::RegisterComponentLoader(world, "dialog_follow", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<DialogFollow>({.speed = j.value("speed", 3.0F), .stop_distance = j.value("stop_distance", 1.0F)});
		e.set<DialogNode>({.type = "follow"});
	});
	level::RegisterComponentLoader(world, "dialog_give_item", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<DialogGiveItem>({.item = j.value("item", std::string{}), .count = j.value("count", 1)});
		e.set<DialogNode>({.type = j.value("take", false) ? "take_item" : "give_item"});
	});
	level::RegisterComponentLoader(world, "dialog_give_coins", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<DialogGiveCoins>({.amount = j.value("amount", 1)});
		e.set<DialogNode>({.type = j.value("take", false) ? "take_coins" : "give_coins"});
	});
	level::RegisterComponentLoader(world, "dialog_wait", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<DialogWait>({.seconds = j.value("seconds", 1.0F)});
		e.set<DialogNode>({.type = "wait"});
	});
	level::RegisterComponentLoader(world, "dialog_condition", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<DialogCondition>({.flag = j.value("flag", std::string{})});
		e.set<DialogNode>({.type = "condition"});
	});
	level::RegisterComponentLoader(world, "dialog_branch", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<DialogBranch>({.on = j.value("on", true)});
	});
	level::RegisterComponentLoader(world, "dialog_set_flag", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<DialogSetFlag>({.flag = j.value("flag", std::string{}), .value = j.value("value", true)});
		e.set<DialogNode>({.type = "set_flag"});
	});
	level::RegisterComponentLoader(world, "dialog_end", [](const flecs::entity e, const nlohmann::json&) {
		e.add<DialogEnd>();
		e.set<DialogNode>({.type = "end"});
	});

	level::RegisterRef(world, "dialog_subject", [](const flecs::entity self, const flecs::entity target) {
		self.add<DialogSubject>(target);
	});
	level::RegisterRef(world, "dialog_target", [](const flecs::entity self, const flecs::entity target) {
		self.add<DialogTarget>(target);
	});
}

// -----------------------------------------------------------------------------
// Scripting authoring — global builders + runtime control. Node entities are children of the
// passed tree, so destroying a tree frees its nodes.
// -----------------------------------------------------------------------------

void RegisterScripting(const flecs::world& world) {
	if (!world.has<scripting::ScriptBackendSingleton>()) return;
	using namespace scripting;

	const auto entity_type = world.component<ScriptEntityRef>();
	const auto P_ent = [&](const char* n) {
		return ScriptMethodParam{.type = ScriptValueType::MakeObject(entity_type), .by_reference = false, .name = n};
	};
	const auto P_str = [](const char* n) {
		return ScriptMethodParam{.type = ScriptValueType::MakeString(), .by_reference = true, .name = n};
	};
	const auto P_int = [](const char* n) {
		return ScriptMethodParam{.type = ScriptValueType::MakeInt(), .by_reference = false, .name = n};
	};
	const auto P_flt = [](const char* n) {
		return ScriptMethodParam{.type = ScriptValueType::MakeFloat(), .by_reference = false, .name = n};
	};
	const auto P_bool = [](const char* n) {
		return ScriptMethodParam{.type = ScriptValueType::MakeBool(), .by_reference = false, .name = n};
	};
	const auto arg_ent = [](const ScriptCallContext& ctx, const int i) {
		auto* r = static_cast<ScriptEntityRef*>(ctx.GetArgObject(i));
		return r ? r->GetEntity() : flecs::entity{};
	};
	const auto return_ent = [](ScriptCallContext& ctx, const flecs::entity e) {
		new (ctx.GetReturnObjectLocation()) ScriptEntityRef{e};
	};

	// CreateDialogTree(name) -> Entity
	RegisterGlobalFunctionForScripts(
		world,
		{.name = "CreateDialogTree",
		 .return_type = ScriptValueType::MakeObject(entity_type),
		 .params = {P_str("name")}},
		[return_ent](ScriptCallContext& ctx, const flecs::world& w) {
			const flecs::entity e = w.entity(ctx.GetArgString(0).c_str());
			e.add<graph::GraphRoot>();
			e.set<graph::GraphNode>({.label = ctx.GetArgString(0)});
			return_ent(ctx, e);
		}
	);

	// AddDialogNode(tree, name) -> Entity
	RegisterGlobalFunctionForScripts(
		world,
		{.name = "AddDialogNode",
		 .return_type = ScriptValueType::MakeObject(entity_type),
		 .params = {P_ent("tree"), P_str("name")}},
		[arg_ent, return_ent](ScriptCallContext& ctx, const flecs::world& w) {
			const flecs::entity tree = arg_ent(ctx, 0);
			const flecs::entity e = w.entity(ctx.GetArgString(1).c_str());
			if (tree.is_valid()) {
				e.child_of(tree);
			}
			e.set<graph::GraphNode>({.label = ctx.GetArgString(1)});
			return_ent(ctx, e);
		}
	);

	// ConnectDialogNodes(parent, child)
	RegisterGlobalFunctionForScripts(
		world,
		{.name = "ConnectDialogNodes",
		 .return_type = ScriptValueType::MakeVoid(),
		 .params = {P_ent("parent"), P_ent("child")}},
		[arg_ent](const ScriptCallContext& ctx, flecs::world&) {
			const flecs::entity p = arg_ent(ctx, 0);
			const flecs::entity c = arg_ent(ctx, 1);
			if (p.is_valid() && c.is_valid()) {
				p.add<graph::GraphEdge>(c);
			}
		}
	);

	// SetDialogLine(node, speaker, text, portrait)
	RegisterGlobalFunctionForScripts(
		world,
		{.name = "SetDialogLine",
		 .return_type = ScriptValueType::MakeVoid(),
		 .params = {P_ent("node"), P_str("speaker"), P_str("text"), P_str("portrait")}},
		[arg_ent](const ScriptCallContext& ctx, flecs::world&) {
			const flecs::entity n = arg_ent(ctx, 0);
			if (!n.is_valid()) return;
			n.set<DialogLine>(
				{.speaker = ctx.GetArgString(1), .text = ctx.GetArgString(2), .portrait = ctx.GetArgString(3)}
			);
			n.set<DialogNode>({.type = "line"});
		}
	);

	// SetDialogChoice(child, label, order, requires_flag)
	RegisterGlobalFunctionForScripts(
		world,
		{.name = "SetDialogChoice",
		 .return_type = ScriptValueType::MakeVoid(),
		 .params = {P_ent("child"), P_str("label"), P_int("order"), P_str("requires_flag")}},
		[arg_ent](const ScriptCallContext& ctx, flecs::world&) {
			const flecs::entity c = arg_ent(ctx, 0);
			if (!c.is_valid()) return;
			c.set<DialogChoice>(
				{.label = ctx.GetArgString(1), .requires_flag = ctx.GetArgString(3), .order = ctx.GetArgInt(2)}
			);
		}
	);

	// SetDialogMove(node, x, y, z, speed, wait)
	RegisterGlobalFunctionForScripts(
		world,
		{.name = "SetDialogMove",
		 .return_type = ScriptValueType::MakeVoid(),
		 .params = {P_ent("node"), P_flt("x"), P_flt("y"), P_flt("z"), P_flt("speed"), P_bool("wait")}},
		[arg_ent](const ScriptCallContext& ctx, flecs::world&) {
			const flecs::entity n = arg_ent(ctx, 0);
			if (!n.is_valid()) return;
			n.set<DialogMove>(
				{.to = {ctx.GetArgFloat(1), ctx.GetArgFloat(2), ctx.GetArgFloat(3)},
				 .speed = ctx.GetArgFloat(4),
				 .wait_for_arrival = ctx.GetArgBool(5)}
			);
			n.set<DialogNode>({.type = "move"});
		}
	);

	// SetDialogSubject(node, subject)
	RegisterGlobalFunctionForScripts(
		world,
		{.name = "SetDialogSubject",
		 .return_type = ScriptValueType::MakeVoid(),
		 .params = {P_ent("node"), P_ent("subject")}},
		[arg_ent](const ScriptCallContext& ctx, flecs::world&) {
			const flecs::entity n = arg_ent(ctx, 0);
			const flecs::entity s = arg_ent(ctx, 1);
			if (n.is_valid() && s.is_valid()) {
				n.add<DialogSubject>(s);
			}
		}
	);

	// SetDialogFollow(node, speed, stop_distance) — the node's DialogSubject follows the actor.
	RegisterGlobalFunctionForScripts(
		world,
		{.name = "SetDialogFollow",
		 .return_type = ScriptValueType::MakeVoid(),
		 .params = {P_ent("node"), P_flt("speed"), P_flt("stop_distance")}},
		[arg_ent](const ScriptCallContext& ctx, flecs::world&) {
			const flecs::entity n = arg_ent(ctx, 0);
			if (!n.is_valid()) return;
			n.set<DialogFollow>({.speed = ctx.GetArgFloat(1), .stop_distance = ctx.GetArgFloat(2)});
			n.set<DialogNode>({.type = "follow"});
		}
	);

	// SetDialogGiveItem(node, item, count, take)
	RegisterGlobalFunctionForScripts(
		world,
		{.name = "SetDialogGiveItem",
		 .return_type = ScriptValueType::MakeVoid(),
		 .params = {P_ent("node"), P_str("item"), P_int("count"), P_bool("take")}},
		[arg_ent](const ScriptCallContext& ctx, flecs::world&) {
			const flecs::entity n = arg_ent(ctx, 0);
			if (!n.is_valid()) return;
			n.set<DialogGiveItem>({.item = ctx.GetArgString(1), .count = ctx.GetArgInt(2)});
			n.set<DialogNode>({.type = ctx.GetArgBool(3) ? "take_item" : "give_item"});
		}
	);

	// SetDialogGiveCoins(node, amount, take)
	RegisterGlobalFunctionForScripts(
		world,
		{.name = "SetDialogGiveCoins",
		 .return_type = ScriptValueType::MakeVoid(),
		 .params = {P_ent("node"), P_int("amount"), P_bool("take")}},
		[arg_ent](const ScriptCallContext& ctx, flecs::world&) {
			const flecs::entity n = arg_ent(ctx, 0);
			if (!n.is_valid()) return;
			n.set<DialogGiveCoins>({.amount = ctx.GetArgInt(1)});
			n.set<DialogNode>({.type = ctx.GetArgBool(2) ? "take_coins" : "give_coins"});
		}
	);

	// SetDialogWait(node, seconds)
	RegisterGlobalFunctionForScripts(
		world,
		{.name = "SetDialogWait",
		 .return_type = ScriptValueType::MakeVoid(),
		 .params = {P_ent("node"), P_flt("seconds")}},
		[arg_ent](const ScriptCallContext& ctx, flecs::world&) {
			const flecs::entity n = arg_ent(ctx, 0);
			if (!n.is_valid()) return;
			n.set<DialogWait>({.seconds = ctx.GetArgFloat(1)});
			n.set<DialogNode>({.type = "wait"});
		}
	);

	// SetDialogCondition(node, flag)
	RegisterGlobalFunctionForScripts(
		world,
		{.name = "SetDialogCondition",
		 .return_type = ScriptValueType::MakeVoid(),
		 .params = {P_ent("node"), P_str("flag")}},
		[arg_ent](const ScriptCallContext& ctx, flecs::world&) {
			const flecs::entity n = arg_ent(ctx, 0);
			if (!n.is_valid()) return;
			n.set<DialogCondition>({.flag = ctx.GetArgString(1)});
			n.set<DialogNode>({.type = "condition"});
		}
	);

	// SetDialogBranch(child, on)
	RegisterGlobalFunctionForScripts(
		world,
		{.name = "SetDialogBranch",
		 .return_type = ScriptValueType::MakeVoid(),
		 .params = {P_ent("child"), P_bool("on")}},
		[arg_ent](const ScriptCallContext& ctx, flecs::world&) {
			const flecs::entity c = arg_ent(ctx, 0);
			if (c.is_valid()) {
				c.set<DialogBranch>({.on = ctx.GetArgBool(1)});
			}
		}
	);

	// SetDialogSetFlag(node, flag, value)
	RegisterGlobalFunctionForScripts(
		world,
		{.name = "SetDialogSetFlag",
		 .return_type = ScriptValueType::MakeVoid(),
		 .params = {P_ent("node"), P_str("flag"), P_bool("value")}},
		[arg_ent](const ScriptCallContext& ctx, flecs::world&) {
			const flecs::entity n = arg_ent(ctx, 0);
			if (!n.is_valid()) return;
			n.set<DialogSetFlag>({.flag = ctx.GetArgString(1), .value = ctx.GetArgBool(2)});
			n.set<DialogNode>({.type = "set_flag"});
		}
	);

	// SetDialogEnd(node)
	RegisterGlobalFunctionForScripts(
		world,
		{.name = "SetDialogEnd", .return_type = ScriptValueType::MakeVoid(), .params = {P_ent("node")}},
		[arg_ent](const ScriptCallContext& ctx, flecs::world&) {
			const flecs::entity n = arg_ent(ctx, 0);
			if (!n.is_valid()) return;
			n.add<DialogEnd>();
			n.set<DialogNode>({.type = "end"});
		}
	);

	// StartDialog(root, actor)
	RegisterGlobalFunctionForScripts(
		world,
		{.name = "StartDialog", .return_type = ScriptValueType::MakeVoid(), .params = {P_ent("root"), P_ent("actor")}},
		[arg_ent](const ScriptCallContext& ctx, const flecs::world& w) {
			const flecs::entity root = arg_ent(ctx, 0);
			const flecs::entity actor = arg_ent(ctx, 1);
			if (root.is_valid()) {
				StartDialog(w, root, actor);
			}
		}
	);

	// SetDialogFlag(flag, value) / GetDialogFlag(flag)
	RegisterGlobalFunctionForScripts(
		world,
		{.name = "SetDialogFlag",
		 .return_type = ScriptValueType::MakeVoid(),
		 .params = {P_str("flag"), P_bool("value")}},
		[](const ScriptCallContext& ctx, const flecs::world& w) {
			SetDialogFlag(w, ctx.GetArgString(0), ctx.GetArgBool(1));
		}
	);
	RegisterGlobalFunctionForScripts(
		world,
		{.name = "GetDialogFlag", .return_type = ScriptValueType::MakeBool(), .params = {P_str("flag")}},
		[](ScriptCallContext& ctx, const flecs::world& w) { ctx.SetReturnBool(GetDialogFlag(w, ctx.GetArgString(0))); }
	);

	// SetDialogNodeHandler(type, @fn) — bind a script action callback to a node type. The
	// callback runs the effect and the runner auto-advances. Signature: void fn(Entity actor, Entity node).
	RegisterCallbackFunctionForScripts(
		world,
		{.name = "SetDialogNodeHandler",
		 .fixed_params = {P_str("type")},
		 .callback = {.funcdef_name = "DialogActionCallback", .params = {P_ent("actor"), P_ent("node")}, .name = "fn"}},
		[](const ScriptCallContext& ctx, const flecs::world& w, ScriptFunctionHandle* handle) {
			const std::string type = ctx.GetArgString(0);
			if (!handle) {
				RegisterDialogNodeHandler(w, type, {});
				return;
			}
			handle->AddRef();
			auto sp = std::shared_ptr<ScriptFunctionHandle>(handle, [](ScriptFunctionHandle* h) {
				if (h) {
					h->Release();
				}
			});
			RegisterDialogNodeHandler(w, type, [sp](const DialogRunContext& rc) -> NodeResult {
				ScriptEntityRef actor{rc.actor};
				ScriptEntityRef node{rc.node};
				const void* args[] = {&actor, &node};
				sp->Invoke(args, 2);
				return {.flow = DialogFlow::Advance};
			});
		}
	);
}

} // namespace

DialogModule::DialogModule(const flecs::world& world) {
	// (1) Singletons
	world.set<DialogFlags>({});
	world.set<DialogNodeHandlers>({});
	world.set<DialogUI>({});

	// (2) Reflection — plain-data payloads. Runtime components hold flecs::entity handles (not
	// meta-reflectable) and are registered without members.
	world.component<DialogNode>().member("type", &DialogNode::type);
	world.component<DialogLine>()
		.member("speaker", &DialogLine::speaker)
		.member("text", &DialogLine::text)
		.member("portrait", &DialogLine::portrait);
	world.component<DialogChoice>()
		.member("label", &DialogChoice::label)
		.member("requires_flag", &DialogChoice::requires_flag)
		.member("order", &DialogChoice::order);
	world.component<DialogMove>()
		.member("to", &DialogMove::to)
		.member("speed", &DialogMove::speed)
		.member("wait_for_arrival", &DialogMove::wait_for_arrival);
	world.component<DialogFollow>()
		.member("speed", &DialogFollow::speed)
		.member("stop_distance", &DialogFollow::stop_distance);
	world.component<DialogGiveItem>().member("item", &DialogGiveItem::item).member("count", &DialogGiveItem::count);
	world.component<DialogGiveCoins>().member("amount", &DialogGiveCoins::amount);
	world.component<DialogWait>().member("seconds", &DialogWait::seconds);
	world.component<DialogCondition>().member("flag", &DialogCondition::flag);
	world.component<DialogBranch>().member("on", &DialogBranch::on);
	world.component<DialogSetFlag>().member("flag", &DialogSetFlag::flag).member("value", &DialogSetFlag::value);
	world.component<DialogEnd>();
	world.component<DialogSubject>().add(flecs::Exclusive);
	world.component<DialogTarget>().add(flecs::Exclusive);
	world.component<DialogRunner>();
	world.component<DialogTimer>();
	world.component<DialogTween>();
	world.component<DialogFollower>();

	// (3) Default node handlers (Strategy registry)
	RegisterDefaultHandlers(world);

	// (4) Systems — drive wait/move nodes. Non-Pausable so cut-scenes advance while the scene is
	// paused; untagged so they run in every scene pipeline (they only match dialog runtime entities).
	world.system<DialogTimer>("DialogTimerSystem")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, DialogTimer& timer) {
			timer.remaining -= it.delta_time();
			if (timer.remaining <= 0.0F) {
				const flecs::entity runner = timer.runner;
				it.entity(i).remove<DialogTimer>();
				ResolveDialog(it.world(), runner, {});
			}
		});

	world.system<DialogTween, spatial::Transform>("DialogTweenSystem")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, DialogTween& tween, spatial::Transform& transform) {
			tween.elapsed += it.delta_time();
			const float t = tween.duration > 0.0F ? tween.elapsed / tween.duration : 1.0F;
			const float k = t >= 1.0F ? 1.0F : t;
			transform.position = glm::mix(tween.from, tween.to, k);
			if (t >= 1.0F) {
				const flecs::entity runner = tween.runner;
				const bool resolve = tween.resolve;
				it.entity(i).remove<DialogTween>();
				if (resolve) {
					ResolveDialog(it.world(), runner, {});
				}
			}
		});

	// Ongoing follow behaviour (installed by "follow" nodes). Pausable — a companion halts while the
	// game is paused (including during a conversation) and resumes with play.
	std::ignore =
		world.system<DialogFollower, spatial::Transform>("DialogFollowerSystem")
			.kind(flecs::OnUpdate)
			.each([](const flecs::iter& it, const size_t, const DialogFollower& follow, spatial::Transform& transform) {
				if (!follow.leader.is_valid() || !follow.leader.has<spatial::Transform>()) return;
				glm::vec3 delta = follow.leader.get<spatial::Transform>().position - transform.position;
				delta.y = 0.0F;
				if (const float dist = glm::length(delta); dist > follow.stop_distance) {
					auto move_dist = follow.speed * it.delta_time();
					// Clamp to stop_distance if the leader is very close (avoid overshoot).
					if (dist - move_dist < follow.stop_distance) {
						move_dist = dist - follow.stop_distance;
					}
					transform.position += (delta / dist) * move_dist;
				}
			})
			.add<ecs::Pausable>();

	// (5) Observers — build/tear down the dialog box as interactions gain/lose the UI.
	std::ignore = world.observer<DialogActive>("DialogActiveBuildBox")
					  .event(flecs::OnSet)
					  .each([](const flecs::iter& it, size_t, DialogActive&) { BuildDialogBox(it.world()); });
	std::ignore = world.observer<DialogActive>("DialogActiveDestroyBox")
					  .event(flecs::OnRemove)
					  .each([](const flecs::iter& it, size_t, DialogActive&) { DestroyDialogBox(it.world()); });

	// (6) Authoring surfaces
	RegisterJsonLoaders(world);
	RegisterScripting(world);

	spdlog::info("[DialogModule] Registered interaction-tree components, handlers, systems, and authoring bindings");
}

} // namespace engine::dialog
