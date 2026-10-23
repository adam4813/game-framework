#include "key_names.hpp"

#include <algorithm>
#include <array>

#include "input_components.hpp"

namespace engine::input {

namespace {

// clang-format off
#define K(Name) KeyName{#Name, KeyCode::Name}

constexpr std::array kKeyNames{
	// Special
	K(Return), K(Escape), K(Backspace), K(Tab), K(Space), K(Delete),
	// Letters
	K(A), K(B), K(C), K(D), K(E), K(F), K(G), K(H), K(I), K(J), K(K), K(L), K(M),
	K(N), K(O), K(P), K(Q), K(R), K(S), K(T), K(U), K(V), K(W), K(X), K(Y), K(Z),
	// Numbers
	K(Key0), K(Key1), K(Key2), K(Key3), K(Key4),
	K(Key5), K(Key6), K(Key7), K(Key8), K(Key9),
	// Symbols
	K(Minus), K(Equals), K(LeftBracket), K(RightBracket), K(Backslash),
	K(Semicolon), K(Apostrophe), K(Comma), K(Period), K(Slash), K(Grave),
	// Arrows
	K(Right), K(Left), K(Down), K(Up),
	// Navigation
	K(Home), K(End), K(PageUp), K(PageDown), K(Insert),
	// Function keys
	K(F1), K(F2), K(F3), K(F4), K(F5), K(F6),
	K(F7), K(F8), K(F9), K(F10), K(F11), K(F12),
	// Numpad
	K(NumPad0), K(NumPad1), K(NumPad2), K(NumPad3), K(NumPad4),
	K(NumPad5), K(NumPad6), K(NumPad7), K(NumPad8), K(NumPad9),
	K(NumPadMultiply), K(NumPadAdd), K(NumPadSubtract),
	K(NumPadDecimal), K(NumPadDivide), K(NumPadEnter),
	// Modifiers
	K(LeftCtrl), K(LeftShift), K(LeftAlt), K(LeftCmd),
	K(RightCtrl), K(RightShift), K(RightAlt), K(RightCmd),
	// Locks / misc
	K(CapsLock), K(NumLock), K(ScrollLock), K(PrintScreen), K(Pause),
};

#undef K
// clang-format on

} // namespace

std::span<const KeyName> KeyNames() { return kKeyNames; }

std::optional<int> KeyCodeFromName(const std::string_view name) {
	const auto it = std::ranges::find(kKeyNames, name, &KeyName::name);
	return it != kKeyNames.end() ? std::optional{it->code} : std::nullopt;
}

} // namespace engine::input
