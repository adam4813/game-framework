# Input Module

Manages keyboard and mouse input state for the engine, exposing input data through a Flecs ECS singleton.

## Components

### `InputState` (Singleton)

The main input singleton that contains the complete input state for the current frame. Populated by the platform layer.

```cpp
struct InputState {
	std::array<KeyState, 512> keys;  // All 512 possible key states
	MouseState mouse;                 // Mouse and button states
};
```

### `KeyState`

Represents the state of a single key:

- `pressed` - Key was pressed this frame (transition from up → down)
- `state` - Current key state (down/up)
- `current` - Current frame state (redundant with state, for compatibility)

### `MouseState`

Represents mouse input:

- `left`, `right`, `middle` - Button states (KeyState)
- `window_position` - Mouse position in window coordinates
- `relative` - Relative movement since last frame
- `view` - View-space coordinates (populated by camera system if needed)
- `scroll` - Scroll wheel delta (cleared each frame)

### Tag: `InputEnabled`

Can be used to filter systems that should only run when input is available.

## Key Codes

Common key codes are defined in `engine::input::KeyCode` namespace:

### Letter and Number Keys

```cpp
KeyCode::A, KeyCode::B, ... KeyCode::Z
KeyCode::Key0, KeyCode::Key1, ... KeyCode::Key9
```

### Symbol Keys

```cpp
KeyCode::Minus, KeyCode::Equals, KeyCode::LeftBracket, KeyCode::RightBracket
KeyCode::Backslash, KeyCode::Semicolon, KeyCode::Apostrophe
KeyCode::Comma, KeyCode::Period, KeyCode::Slash, KeyCode::Grave
```

### Arrow Keys

```cpp
KeyCode::Up, KeyCode::Down, KeyCode::Left, KeyCode::Right
```

### Navigation Keys

```cpp
KeyCode::Home, KeyCode::End, KeyCode::PageUp, KeyCode::PageDown
KeyCode::Insert, KeyCode::Delete
```

### Function Keys

```cpp
KeyCode::F1 through KeyCode::F12
```

### Numpad Keys

```cpp
KeyCode::NumPad0 through KeyCode::NumPad9
KeyCode::NumPadMultiply, KeyCode::NumPadAdd, KeyCode::NumPadSubtract
KeyCode::NumPadDecimal, KeyCode::NumPadDivide, KeyCode::NumPadEnter
```

### Modifier Keys

```cpp
KeyCode::LeftCtrl, KeyCode::RightCtrl
KeyCode::LeftShift, KeyCode::RightShift
KeyCode::LeftAlt, KeyCode::RightAlt
KeyCode::LeftCmd, KeyCode::RightCmd
```

### Lock Keys

```cpp
KeyCode::CapsLock, KeyCode::NumLock, KeyCode::ScrollLock
```

### Special Keys

```cpp
KeyCode::Space, KeyCode::Tab, KeyCode::Return, KeyCode::Escape
KeyCode::Backspace, KeyCode::Delete, KeyCode::Pause, KeyCode::PrintScreen
```

## Systems

### `InputPoll` (PreUpdate)

Runs before game logic each frame. Polls input from the platform layer and updates the InputState singleton with current
keyboard and mouse state.

**What it does:**

- Updates key states from platform
- Updates mouse button states
- Tracks "pressed" events (transitions from up to down)
- Updates mouse position and delta
- Updates scroll wheel state

### `InputResetFrameState` (PostUpdate)

Runs after game logic. Clears frame-state flags so they don't persist into the next frame.

**What it clears:**

- Key `pressed` flags (only set when key transitions from up to down)
- Mouse button `pressed` flags
- Mouse scroll values

## Usage Example

```cpp
// Check if a key is currently held down
bool is_down = world.get<InputState>().keys[KeyCode::W].state;

// Check if a key was pressed this frame
bool just_pressed = world.get<InputState>().keys[KeyCode::Space].pressed;

// Get mouse position
auto& input = world.get<InputState>();
float mouse_x = input.mouse.window_position.x;
float mouse_y = input.mouse.window_position.y;

// Check mouse button
bool left_click = input.mouse.left.pressed;

// Get mouse scroll
float scroll_y = input.mouse.scroll.y;
```

## Platform Requirements

The InputModule expects the platform layer to implement:

- `IsKeyDown(int key_code)` - Returns true if key is held down
- `IsMouseButtonDown(int button)` - Returns true if mouse button is held (0=left, 1=right, 2=middle)
- `GetMousePosition()` - Returns (x, y) in window coordinates
- `GetMouseDelta()` - Returns (dx, dy) relative movement
- `GetMouseWheel()` - Returns (scroll_x, scroll_y)

## Integration

Create the module in your ECS world initialization:

```cpp
flecs::world world;
// ... other modules ...
engine::input::InputModule input_module(world);
```

The input state will be automatically populated each frame by the InputPoll system.
