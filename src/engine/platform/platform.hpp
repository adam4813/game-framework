#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

// Platform abstraction. All Raylib/ImGui calls live behind this interface so
// game logic stays engine-agnostic. Game code uses these POD types, never
// Raylib's own Vector2/Rectangle/Color.
namespace engine::platform {

class Platform;

struct PlatformRef {
	Platform* ptr;
};

// Parameters for a perspective 3D camera. Uses glm math types (a core dependency) so game
// code gets the vector operations for free. Converted to the backend's native camera when
// BeginMode3D is called.
struct Camera3DParams {
	glm::vec3 position{0.0f};
	glm::vec3 target{0.0f};
	glm::vec3 up{0.0f, 1.0f, 0.0f};
	float fov_y = 60.0f;
};

struct Rect {
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;

	[[nodiscard]] bool Contains(const glm::vec2 p) const {
		return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;
	}
};

struct Rgba {
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 255;
};

// Scene lighting pushed to the 3D lit shader before primitives are drawn. A single directional
// light plus ambient term drives Phong shading; planar projected shadows are cast onto a ground
// plane at `shadow_ground_y` (ideal for a flat playfield).
struct LightParams {
	glm::vec3 direction{-0.5f, -1.0f, -0.35f}; // direction the light travels (world space)
	Rgba light_color{255, 244, 214, 255};
	float light_intensity = 1.0f;
	Rgba ambient_color{120, 130, 150, 255};
	float ambient_intensity = 0.35f;
	float specular_strength = 0.4f;
	float shininess = 24.0f;

	bool shadows_enabled = true;
	float shadow_ground_y = 0.0f;
	Rgba shadow_color{10, 12, 16, 120};
};

namespace colors {
inline constexpr Rgba Background{24, 28, 24, 255};
inline constexpr Rgba Panel{40, 46, 40, 255};
inline constexpr Rgba PanelHi{56, 64, 56, 255};
inline constexpr Rgba PanelBg{18, 20, 18, 235};
inline constexpr Rgba Border{90, 100, 90, 255};
inline constexpr Rgba Accent{70, 140, 70, 255};
inline constexpr Rgba Title{120, 200, 120, 255};
inline constexpr Rgba Text{220, 225, 220, 255};
inline constexpr Rgba Subtle{140, 150, 140, 255};
inline constexpr Rgba Pollinator{170, 120, 210, 255};
inline constexpr Rgba Water{70, 130, 200, 255};
inline constexpr Rgba Fertilizer{90, 170, 90, 255};
inline constexpr Rgba Boost{220, 150, 60, 255};
inline constexpr Rgba Hazard{200, 70, 70, 255};
} // namespace colors

enum class MouseButton { Left = 0, Right = 1 };

class Platform {
public:
	virtual ~Platform() = default;

	// ── Lifecycle ──
	virtual void Init(int width, int height, std::string_view title) = 0;
	[[nodiscard]] virtual bool ShouldClose() const = 0;
	virtual void Shutdown() = 0;
	[[nodiscard]] virtual int Width() const = 0;
	[[nodiscard]] virtual int Height() const = 0;
	[[nodiscard]] virtual float DeltaTime() const = 0;
	[[nodiscard]] virtual double Time() const = 0;
	[[nodiscard]] virtual std::string AssetDirectory() const = 0;

	// ── Frame ──
	virtual void BeginFrame(Rgba clear) = 0;
	virtual void EndFrame() = 0;

	// ── Input ──
	[[nodiscard]] virtual glm::vec2 MousePosition() const = 0;
	[[nodiscard]] virtual glm::vec2 MouseDelta() const = 0;
	[[nodiscard]] virtual glm::vec2 MouseWheel() const = 0;
	[[nodiscard]] virtual bool MousePressed(MouseButton button) const = 0;
	[[nodiscard]] virtual bool MouseReleased(MouseButton button) const = 0;
	[[nodiscard]] virtual bool MouseDown(MouseButton button) const = 0;
	[[nodiscard]] virtual bool IsKeyDown(int key_code) const = 0;
	[[nodiscard]] virtual bool IsMouseButtonDown(int button) const = 0;
	// Returns the next character typed this frame (Unicode codepoint), or 0 if none.
	// Useful for text input boxes. Returns one character per frame even if user holds a key.
	[[nodiscard]] virtual int CharInput() const = 0;
	// Returns text typed this frame (may contain multiple characters from repeat/IME).
	// Clears after the frame is read. Used by UI text input fields.
	[[nodiscard]] virtual std::string TextInput() const = 0;

