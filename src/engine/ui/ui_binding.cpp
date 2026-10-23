#include "ui_binding.hpp"

#include <format>
#include <string>
#include <utility>

#include <flecs.h>
#include <spdlog/spdlog.h>

#include "engine/ai/ai.hpp"
#include "ui_components.hpp"

namespace engine::ui {

// ─────────────────────────────────────────────────────────────────────────────────────────────
// BindingValue conversions
// ─────────────────────────────────────────────────────────────────────────────────────────────

bool BindingValue::AsBool() const {
	switch (kind) {
	case Kind::Bool: return b;
	case Kind::Number: return number != 0.0;
	case Kind::Text: return text == "true" || text == "1";
	default: return false;
	}
}

double BindingValue::AsNumber() const {
	switch (kind) {
	case Kind::Number: return number;
	case Kind::Bool: return b ? 1.0 : 0.0;
	case Kind::Text: try { return std::stod(text);
		}
		catch (...) {
			return 0.0;
		}
	default: return 0.0;
	}
}

std::string BindingValue::AsText() const {
	switch (kind) {
	case Kind::Text: return text;
	case Kind::Bool: return b ? "true" : "false";
	case Kind::Number:
		if (number == std::floor(number) && std::abs(number) < 1e15) {
			return std::format("{}", static_cast<long long>(number));
		}
		return std::format("{:g}", number);
	default: return {};
	}
}

bool BindingValue::operator==(const BindingValue& other) const {
	if (kind != other.kind) {
		return false;
	}
	switch (kind) {
	case Kind::Bool: return b == other.b;
	case Kind::Number: return number == other.number;
	case Kind::Text: return text == other.text;
	default: return true;
	}
}

namespace {

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Widget value sink: read/write the owning entity's widget-natural value as a BindingValue.
// ─────────────────────────────────────────────────────────────────────────────────────────────

BindingValue GetWidgetValue(const flecs::entity e) {
	if (const auto* l = e.try_get<Label>()) return BindingValue::FromText(l->text);
	if (const auto* ti = e.try_get<TextInput>()) return BindingValue::FromText(ti->text);
	if (const auto* ta = e.try_get<TextArea>()) return BindingValue::FromText(ta->text);
	if (const auto* p = e.try_get<ProgressBar>()) return BindingValue::FromNumber(p->value);
	if (const auto* s = e.try_get<Slider>()) return BindingValue::FromNumber(s->value);
	if (const auto* dd = e.try_get<Dropdown>()) return BindingValue::FromNumber(dd->selected);
	if (const auto* cb = e.try_get<Checkbox>()) return BindingValue::FromBool(cb->checked);
	if (const auto* rb = e.try_get<Radio>()) return BindingValue::FromBool(rb->selected);
	return {};
}

// True while `e` is the focused text field — bindings must not overwrite its text mid-edit.
bool FocusedTextField(const flecs::entity e) { return e.world().get<UIFocus>().focused == e; }

void SetWidgetValue(const flecs::entity e, const BindingValue& v) {
	if (e.has<Label>()) {
		e.get_mut<Label>().text = v.AsText();
	}
	else if (e.has<TextInput>()) {
		if (!FocusedTextField(e)) {
			e.get_mut<TextInput>().text = v.AsText();
		}
	}
	else if (e.has<TextArea>()) {
		if (!FocusedTextField(e)) {
			e.get_mut<TextArea>().text = v.AsText();
		}
	}
	else if (e.has<ProgressBar>()) {
		e.get_mut<ProgressBar>().value = static_cast<float>(v.AsNumber());
	}
	else if (e.has<Slider>()) {
		e.get_mut<Slider>().value = static_cast<float>(v.AsNumber());
	}
	else if (e.has<Dropdown>()) {
		e.get_mut<Dropdown>().selected = v.AsInt();
	}
	else if (e.has<Checkbox>()) {
		e.get_mut<Checkbox>().checked = v.AsBool();
	}
	else if (e.has<Radio>()) {
		e.get_mut<Radio>().selected = v.AsBool();
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Source strategies: Blackboard (ai::Blackboard), EntityField (Flecs meta), Script (callbacks).
// ─────────────────────────────────────────────────────────────────────────────────────────────

BindingValue ReadBlackboard(const flecs::entity bb_entity, const std::string& key) {
	// is_valid (not is_alive): a binding source may legitimately be 0/destroyed; is_alive(0) asserts
	// (debug) / reads out of bounds (release) under flecs 4.x, whereas is_valid returns false safely.
	if (!bb_entity.is_valid() || !bb_entity.has<ai::Blackboard>()) {
		return {};
	}
	const auto& bb = bb_entity.get<ai::Blackboard>();
	// Probe the typed maps in a fixed order so a key resolves to whatever kind it was stored as.
	if (const auto it = bb.strings.find(key); it != bb.strings.end()) return BindingValue::FromText(it->second);
	if (const auto it = bb.floats.find(key); it != bb.floats.end()) return BindingValue::FromNumber(it->second);
	if (const auto it = bb.bools.find(key); it != bb.bools.end()) return BindingValue::FromBool(it->second);
	return {};
}

void WriteBlackboard(const flecs::entity bb_entity, const std::string& key, const BindingValue& v) {
	if (!bb_entity.is_valid() || !bb_entity.has<ai::Blackboard>()) {
		return;
	}
	auto& bb = bb_entity.get_mut<ai::Blackboard>();
	switch (v.kind) {
	case BindingValue::Kind::Text:
	{
		bb.SetString(key, v.text);
		break;
	}
	case BindingValue::Kind::Number:
	{
		bb.SetFloat(key, static_cast<float>(v.number));
		break;
	}
	case BindingValue::Kind::Bool:
	{
		bb.SetBool(key, v.b);
		break;
	}
	default: break;
	}
}

// Classify a reflected member's type into a BindingValue kind. Numeric primitives → Number, bool →
// Bool, everything else (char*, std::string, enums serialised as text) → Text.
BindingValue::Kind ClassifyMetaType(const ecs_entity_t type) {
	if (type == ecs_id(ecs_bool_t)) {
		return BindingValue::Kind::Bool;
	}
	if (type == ecs_id(ecs_f32_t)
		|| type == ecs_id(ecs_f64_t)
		|| type == ecs_id(ecs_i8_t)
		|| type == ecs_id(ecs_i16_t)
		|| type == ecs_id(ecs_i32_t)
		|| type == ecs_id(ecs_i64_t)
		|| type == ecs_id(ecs_u8_t)
		|| type == ecs_id(ecs_u16_t)
		|| type == ecs_id(ecs_u32_t)
		|| type == ecs_id(ecs_u64_t)
		|| type == ecs_id(ecs_byte_t)) {
		return BindingValue::Kind::Number;
	}
	return BindingValue::Kind::Text;
}

BindingValue ReadEntityField(const flecs::world& world, const flecs::entity source, const DataBinding& bind) {
	if (!source.is_valid() || !bind.component) {
		return {};
	}
	const void* ptr = ecs_get_id(world.c_ptr(), source, bind.component);
	if (!ptr) {
		return {};
	}
	ecs_meta_cursor_t cur = ecs_meta_cursor(world.c_ptr(), bind.component, const_cast<void*>(ptr));
	if (ecs_meta_member(&cur, bind.member.c_str()) != 0) {
		return {};
	}
	switch (ClassifyMetaType(ecs_meta_get_type(&cur))) {
	case BindingValue::Kind::Bool:
	{
		return BindingValue::FromBool(ecs_meta_get_bool(&cur));
	}
	case BindingValue::Kind::Text:
	{
		const char* s = ecs_meta_get_string(&cur);
		return BindingValue::FromText(s != nullptr ? s : "");
	}
	default:
	{
		return BindingValue::FromNumber(ecs_meta_get_float(&cur));
	}
	}
}

void WriteEntityField(
	const flecs::world& world,
	const flecs::entity source,
	const DataBinding& bind,
	const BindingValue& v
) {
	if (!source.is_valid() || !bind.component) {
		return;
	}
	void* ptr = ecs_get_mut_id(world.c_ptr(), source, bind.component);
	if (!ptr) {
		return;
	}
	ecs_meta_cursor_t cur = ecs_meta_cursor(world.c_ptr(), bind.component, ptr);
	if (ecs_meta_member(&cur, bind.member.c_str()) != 0) {
		return;
	}
	switch (v.kind) {
	case BindingValue::Kind::Bool:
	{
		ecs_meta_set_bool(&cur, v.b);
		break;
	}
	case BindingValue::Kind::Text:
	{
		ecs_meta_set_string(&cur, v.text.c_str());
		break;
	}
	case BindingValue::Kind::Number:
	{
		ecs_meta_set_float(&cur, v.number);
		break;
	}
	default: break;
	}
	ecs_modified_id(world.c_ptr(), source, bind.component);
}

BindingValue GetSource(const flecs::world& world, const DataBinding& bind, const flecs::entity source) {
	switch (bind.source) {
	case BindSource::Blackboard: return ReadBlackboard(source, bind.key);
	case BindSource::EntityField: return ReadEntityField(world, source, bind);
	case BindSource::Script: return bind.getter ? bind.getter() : BindingValue{};
	}
	return {};
}

void SetSource(const flecs::world& world, const DataBinding& bind, const flecs::entity source, const BindingValue& v) {
	switch (bind.source) {
	case BindSource::Blackboard:
	{
		WriteBlackboard(source, bind.key, v);
		break;
	}
	case BindSource::EntityField:
	{
		WriteEntityField(world, source, bind, v);
		break;
	}
	case BindSource::Script:
	{
		if (bind.setter) bind.setter(v);
		break;
	}
	}
}

} // namespace

void RegisterUIBindings(const flecs::world& world) {
	// Source-entity relationship: exclusive so a binding has a single source.
	world.component<BindsTo>().add(flecs::Exclusive);

	// === SYSTEM: Data binding sync (OnUpdate) ===
	// Runs after the widget interaction systems (registered earlier) so it sees this frame's edits,
	// and before the PreStore draw-list build so source-driven values are rendered the same frame.
	world.system<DataBinding>("UIDataBindingSync")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, DataBinding& bind) {
			const flecs::entity e = it.entity(i);
			const flecs::world ecs_world = e.world();
			const flecs::entity source = e.target<BindsTo>(); // the data-owning entity (invalid for Script)

			// First sync after authoring: if the source already has a value it is authoritative — pull
			// it into the widget. Otherwise seed the (empty) source from the widget's authored value, so
			// a writable binding publishes its initial value (e.g. a slider at 50 populates its key).
			if (bind.last.Empty()) {
				if (const BindingValue src = GetSource(ecs_world, bind, source); !src.Empty()) {
					SetWidgetValue(e, src);
				}
				else if (bind.mode != BindMode::ToUI) {
					SetSource(ecs_world, bind, source, GetWidgetValue(e));
				}
				bind.last = GetWidgetValue(e);
				return;
			}

			const BindingValue ui = GetWidgetValue(e);

			if (const bool ui_changed = !ui.Empty() && ui != bind.last; bind.mode != BindMode::ToUI && ui_changed) {
				SetSource(ecs_world, bind, source, ui); // the widget was edited — push it out
				bind.last = ui;
				return;
			}
			if (bind.mode != BindMode::ToSource) {
				if (const BindingValue src = GetSource(ecs_world, bind, source); !src.Empty()) {
					SetWidgetValue(e, src);
				}
				bind.last = GetWidgetValue(e);
			}
		});

	spdlog::info("[UIModule] Registered UI data-binding system");
}

flecs::entity
BindToBlackboard(const flecs::entity e, const flecs::entity blackboard, std::string key, const BindMode mode) {
	if (blackboard.is_valid() && !blackboard.has<ai::Blackboard>()) {
		blackboard.set<ai::Blackboard>({});
	}
	e.add<BindsTo>(blackboard);
	e.set<DataBinding>({.source = BindSource::Blackboard, .mode = mode, .key = std::move(key)});
	return e;
}

flecs::entity BindToEntityField(
	const flecs::entity e,
	const flecs::entity source,
	const flecs::entity component,
	std::string member,
	const BindMode mode
) {
	e.add<BindsTo>(source);
	e.set<DataBinding>(
		{.source = BindSource::EntityField, .mode = mode, .component = component, .member = std::move(member)}
	);
	return e;
}

flecs::entity BindToScript(
	const flecs::entity e,
	std::function<BindingValue()> getter,
	std::function<void(const BindingValue&)> setter,
	const BindMode mode
) {
	e.set<DataBinding>(
		{.source = BindSource::Script, .mode = mode, .getter = std::move(getter), .setter = std::move(setter)}
	);
	return e;
}

} // namespace engine::ui
