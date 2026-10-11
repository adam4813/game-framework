#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "engine/core/core.hpp"

// Presentation layer for the data-driven 2D UI — the "view/presentation split". The build systems
// never call the platform directly: they translate the resolved UI tree (the "view", computed from
// parent/child relationships + UIRect) into a flat list of primitive draw commands (this
// UIDrawList singleton) during the build phase, and a single flush system submits them to the
// platform during a later phase.
//
// Draw order is fully determined by the build walk, NOT by ECS archetype/registration/iteration
// order. Every command carries:
//   - `layer`: the z-index (from UIElement.z_index, inherited by descendants). Higher layers always
//      composite on top, so e.g. a Modal (high z) paints above all base UI.
//   - `seq`:   a monotonically increasing sequence number stamped on each push, i.e. the tree
//      pre-order position. Within a layer this preserves "parent before child, earlier sibling
//      before later sibling", so a later-authored element reliably paints on top of an earlier one.
//
// The renderer sorts by (layer, seq), which is a total order independent of how the underlying
// entities happen to be stored or iterated — fixing the class of bug where an element rendered on
// top of another purely because of creation/archetype order.
namespace engine::ui {

// Kind of 2D primitive a queued UIDrawCommand represents.
enum class UIDrawKind : std::uint8_t {
	Rect,
	RoundedRect,
	RectLines,
	RoundedRectLines,
	Text,
	Line,
	Circle,
	TexturedRect, // a texture tiled into a rect, modulated by `color` (tint)
	BeginClip,    // push a scissor rectangle (intersected with any active clip)
	EndClip,      // pop the last scissor rectangle
};

// One queued 2D draw call. Only the fields relevant to `kind` are meaningful. Commands are sorted
// by (layer, seq); see the file header for the ordering model.
struct UIDrawCommand {
	UIDrawKind kind{UIDrawKind::Rect};
	core::Rect rect{};      // rect-based kinds
	core::Rgba color{};     // all kinds
	float roundness{0.0F};  // (Rounded)Rect(Lines)
	float thickness{1.0F};  // *Lines, Line
	glm::vec2 p0{0.0F};     // Text position / Line start / Circle centre
	glm::vec2 p1{0.0F};     // Line end
	float radius{0.0F};     // Circle
	int texture{-1};        // TexturedRect (handle from LoadTexture)
	std::string text;       // Text
	float font_size{20.0F}; // Text
	int layer{0};           // primary sort key (z_index)
	int seq{0};             // secondary sort key (tree pre-order paint sequence, stamped on push)
};

// Singleton frame buffer of queued UI draw commands. Cleared and refilled every frame by the build
// system, then flushed to the platform by the render system. Push* stamps each command with the
// current `layer` and the next `seq`, so game/scripting code may also enqueue custom overlay draws
// through the same list (call SetLayer first to choose the compositing layer); anything pushed
// after the build runs paints above it within the same layer.
struct UIDrawList {
	std::vector<UIDrawCommand> commands;

	// Draw state applied to subsequent pushes. `layer` is set by the build walk per element (and may
	// be set by custom overlay code); `seq` auto-increments on every push so paint order follows
	// push (== tree pre-order) order.
	int layer{0};
	int seq{0};

	void Clear() {
		commands.clear();
		layer = 0;
		seq = 0;
	}

	// Set the active z-layer stamped onto subsequent pushes (e.g. a Modal subtree raises its layer).
	void SetLayer(const int value) { layer = value; }

	void PushRect(const core::Rect r, const core::Rgba color) {
		commands.push_back({.kind = UIDrawKind::Rect, .rect = r, .color = color, .layer = layer, .seq = seq++});
	}

	void PushRoundedRect(const core::Rect r, const float roundness, const core::Rgba color) {
		commands.push_back(
			{.kind = UIDrawKind::RoundedRect,
			 .rect = r,
			 .color = color,
			 .roundness = roundness,
			 .layer = layer,
			 .seq = seq++}
		);
	}

	void PushRectLines(const core::Rect r, const float thickness, const core::Rgba color) {
		commands.push_back(
			{.kind = UIDrawKind::RectLines,
			 .rect = r,
			 .color = color,
			 .thickness = thickness,
			 .layer = layer,
			 .seq = seq++}
		);
	}

	void
	PushRoundedRectLines(const core::Rect r, const float roundness, const float thickness, const core::Rgba color) {
		commands.push_back(
			{.kind = UIDrawKind::RoundedRectLines,
			 .rect = r,
			 .color = color,
			 .roundness = roundness,
			 .thickness = thickness,
			 .layer = layer,
			 .seq = seq++}
		);
	}

	void PushText(std::string text, const float x, const float y, const float font_size, const core::Rgba color) {
		commands.push_back(
			{.kind = UIDrawKind::Text,
			 .color = color,
			 .p0 = {x, y},
			 .text = std::move(text),
			 .font_size = font_size,
			 .layer = layer,
			 .seq = seq++}
		);
	}

	void PushLine(const glm::vec2 a, const glm::vec2 b, const float thickness, const core::Rgba color) {
		commands.push_back(
			{.kind = UIDrawKind::Line,
			 .color = color,
			 .thickness = thickness,
			 .p0 = a,
			 .p1 = b,
			 .layer = layer,
			 .seq = seq++}
		);
	}

	void PushCircle(const glm::vec2 center, const float radius, const core::Rgba color) {
		commands.push_back(
			{.kind = UIDrawKind::Circle,
			 .color = color,
			 .p0 = center,
			 .radius = radius,
			 .layer = layer,
			 .seq = seq++}
		);
	}

	// Queue a textured rect: `texture` (a LoadTexture handle) tiled into `r`, modulated by `tint`.
	void PushTexturedRect(const int texture, const core::Rect r, const core::Rgba tint) {
		commands.push_back(
			{.kind = UIDrawKind::TexturedRect,
			 .rect = r,
			 .color = tint,
			 .texture = texture,
			 .layer = layer,
			 .seq = seq++}
		);
	}

	void PushBeginClip(const core::Rect r) {
		commands.push_back({.kind = UIDrawKind::BeginClip, .rect = r, .layer = layer, .seq = seq++});
	}

	void PushEndClip() { commands.push_back({.kind = UIDrawKind::EndClip, .layer = layer, .seq = seq++}); }
};

} // namespace engine::ui
