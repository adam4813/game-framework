#include "title_logo.hpp"

#include <array>
#include <cmath>
#include <numbers>
#include <string>

#include "crabnado_constants.hpp"
#include "engine/engine.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr core::Rgba kOutline{.r = 18, .g = 40, .b = 70, .a = 255};
constexpr core::Rgba kClaw{.r = 240, .g = 110, .b = 50, .a = 255};
constexpr core::Rgba kClawTip{.r = 255, .g = 170, .b = 110, .a = 255};
constexpr core::Rgba kShell{.r = 245, .g = 196, .b = 170, .a = 255};
constexpr core::Rgba kShellBand{.r = 214, .g = 132, .b = 120, .a = 255};
constexpr core::Rgba kShellCore{.r = 255, .g = 236, .b = 214, .a = 255};
constexpr core::Rgba kEye{.r = 18, .g = 40, .b = 70, .a = 255};
constexpr float kOutlineWidth = 4.0F;

// A shell perched on one letter: the letter's index in the title, how far below the label's top it sits as a fraction
// of the font size (capitals and ascenders sit higher than x-height letters), and its radius in pixels.
struct Perch {
	int letter;
	float drop;
	float radius;
};
constexpr std::array kPerches{
	Perch{.letter = 2, .drop = 0.33F, .radius = 15.0F}, // a
	Perch{.letter = 6, .drop = 0.10F, .radius = 17.0F}, // d
};

// Snapping: the claw holds open as the title's C, then snaps shut briefly. `kSnapPeriod` seconds per cycle; shut for
// `kSnapShut` of it.
constexpr float kSnapPeriod = 3.2F;
constexpr float kSnapShut = 0.16F;
constexpr float kClawOpen = 1.5F; // radians of gap between the crescent's fingertips: the C's mouth
constexpr float kClawClosed = 0.2F;
constexpr float kBobRate = 2.2F;
constexpr float kBobHeight = 3.0F;

glm::vec2 Heading(const float angle) { return {std::cos(angle), std::sin(angle)}; }

// A thick capsule from `a` to `b`, with a navy outline under it.
void PushLimb(ui::UIDrawList& list, const glm::vec2 a, const glm::vec2 b, const float thickness, const core::Rgba color,
	const bool outline) {
	const float t = outline ? thickness + 2.0F * kOutlineWidth : thickness;
	const core::Rgba c = outline ? kOutline : color;
	list.PushLine(a, b, t, c);
	list.PushCircle(a, t / 2.0F, c);
	list.PushCircle(b, t / 2.0F, c);
}

// One finger of a crescent claw: an arc of circle `center`/`radius` from `from` to `to` radians, tapering from
// `thick` at the palm to a point at the tip. Returns the tip.
glm::vec2 PushFinger(ui::UIDrawList& list, const glm::vec2 center, const float radius, const float from, const float to,
	const float thick, const bool outline) {
	constexpr int kSegments = 10;
	glm::vec2 previous = center + Heading(from) * radius;
	for (int i = 1; i <= kSegments; ++i) {
		const float t = static_cast<float>(i) / kSegments;
		const glm::vec2 point = center + Heading(from + (to - from) * t) * radius;
		const float width = thick * (1.0F - 0.6F * t);
		PushLimb(list, previous, point, width, kClaw, outline);
		previous = point;
	}
	return previous;
}

// A crescent-moon crab claw shaped like a C: two fingers curve around the circle `center`/`radius` from the palm on its
// left and stop `gap` radians short of meeting on its right. Drawn twice: the navy outline pass, then the orange fill.
void PushClaw(ui::UIDrawList& list, const glm::vec2 center, const float radius, const float gap) {
	const float scale = radius / 1.5F; // the palm's radius
	const float back = std::numbers::pi_v<float>;
	const float sweep = std::numbers::pi_v<float> - gap / 2.0F;
	const glm::vec2 palm = center + Heading(back) * radius;
	glm::vec2 upper{};
	glm::vec2 lower{};
	for (const bool outline : {true, false}) {
		upper = PushFinger(list, center, radius, back, back + sweep, scale * 1.4F, outline);
		lower = PushFinger(list, center, radius, back, back - sweep * 0.9F, scale * 1.15F, outline);
		list.PushCircle(palm, outline ? scale * 1.1F + kOutlineWidth : scale * 1.1F, outline ? kOutline : kClaw);
	}
	list.PushCircle(upper, scale * 0.14F, kClawTip);
	list.PushCircle(lower, scale * 0.12F, kClawTip);
}

