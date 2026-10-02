#include "ui_scripting.hpp"

#include <new>
#include <string>
#include <utility>
#include <vector>

#include <flecs.h>

#include "engine/scripting/scripting.hpp"
#include "ui_components.hpp"
#include "ui_factories.hpp"

namespace engine::ui {

void RegisterUIScriptFactories(const flecs::world& world) {
	using scripting::ScriptCallContext;
	using scripting::ScriptEntityRef;
	using scripting::ScriptMethodParam;
	using scripting::ScriptMethodSignature;
	using scripting::ScriptValueType;

	const ScriptValueType entity_type = ScriptValueType::MakeObject(world.component<ScriptEntityRef>());

	// Builds a signature `Entity <name>(Entity parent, float x, float y, float w, float h, <extra...>)`.
	const auto make_sig = [entity_type](const char* name, std::vector<ScriptMethodParam> extra) {
		ScriptMethodSignature sig;
		sig.name = name;
		sig.return_type = entity_type;
		sig.params = {
			{.type = entity_type, .by_reference = false, .name = "parent"},
			{.type = ScriptValueType::MakeFloat(), .by_reference = false, .name = "x"},
			{.type = ScriptValueType::MakeFloat(), .by_reference = false, .name = "y"},
			{.type = ScriptValueType::MakeFloat(), .by_reference = false, .name = "w"},
			{.type = ScriptValueType::MakeFloat(), .by_reference = false, .name = "h"},
		};
		for (auto& p : extra) {
			sig.params.push_back(std::move(p));
		}
		return sig;
	};

	// Shared arg readers: the parent handle (arg 0) and the rect (args 1-4).
	const auto parent_of = [](const ScriptCallContext& ctx) -> flecs::entity {
		const auto* ref = static_cast<ScriptEntityRef*>(ctx.GetArgObject(0));
		return ref != nullptr ? ref->GetEntity() : flecs::entity{};
	};
	const auto rect_of = [](const ScriptCallContext& ctx) {
		return core::Rect{
			.x = ctx.GetArgFloat(1),
			.y = ctx.GetArgFloat(2),
			.w = ctx.GetArgFloat(3),
			.h = ctx.GetArgFloat(4)
		};
	};
	// Parent (when valid) and return the created entity as the script's Entity return value.
	const auto finish = [](ScriptCallContext& ctx, const flecs::entity e, const flecs::entity parent) {
		if (parent) {
			e.child_of(parent);
		}
		new (ctx.GetReturnObjectLocation()) ScriptEntityRef(e);
	};

	const auto f = ScriptValueType::MakeFloat();
	const auto s = ScriptValueType::MakeString();
	const auto b = ScriptValueType::MakeBool();

	scripting::RegisterGlobalFunctionForScripts(
		world,
		make_sig("CreatePanel", {}),
		[parent_of, rect_of, finish](ScriptCallContext& ctx, const flecs::world& w) {
			finish(ctx, CreatePanel(w, rect_of(ctx)), parent_of(ctx));
		}
	);
	scripting::RegisterGlobalFunctionForScripts(
		world,
		make_sig(
			"CreateLabel",
			{{.type = s, .by_reference = false, .name = "text"},
			 {.type = f, .by_reference = false, .name = "font_size"}}
		),
		[parent_of, rect_of, finish](ScriptCallContext& ctx, const flecs::world& w) {
			finish(ctx, CreateLabel(w, rect_of(ctx), ctx.GetArgString(5), ctx.GetArgFloat(6)), parent_of(ctx));
		}
	);
	scripting::RegisterGlobalFunctionForScripts(
		world,
		make_sig("CreateButton", {{.type = s, .by_reference = false, .name = "label"}}),
		[parent_of, rect_of, finish](ScriptCallContext& ctx, const flecs::world& w) {
			finish(ctx, CreateButton(w, rect_of(ctx), ctx.GetArgString(5)), parent_of(ctx));
		}
	);
	scripting::RegisterGlobalFunctionForScripts(
		world,
		make_sig(
			"CreateCheckbox",
			{{.type = s, .by_reference = false, .name = "label"}, {.type = b, .by_reference = false, .name = "checked"}}
		),
		[parent_of, rect_of, finish](ScriptCallContext& ctx, const flecs::world& w) {
			finish(ctx, CreateCheckbox(w, rect_of(ctx), ctx.GetArgString(5), ctx.GetArgBool(6)), parent_of(ctx));
		}
	);
	scripting::RegisterGlobalFunctionForScripts(
		world,
		make_sig(
			"CreateRadio",
			{{.type = s, .by_reference = false, .name = "group"},
			 {.type = s, .by_reference = false, .name = "value"},
			 {.type = s, .by_reference = false, .name = "label"}}
		),
		[parent_of, rect_of, finish](ScriptCallContext& ctx, const flecs::world& w) {
			finish(
				ctx,
				CreateRadio(w, rect_of(ctx), ctx.GetArgString(5), ctx.GetArgString(6), ctx.GetArgString(7)),
				parent_of(ctx)
			);
		}
	);
	scripting::RegisterGlobalFunctionForScripts(
		world,
		make_sig("CreateTextInput", {{.type = s, .by_reference = false, .name = "placeholder"}}),
		[parent_of, rect_of, finish](ScriptCallContext& ctx, const flecs::world& w) {
			finish(ctx, CreateTextInput(w, rect_of(ctx), ctx.GetArgString(5)), parent_of(ctx));
		}
	);
	scripting::RegisterGlobalFunctionForScripts(
		world,
		make_sig("CreateTextArea", {{.type = s, .by_reference = false, .name = "placeholder"}}),
		[parent_of, rect_of, finish](ScriptCallContext& ctx, const flecs::world& w) {
			finish(ctx, CreateTextArea(w, rect_of(ctx), ctx.GetArgString(5)), parent_of(ctx));
		}
	);
	scripting::RegisterGlobalFunctionForScripts(
		world,
		make_sig(
			"CreateSlider",
			{{.type = f, .by_reference = false, .name = "value"},
			 {.type = f, .by_reference = false, .name = "min"},
			 {.type = f, .by_reference = false, .name = "max"}}
		),
		[parent_of, rect_of, finish](ScriptCallContext& ctx, const flecs::world& w) {
			finish(
				ctx,
				CreateSlider(w, rect_of(ctx), ctx.GetArgFloat(5), ctx.GetArgFloat(6), ctx.GetArgFloat(7)),
				parent_of(ctx)
			);
		}
	);
	scripting::RegisterGlobalFunctionForScripts(
		world,
		make_sig(
			"CreateProgressBar",
			{{.type = f, .by_reference = false, .name = "value"},
			 {.type = f, .by_reference = false, .name = "min"},
			 {.type = f, .by_reference = false, .name = "max"}}
		),
		[parent_of, rect_of, finish](ScriptCallContext& ctx, const flecs::world& w) {
			const ProgressBar bar{.value = ctx.GetArgFloat(5), .min = ctx.GetArgFloat(6), .max = ctx.GetArgFloat(7)};
			finish(ctx, CreateProgressBar(w, rect_of(ctx), bar), parent_of(ctx));
		}
	);
	scripting::RegisterGlobalFunctionForScripts(
		world,
		make_sig(
			"CreateStack",
			{{.type = b, .by_reference = false, .name = "horizontal"},
			 {.type = f, .by_reference = false, .name = "spacing"},
			 {.type = f, .by_reference = false, .name = "padding"}}
		),
		[parent_of, rect_of, finish](ScriptCallContext& ctx, const flecs::world& w) {
			const Stack stack{
				.direction = ctx.GetArgBool(5) ? StackDirection::Horizontal : StackDirection::Vertical,
				.spacing = ctx.GetArgFloat(6),
				.padding = ctx.GetArgFloat(7)
			};
			finish(ctx, CreateStack(w, rect_of(ctx), stack), parent_of(ctx));
		}
	);
	scripting::RegisterGlobalFunctionForScripts(
		world,
		make_sig("CreateSpinner", {}),
		[parent_of, rect_of, finish](ScriptCallContext& ctx, const flecs::world& w) {
			finish(ctx, CreateSpinner(w, rect_of(ctx)), parent_of(ctx));
		}
	);
}

} // namespace engine::ui