	// ── 2D drawing ──
	virtual void DrawRect(Rect r, Rgba c) = 0;
	virtual void DrawRectLines(Rect r, float thickness, Rgba c) = 0;
	virtual void DrawRoundedRect(Rect r, float roundness, Rgba c) = 0;
	virtual void DrawRoundedRectLines(Rect r, float roundness, float thickness, Rgba c) = 0;
	virtual void DrawText(std::string_view text, float x, float y, float size, Rgba c) = 0;
	[[nodiscard]] virtual float MeasureText(std::string_view text, float size) const = 0;
	virtual void DrawLine(glm::vec2 a, glm::vec2 b, float thickness, Rgba c) = 0;
	virtual void DrawCircle(glm::vec2 center, float radius, Rgba c) = 0;
	// Restricts subsequent 2D drawing to the rectangle `r` (screen-space pixels) until EndScissor
	// is called. Used by the UI layer to clip scrollable content. Calls are not nestable at the
	// backend level — the UI render pass intersects rectangles and issues a single active region.
	virtual void BeginScissor(Rect r) = 0;
	virtual void EndScissor() = 0;

	// ── 3D drawing ──
	// Primitives are drawn as lit meshes (Phong shading with per-face normals) so depth reads
	// clearly. `texture` is a handle from LoadTexture (or -1 for a flat colour). `cast_shadow`
	// adds a planar projected shadow onto the ground plane configured via SetLighting. All 3D
	// draw calls must occur between BeginMode3D and EndMode3D.
	virtual void BeginMode3D(const Camera3DParams& camera) = 0;
	virtual void EndMode3D() = 0;
	// Uploads scene lighting + shadow configuration to the lit shader. Call once per frame after
	// BeginMode3D and before drawing primitives.
	virtual void SetLighting(const LightParams& lighting) = 0;
	virtual void
	DrawCube(const glm::mat4& transform, glm::vec3 size, Rgba c, int texture, bool cast_shadow, bool wireframe) = 0;
	virtual void
	DrawSphere(const glm::mat4& transform, float radius, Rgba c, int texture, bool cast_shadow, bool wireframe) = 0;
	virtual void
	DrawQuad(const glm::mat4& transform, glm::vec2 size, Rgba c, int texture, bool cast_shadow, bool wireframe) = 0;
	virtual void DrawCapsule(
		const glm::mat4& transform,
		float radius,
		float height,
		Rgba c,
		int texture,
		bool cast_shadow,
		bool wireframe
	) = 0;
	virtual void
	DrawMesh(int handle, const glm::mat4& transform, Rgba tint, int texture, bool cast_shadow, bool wireframe) = 0;

	// Upload a dynamic mesh built from vertices, indices, per-vertex colors, and optional UV
	// coordinates. When `uvs` is empty, all tex-coords default to (0,0).
	// Returns a handle to the GPU mesh, or -1 on failure.
	virtual int UploadDynamicMesh(
		const std::vector<glm::vec3>& vertices,
		const std::vector<uint32_t>& indices,
		const std::vector<glm::vec4>& colors,
		const std::vector<glm::vec2>& uvs
	) = 0;
	virtual void UnloadDynamicMesh(int handle) = 0;
	virtual void DrawDynamicMesh(int handle, const glm::mat4& transform, int texture, bool wireframe) = 0;

	// ── Assets ──
	virtual int LoadMesh(std::string_view path) = 0;    // loads a model file, returns handle or -1
	virtual int LoadTexture(std::string_view path) = 0; // loads an image file, returns handle or -1
	// Free a previously loaded resource. Safe to call with an out-of-range or already-unloaded
	// handle (no-op). Handles are not reused, so the freed slot stays valid-but-empty.
	virtual void UnloadMesh(int handle) = 0;
	virtual void UnloadTexture(int handle) = 0;

	// ── Audio ──
	virtual int LoadSound(std::string_view path) = 0; // returns handle, or -1
	virtual void UnloadSound(int handle) = 0;
	virtual void PlaySound(int handle) = 0;

	// ── Persistence (save data) ──
	// A minimal key-value blob store for save data, keyed by slot name. Desktop stores each slot as
	// a file under a saves directory; web uses browser localStorage. `name` should be a simple
	// identifier (letters/digits/-/_) with no path separators. ReadSave returns "" for a missing
	// slot; ListSaves returns the slot names (without extension), sorted.
	virtual void WriteSave(std::string_view name, std::string_view data) = 0;
	[[nodiscard]] virtual std::string ReadSave(std::string_view name) const = 0;
	[[nodiscard]] virtual bool HasSave(std::string_view name) const = 0;
	virtual void DeleteSave(std::string_view name) = 0;
	[[nodiscard]] virtual std::vector<std::string> ListSaves() const = 0;
};

} // namespace engine::platform
