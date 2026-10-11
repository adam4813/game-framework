#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include <flecs.h>

// Data binding for the 2D UI: connect a widget's natural value (Label text, ProgressBar/Slider
// value, Checkbox/Radio checked, TextInput/TextArea text, Dropdown selected index) to an external
// "data point" without hand-written glue. A DataBinding entity component names a source; the
// UIDataBindingSync system each frame reads the source into the widget (and, for editable widgets,
// writes edits back), so JSON- or script-authored UI stays in sync with game state.
//
// Sources are a Strategy over three kinds (see BindSource):
//   - Blackboard:   a key in an entity's ai::Blackboard (the engine's existing shared key-value
//                   store — this binding is just an adapter to it, not a new registry).
//   - EntityField:  a reflected member of a component on another entity (via Flecs meta), e.g. bind
//                   a Label to Health.current — no copy, reads live ECS state.
//   - Script:       getter/setter callbacks, for values a script computes on demand.
//
// Values flow through BindingValue, a small dynamically-typed box, so a numeric source can drive a
// text Label and vice versa with automatic conversion.
namespace engine::ui {

// Dynamically-typed value carried across a binding. Keeps sources and sinks type-agnostic: a float
// Health can drive a text Label, a "true"/"false" string can drive a Checkbox, etc., via the As*
// conversions.
struct BindingValue {
	enum class Kind : std::uint8_t { None, Bool, Number, Text };

	Kind kind{Kind::None};
	bool b{false};
	double number{0.0};
	std::string text;

	[[nodiscard]] static BindingValue FromBool(const bool v) { return {.kind = Kind::Bool, .b = v}; }
	[[nodiscard]] static BindingValue FromNumber(const double v) { return {.kind = Kind::Number, .number = v}; }
	[[nodiscard]] static BindingValue FromText(std::string v) { return {.kind = Kind::Text, .text = std::move(v)}; }

	[[nodiscard]] bool Empty() const { return kind == Kind::None; }
	[[nodiscard]] bool AsBool() const;
	[[nodiscard]] double AsNumber() const;
	[[nodiscard]] int AsInt() const { return static_cast<int>(AsNumber()); }
	[[nodiscard]] std::string AsText() const;

	[[nodiscard]] bool operator==(const BindingValue& other) const;
};

// Which kind of data point a DataBinding reads/writes.
enum class BindSource : std::uint8_t { Blackboard, EntityField, Script };

// Direction of synchronisation between the widget and the source.
enum class BindMode : std::uint8_t {
	ToUI,     // source -> widget only (read-only display)
	ToSource, // widget -> source only (push edits out)
	TwoWay,   // source drives the widget until it is edited, then the edit is pushed back to source
};

// Relationship: `widget.add<BindsTo>(sourceEntity)` links a DataBinding widget to the entity that
// owns its data — the entity carrying ai::Blackboard (Blackboard source) or the `component`
// (EntityField source). A relationship (rather than a stored flecs::entity field) is used because it
// is resolved deferred-safely during JSON level loading: the pair can be added in the ref pass even
// though the DataBinding component set earlier in the same deferred batch isn't applied yet.
// Exclusive — a binding has one source. Script sources need no BindsTo.
struct BindsTo {};

// Binds the owning UI entity's widget value to a data point. Attach via the Bind* helpers or author
// it from JSON. The target widget field is inferred from the widget component present on the entity;
// the source *entity* is the BindsTo target.
struct DataBinding {
	BindSource source{BindSource::Blackboard};
	BindMode mode{BindMode::TwoWay};

	// Blackboard source: the key within the BindsTo target's ai::Blackboard.
	std::string key;

	// EntityField source: the reflected `member` of `component` on the BindsTo target.
	flecs::entity component;
	std::string member;

	// Script source.
	std::function<BindingValue()> getter;
	std::function<void(const BindingValue&)> setter;

	// Runtime: last synced value, for edit detection (do not author).
	BindingValue last;
};

// Register the UIDataBindingSync system. Called by UIModule. The shared key-value store itself is
// ai::Blackboard, registered by AIModule — this binding only reads/writes it.
void RegisterUIBindings(const flecs::world& world);

// === Convenience binders ===
// Attach a DataBinding to a UI entity `e`, returning `e` for chaining.

/// Bind the widget to `key` within `blackboard`'s ai::Blackboard component.
flecs::entity
BindToBlackboard(flecs::entity e, flecs::entity blackboard, std::string key, BindMode mode = BindMode::TwoWay);

/// Bind the widget to a reflected `member` of `component` on `source` (via Flecs meta reflection).
flecs::entity BindToEntityField(
	flecs::entity e,
	flecs::entity source,
	flecs::entity component,
	std::string member,
	BindMode mode = BindMode::TwoWay
);

/// Bind the widget to script-style `getter`/`setter` callbacks. Omit `setter` for a read-only bind.
flecs::entity BindToScript(
	flecs::entity e,
	std::function<BindingValue()> getter,
	std::function<void(const BindingValue&)> setter = {},
	BindMode mode = BindMode::TwoWay
);

} // namespace engine::ui
