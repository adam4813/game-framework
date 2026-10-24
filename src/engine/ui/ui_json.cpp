#include "ui_json.hpp"

#include <string>
#include <utility>

#include <flecs.h>
#include <nlohmann/json.hpp>

#include "engine/ai/ai.hpp"
#include "engine/assets/assets.hpp"
#include "engine/core/core.hpp"
#include "engine/level/level.hpp"
#include "ui_binding.hpp"
#include "ui_components.hpp"

using engine::core::JRgba;
using nlohmann::json;

namespace engine::ui {

namespace {

TextAlign ParseAlign(const std::string& s, const TextAlign def) {
	if (s == "left") return TextAlign::Left;
	if (s == "center") return TextAlign::Center;
	if (s == "right") return TextAlign::Right;
	return def;
}

StackDirection ParseDirection(const std::string& s, const StackDirection def) {
	if (s == "horizontal") return StackDirection::Horizontal;
	if (s == "vertical") return StackDirection::Vertical;
	return def;
}

// Optional "texture" path -> loaded texture handle (-1 when absent). Shared by Panel/Button loaders.
int LoadTextureField(const flecs::world& w, const json& j) {
	const std::string path = j.value("texture", std::string{});
	if (path.empty()) {
		return -1;
	}
	return assets::LoadTexture(w, assets::ResolveAsset(w, path));
}

core::Rect ParseRect(const json& j) {
	if (j.is_array() && j.size() >= 4) {
		return {.x = j[0].get<float>(), .y = j[1].get<float>(), .w = j[2].get<float>(), .h = j[3].get<float>()};
	}
	if (j.is_object()) {
		return {.x = j.value("x", 0.0F), .y = j.value("y", 0.0F), .w = j.value("w", 0.0F), .h = j.value("h", 0.0F)};
	}
	return {};
}

} // namespace

void RegisterUILevelLoaders(const flecs::world& world) {
	// --- Core: rect + element metadata ---
	level::RegisterComponentLoader(world, "ui_rect", [](const flecs::entity e, const json& j) {
		e.set<UIRect>({ParseRect(j)});
	});
	level::RegisterComponentLoader(world, "ui_element", [](const flecs::entity e, const json& j) {
		e.set<UIElement>({.z_index = j.value("z_index", 0), .visible = j.value("visible", true)});
	});

	// --- Visuals ---
	level::RegisterComponentLoader(world, "panel", [](const flecs::entity e, const json& j) {
		Panel p{};
		p.color = JRgba(j, "color", p.color);
		p.border_color = JRgba(j, "border_color", p.border_color);
		p.border_thickness = j.value("border_thickness", p.border_thickness);
		p.roundness = j.value("roundness", p.roundness);
		p.texture = LoadTextureField(e.world(), j);
		e.set<Panel>(p);
	});
	level::RegisterComponentLoader(world, "label", [](const flecs::entity e, const json& j) {
		Label l{};
		l.text = j.value("text", l.text);
		l.font_size = j.value("font_size", l.font_size);
		l.color = JRgba(j, "color", l.color);
		l.align = ParseAlign(j.value("align", std::string{}), l.align);
		l.wrap = j.value("wrap", l.wrap);
		l.line_spacing = j.value("line_spacing", l.line_spacing);
		l.outline = j.value("outline", l.outline);
		l.outline_color = JRgba(j, "outline_color", l.outline_color);
		e.set<Label>(std::move(l));
	});
	level::RegisterComponentLoader(world, "button", [](const flecs::entity e, const json& j) {
		Button b{};
		b.label = j.value("label", b.label);
		b.font_size = j.value("font_size", b.font_size);
		b.normal = JRgba(j, "normal", b.normal);
		b.hover = JRgba(j, "hover", b.hover);
		b.pressed_color = JRgba(j, "pressed_color", b.pressed_color);
		b.border_color = JRgba(j, "border_color", b.border_color);
		b.text_color = JRgba(j, "text_color", b.text_color);
		b.roundness = j.value("roundness", b.roundness);
		b.border_thickness = j.value("border_thickness", b.border_thickness);
		b.texture = LoadTextureField(e.world(), j);
		e.set<Button>(std::move(b));
	});
	level::RegisterComponentLoader(world, "progress_bar", [](const flecs::entity e, const json& j) {
		ProgressBar p{};
		p.value = j.value("value", p.value);
		p.min = j.value("min", p.min);
		p.max = j.value("max", p.max);
		p.fill_color = JRgba(j, "fill_color", p.fill_color);
		p.track_color = JRgba(j, "track_color", p.track_color);
		p.border_color = JRgba(j, "border_color", p.border_color);
		p.border_thickness = j.value("border_thickness", p.border_thickness);
		p.roundness = j.value("roundness", p.roundness);
		e.set<ProgressBar>(p);
	});
	level::RegisterComponentLoader(world, "slider", [](const flecs::entity e, const json& j) {
		Slider s{};
		s.value = j.value("value", s.value);
		s.min = j.value("min", s.min);
		s.max = j.value("max", s.max);
		s.step = j.value("step", s.step);
		e.set<Slider>(std::move(s));
	});
	level::RegisterComponentLoader(world, "spinner", [](const flecs::entity e, const json& j) {
		Spinner s{};
		s.color = JRgba(j, "color", s.color);
		s.radius = j.value("radius", s.radius);
		s.dot_radius = j.value("dot_radius", s.dot_radius);
		s.dots = j.value("dots", s.dots);
		s.speed = j.value("speed", s.speed);
		e.set<Spinner>(s);
	});

	// --- Layout containers ---
	level::RegisterComponentLoader(world, "stack", [](const flecs::entity e, const json& j) {
		Stack s{};
		s.direction = ParseDirection(j.value("direction", std::string{}), s.direction);
		s.spacing = j.value("spacing", s.spacing);
		s.padding = j.value("padding", s.padding);
		s.stretch = j.value("stretch", s.stretch);
		e.set<Stack>(s);
	});
	level::RegisterComponentLoader(world, "scroll_rect", [](const flecs::entity e, const json& j) {
		ScrollRect s{};
		s.vertical = j.value("vertical", s.vertical);
		s.horizontal = j.value("horizontal", s.horizontal);
		s.scrollbar_thickness = j.value("scrollbar_thickness", s.scrollbar_thickness);
		s.wheel_speed = j.value("wheel_speed", s.wheel_speed);
		s.track_color = JRgba(j, "track_color", s.track_color);
		s.thumb_color = JRgba(j, "thumb_color", s.thumb_color);
		e.set<ScrollRect>(s);
	});
	level::RegisterComponentLoader(world, "modal", [](const flecs::entity e, const json& j) {
		Modal m{};
		m.open = j.value("open", m.open);
		m.backdrop_color = JRgba(j, "backdrop_color", m.backdrop_color);
		m.close_on_backdrop = j.value("close_on_backdrop", m.close_on_backdrop);
		e.set<Modal>(m);
	});

	// --- Input widgets ---
	level::RegisterComponentLoader(world, "checkbox", [](const flecs::entity e, const json& j) {
		Checkbox c{};
		c.checked = j.value("checked", c.checked);
		c.label = j.value("label", c.label);
		c.font_size = j.value("font_size", c.font_size);
		c.box_size = j.value("box_size", c.box_size);
		e.set<Checkbox>(std::move(c));
	});
	level::RegisterComponentLoader(world, "radio", [](const flecs::entity e, const json& j) {
		Radio r{};
		r.group = j.value("group", r.group);
		r.value = j.value("value", r.value);
		r.selected = j.value("selected", r.selected);
		r.label = j.value("label", r.label);
		r.font_size = j.value("font_size", r.font_size);
		r.radius = j.value("radius", r.radius);
		e.set<Radio>(std::move(r));
	});
	level::RegisterComponentLoader(world, "text_input", [](const flecs::entity e, const json& j) {
		TextInput t{};
		t.text = j.value("text", t.text);
		t.placeholder = j.value("placeholder", t.placeholder);
		t.max_length = j.value("max_length", t.max_length);
		t.font_size = j.value("font_size", t.font_size);
		e.set<TextInput>(std::move(t));
	});
	level::RegisterComponentLoader(world, "text_area", [](const flecs::entity e, const json& j) {
		TextArea t{};
		t.text = j.value("text", t.text);
		t.placeholder = j.value("placeholder", t.placeholder);
		t.max_length = j.value("max_length", t.max_length);
		t.font_size = j.value("font_size", t.font_size);
		t.line_spacing = j.value("line_spacing", t.line_spacing);
		e.set<TextArea>(std::move(t));
	});
	level::RegisterComponentLoader(world, "dropdown", [](const flecs::entity e, const json& j) {
		Dropdown d{};
		if (j.contains("options") && j.at("options").is_array()) {
			for (const auto& opt : j.at("options")) {
				d.options.push_back(opt.get<std::string>());
			}
		}
		d.selected = j.value("selected", d.selected);
		d.placeholder = j.value("placeholder", d.placeholder);
		d.font_size = j.value("font_size", d.font_size);
		d.row_height = j.value("row_height", d.row_height);
		e.set<Dropdown>(std::move(d));
	});

	// --- Data binding ---
	// The source *kind*, key/member, and mode come from the component JSON; the source *entity* is
	// wired by the "binding_source" ref (resolved in the level's second pass). Script sources aren't
	// JSON-authorable (callbacks are code), so JSON supports blackboard and entity_field.
	level::RegisterComponentLoader(world, "data_binding", [](const flecs::entity e, const json& j) {
		DataBinding b{};
		const std::string source = j.value("source", std::string{"blackboard"});
		b.source = source == "entity_field" ? BindSource::EntityField : BindSource::Blackboard;
		if (const std::string mode = j.value("mode", std::string{"two_way"}); mode == "to_ui") {
			b.mode = BindMode::ToUI;
		}
		else if (mode == "to_source") {
			b.mode = BindMode::ToSource;
		}
		else {
			b.mode = BindMode::TwoWay;
		}
		b.key = j.value("key", std::string{});
		b.member = j.value("member", std::string{});
		if (j.contains("component")) {
			b.component = e.world().lookup(j.at("component").get<std::string>().c_str());
		}
		e.set<DataBinding>(std::move(b));
	});
	level::RegisterRef(world, "binding_source", [](const flecs::entity self, const flecs::entity target) {
		// Wire the source entity as a relationship (deferred-safe: the DataBinding component set earlier
		// in this same level-load batch isn't applied yet, so we cannot read/mutate it here).
		self.add<BindsTo>(target);
		// Ensure the data-owning entity has a Blackboard so Blackboard-source bindings can read/write it
		// (harmless for EntityField sources — an unused empty Blackboard).
		if (!target.has<ai::Blackboard>()) {
			target.set<ai::Blackboard>({});
		}
	});
}

} // namespace engine::ui