// A hermit crab's spiral shell resting on `base`, with two eyes and tiny claws peeking out of its mouth.
void PushShell(ui::UIDrawList& list, const glm::vec2 base, const float r) {
	const glm::vec2 c = base - glm::vec2{0.0F, r * 0.35F};
	for (const float side : {-1.0F, 1.0F}) {
		list.PushCircle(c + glm::vec2{side * r * 0.85F, r * 0.55F}, r * 0.3F + kOutlineWidth / 2.0F, kOutline);
		list.PushCircle(c + glm::vec2{side * r * 0.85F, r * 0.55F}, r * 0.3F, kClaw);
	}
	list.PushCircle(c, r + kOutlineWidth, kOutline);
	list.PushCircle(c, r, kShell);
	list.PushCircle(c + glm::vec2{r * 0.18F, -r * 0.12F}, r * 0.66F, kShellBand);
	list.PushCircle(c + glm::vec2{r * 0.28F, -r * 0.2F}, r * 0.42F, kShell);
	list.PushCircle(c + glm::vec2{r * 0.34F, -r * 0.26F}, r * 0.2F, kShellCore);
	for (const float side : {-1.0F, 1.0F}) {
		const glm::vec2 eye = c + glm::vec2{side * r * 0.32F - r * 0.2F, r * 0.6F};
		list.PushLine(eye + glm::vec2{0.0F, r * 0.25F}, eye, 2.0F, kEye);
		list.PushCircle(eye, r * 0.16F, kEye);
	}
}

} // namespace

void RegisterTitleLogo(const flecs::world& world) {
	world.component<TitleLogo>().member<float>("top").member<float>("font_size").member<float>("time");

	// Drawn into the UI draw list after UIBuildDrawList (same phase, registered later), so it paints over the title
	// label. Tagged GameScene because the title's pipeline admits GameScene systems.
	world.system<TitleLogo>("TitleLogoDraw")
		.kind(flecs::PreStore)
		.each([](const flecs::iter& it, size_t, TitleLogo& logo) {
			logo.time += it.delta_time();
			const flecs::world world = it.world();
			const auto& platform = *world.get<platform::PlatformRef>().ptr;
			auto& list = world.get_mut<ui::UIDrawList>();
			list.SetLayer(0);

			const std::string title(constants::kWindowTitle);
			const float size = logo.font_size;
			const float width = platform.MeasureText(title, size);
			const float left = (static_cast<float>(platform.Width()) - width) / 2.0F;
			const float middle = logo.top + size / 2.0F;

			// The claw stands in for the C, so it fills that glyph's box; the label draws the rest of the title.
			const float glyphC = platform.MeasureText(title.substr(0, 1), size);
			const float phase = std::fmod(logo.time, kSnapPeriod);
			const float gap = phase > kSnapPeriod - kSnapShut ? kClawClosed : kClawOpen;
			PushClaw(list, {left + glyphC * 0.5F, middle + size * 0.04F}, size * 0.26F, gap);

			for (std::size_t i = 0; i < kPerches.size(); ++i) {
				const Perch& perch = kPerches[i];
				const auto letter = static_cast<std::size_t>(perch.letter);
				const float start = platform.MeasureText(title.substr(0, letter), size);
				const float glyph = platform.MeasureText(title.substr(letter, 1), size);
				const float bob = std::sin(logo.time * kBobRate + static_cast<float>(i) * 1.7F) * kBobHeight;
				PushShell(list, {left + start + glyph / 2.0F, logo.top + size * perch.drop + bob}, perch.radius);
			}
		})
		.add<scene::GameScene>();
}

} // namespace crabnado
