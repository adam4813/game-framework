#pragma once

#include <array>

namespace engine::input {

// Key state tracking (similar to flecs-components-input)
struct KeyState {
	bool pressed{false}; // Key was pressed this frame
	bool state{false};   // Current key state (down/up)
	bool current{false}; // Current frame state
};

// Mouse coordinate tracking
struct MouseCoord {
	float x{0.0F};
	float y{0.0F};
};

// Mouse state including all tracked coordinates
struct MouseState {
	KeyState left;              // Left button state
	KeyState right;             // Right button state
	KeyState middle;            // Middle button state
	MouseCoord window_position; // Window-space coordinates
	MouseCoord relative;        // Relative movement this frame
	MouseCoord view;            // View-space coordinates
	MouseCoord scroll;          // Scroll wheel delta
};

// Main input singleton - populated by platform layer
// Represents the complete input state for the current frame
struct InputState {
	static constexpr size_t MaxKeys = 1600;
	std::array<KeyState, MaxKeys> keys;
	MouseState mouse;

	// === Script-facing helpers ===
	// key is a KeyCode constant (see the KeyCode namespace below).

	// True while the key is held down.
	[[nodiscard]] bool IsKeyDown(const int key) const {
		return key >= 0 && key < static_cast<int>(MaxKeys) && keys[static_cast<size_t>(key)].state;
	}

	// True only on the frame the key transitioned to pressed (rising edge).
	[[nodiscard]] bool WasKeyPressed(const int key) const {
		return key >= 0 && key < static_cast<int>(MaxKeys) && keys[static_cast<size_t>(key)].pressed;
	}

	// True while the key is not held down.
	[[nodiscard]] bool IsKeyUp(const int key) const { return !IsKeyDown(key); }
};

// === TAG COMPONENTS ===
// Used to filter systems to run only when input is available
struct InputEnabled {};

// === KEY CODE CONSTANTS ===
// US keyboard layout key codes and modifiers
namespace KeyCode {
// Special keys
constexpr int Unknown = 0;
constexpr int Return = '\r';
constexpr int Escape = 0x001B;
constexpr int Backspace = '\b';
constexpr int Tab = '\t';
constexpr int Space = ' ';
constexpr int Delete = 127;

// Numbers (top row)
constexpr int Key0 = '0';
constexpr int Key1 = '1';
constexpr int Key2 = '2';
constexpr int Key3 = '3';
constexpr int Key4 = '4';
constexpr int Key5 = '5';
constexpr int Key6 = '6';
constexpr int Key7 = '7';
constexpr int Key8 = '8';
constexpr int Key9 = '9';

// Letter keys (lowercase)
constexpr int A = 'a';
constexpr int B = 'b';
constexpr int C = 'c';
constexpr int D = 'd';
constexpr int E = 'e';
constexpr int F = 'f';
constexpr int G = 'g';
constexpr int H = 'h';
constexpr int I = 'i';
constexpr int J = 'j';
constexpr int K = 'k';
constexpr int L = 'l';
constexpr int M = 'm';
constexpr int N = 'n';
constexpr int O = 'o';
constexpr int P = 'p';
constexpr int Q = 'q';
constexpr int R = 'r';
constexpr int S = 's';
constexpr int T = 't';
constexpr int U = 'u';
constexpr int V = 'v';
constexpr int W = 'w';
constexpr int X = 'x';
constexpr int Y = 'y';
constexpr int Z = 'z';

// Symbol keys
constexpr int Minus = '-';        // - and _
constexpr int Equals = '=';       // = and +
constexpr int LeftBracket = '[';  // [ and {
constexpr int RightBracket = ']'; // ] and }
constexpr int Backslash = '\\';   // \ and |
constexpr int Semicolon = ';';    // ; and :
constexpr int Apostrophe = '\'';  // ' and "
constexpr int Comma = ',';        // , and <
constexpr int Period = '.';       // . and >
constexpr int Slash = '/';        // / and ?
constexpr int Grave = '`';        // ` and ~

// Arrow keys
constexpr int Right = 0x0104;
constexpr int Left = 0x0105;
constexpr int Down = 0x0106;
constexpr int Up = 0x0107;

// Navigation keys
constexpr int Home = 0x0108;
constexpr int End = 0x0109;
constexpr int PageUp = 0x010A;
constexpr int PageDown = 0x010B;
constexpr int Insert = 0x010C;

// Function keys
constexpr int F1 = 0x0201;
constexpr int F2 = 0x0202;
constexpr int F3 = 0x0203;
constexpr int F4 = 0x0204;
constexpr int F5 = 0x0205;
constexpr int F6 = 0x0206;
constexpr int F7 = 0x0207;
constexpr int F8 = 0x0208;
constexpr int F9 = 0x0209;
constexpr int F10 = 0x020A;
constexpr int F11 = 0x020B;
constexpr int F12 = 0x020C;

// Numpad keys
constexpr int NumPad0 = 0x0300;
constexpr int NumPad1 = 0x0301;
constexpr int NumPad2 = 0x0302;
constexpr int NumPad3 = 0x0303;
constexpr int NumPad4 = 0x0304;
constexpr int NumPad5 = 0x0305;
constexpr int NumPad6 = 0x0306;
constexpr int NumPad7 = 0x0307;
constexpr int NumPad8 = 0x0308;
constexpr int NumPad9 = 0x0309;
constexpr int NumPadMultiply = 0x030A;
constexpr int NumPadAdd = 0x030B;
constexpr int NumPadSubtract = 0x030C;
constexpr int NumPadDecimal = 0x030D;
constexpr int NumPadDivide = 0x030E;
constexpr int NumPadEnter = 0x030F;

// Modifier keys
constexpr int LeftCtrl = 0x0401;
constexpr int LeftShift = 0x0402;
constexpr int LeftAlt = 0x0403;
constexpr int LeftCmd = 0x0404;

constexpr int RightCtrl = 0x0405;
constexpr int RightShift = 0x0406;
constexpr int RightAlt = 0x0407;
constexpr int RightCmd = 0x0408;

// Lock keys
constexpr int CapsLock = 0x0501;
constexpr int NumLock = 0x0502;
constexpr int ScrollLock = 0x0503;

// Media/System keys (optional extended support)
constexpr int PrintScreen = 0x0601;
constexpr int Pause = 0x0602;
} // namespace KeyCode

} // namespace engine::input
