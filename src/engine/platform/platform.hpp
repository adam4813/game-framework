#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "engine/core/core.hpp"
#include "engine/platform/debug_ui.hpp"

namespace engine::audio {
class SynthSource;
} // namespace engine::audio

// Platform abstraction. All Raylib/ImGui calls live behind this interface so game logic stays engine-agnostic.
// Implementations: RaylibBackend (platform/raylib/) and the headless NullPlatform used by the scene equivalence
// test (tests/scene_equivalence/null_platform.hpp) — a new pure virtual here must be added to both.
namespace engine::platform {

class Platform;

struct PlatformRef {
	Platform* ptr;
};

// Projection mode for a 3D camera. Perspective applies a vertical field of view; Orthographic
// uses a fixed world-space view height (Camera3DParams::ortho_size) with no foreshortening.
enum class CameraProjection : uint8_t { Perspective, Orthographic };

// Parameters for a 3D camera. Uses glm math types (a core dependency) so game code gets the
// vector operations for free. Converted to the backend's native camera when BeginMode3D is called.
// `fov_y` is the vertical field of view for perspective cameras; `ortho_size` is the vertical view
// height in world units for orthographic cameras.
struct Camera3DParams {
	glm::vec3 position{0.0f};
	glm::vec3 target{0.0f};
	glm::vec3 up{0.0f, 1.0f, 0.0f};
	float fov_y = 60.0f;
	CameraProjection projection = CameraProjection::Perspective;
	float ortho_size = 20.0f;
};

struct ShaderStage {
	std::string type; // "vertex", "fragment", "geometry", "compute", etc.
	std::string path; // full resolved path to the shader source file
};

struct DrawMaterial {
	int texture{-1}; // albedo texture handle (-1 = no texture / solid colour)
	int shader{-1};  // shader handle (-1 = backend default for this draw path)
	core::Rgba tint{.r = 255, .g = 255, .b = 255, .a = 255};
	bool wireframe{false};
	bool cast_shadow{true};
	bool unlit{false}; // draw the tint as-is, skipping the scene lighting
	bool depth_write{true};      // when false, the draw tests depth but doesn't write it (transparent pass)
	bool fade_transparent{false}; // selects the occluder-fade shader's transparent (ghost) pass
};

// Identifiers for the backend's built-in unit primitive meshes. Fetch a batchable handle for one
// via Platform::PrimitiveMesh; the handle is used as RenderCommand::mesh so the backend can group
// and GPU-instance many copies of the same primitive in a single draw call.
enum class PrimitiveShape : uint8_t {
	Cube,     // unit cube, centred at the origin
	Sphere,   // unit-radius sphere
	Plane,    // 1x1 quad on the local XZ plane, +Y normal
	Cylinder, // unit-radius, unit-height cylinder, base at y=0
};

// A single backend-agnostic draw request. The render queue fills a buffer of these each frame and
// hands them to the backend, which groups them by (mesh, shader, texture, flags) and issues one
// instanced draw per group. `mesh` is a handle from PrimitiveMesh (or a future batchable mesh).
// `tint` in `material` is applied per-instance, so instances of the same mesh/shader/texture batch
// together regardless of colour.
struct RenderCommand {
	int mesh{-1};
	glm::mat4 transform{1.0f};
	DrawMaterial material{};
};

// Aggregate counters returned by SubmitBatch so the engine can display render stats.
struct BatchStats {
	int draw_calls{0};
	int instances{0};
	double group_ms{0.0}; // time spent grouping/sorting commands into batches (CPU)
	double draw_ms{0.0};  // time spent issuing the instanced draws (CPU submit, incl. VBO upload)
};

// Scene lighting pushed to the 3D lit shader before primitives are drawn. A single directional
// light plus ambient term drives Phong shading; planar projected shadows are cast onto a ground
// plane at `shadow_ground_y` (ideal for a flat playfield).
struct LightParams {
	glm::vec3 direction{-0.5f, -1.0f, -0.35f}; // direction the light travels (world space)
	core::Rgba light_color{.r = 255, .g = 244, .b = 214, .a = 255};
	float light_intensity = 1.0f;
	core::Rgba ambient_color{.r = 120, .g = 130, .b = 150, .a = 255};
	float ambient_intensity = 0.35f;
	float specular_strength = 0.4f;
	float shininess = 24.0f;

	bool shadows_enabled = true;
	float shadow_ground_y = 0.0f;
	core::Rgba shadow_color{.r = 10, .g = 12, .b = 16, .a = 120};
};

// Occluder-fade configuration pushed to fade-aware shaders (e.g. the upright-quad cutout shader).
// Upright sprites that cover `focus` (typically the camera's LookAt target, i.e. the player) on
// screen become alpha-blended translucent so the focus shows through. The test is screen-space, so
// tall quads leaning over the focus fade too. `enabled == false` (or `strength == 0`) renders those
// shaders as a plain cutout.
struct OcclusionFadeParams {
	glm::vec3 focus{0.0f};  // world-space point to keep visible
	float radius = 130.0f;  // screen-space radius (pixels) of the fade region around the focus
	float strength = 0.75f; // transparency applied over the focus (0 = off, 1 = fully invisible)
	bool enabled = false;
};

enum class MouseButton { Left = 0, Right = 1 };

class Platform {
public:
	using Rect = core::Rect;
	using Rgba = core::Rgba;
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
	// Frames-per-second over a recent window, for render-stat overlays.
	[[nodiscard]] virtual int Fps() const = 0;

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
	[[nodiscard]] virtual bool IsKeyPressedRepeat(int key_code) const = 0;
	[[nodiscard]] virtual bool IsMouseButtonDown(int button) const = 0;
	// Returns the next character typed this frame (Unicode codepoint), or 0 if none.
	// Useful for text input boxes. Returns one character per frame even if user holds a key.
	[[nodiscard]] virtual int CharInput() const = 0;
	// Returns text typed this frame (may contain multiple characters from repeat/IME).
	// Clears after the frame is read. Used by UI text input fields.
	[[nodiscard]] virtual std::string TextInput() const = 0;

	// ── Debug UI ──
	// The immediate-mode debug overlay. Always valid: a no-op DebugUi when compiled out.
	[[nodiscard]] virtual DebugUi& GetDebugUi() = 0;
	// Renders this frame's debug UI on top of everything drawn so far. Called once per frame by the
	// engine from the ecs::OnDebugUI phase; no-op while hidden or compiled out.
	virtual void DrawDebugUi() = 0;

	// ── 2D drawing ──
	virtual void DrawRect(Rect r, Rgba c) = 0;
	virtual void DrawRectLines(Rect r, float thickness, Rgba c) = 0;
	virtual void DrawRoundedRect(Rect r, float roundness, Rgba c) = 0;
	virtual void DrawRoundedRectLines(Rect r, float roundness, float thickness, Rgba c) = 0;
	virtual void DrawText(std::string_view text, float x, float y, float size, Rgba c) = 0;
	[[nodiscard]] virtual float MeasureText(std::string_view text, float size) const = 0;
	// Loads a TTF/OTF font file and makes it the font DrawText and MeasureText use from now on (the 2D UI
	// and game overlays; not the debug UI). Text is UTF-8; the printable ASCII and Latin-1 glyphs are loaded. Returns false and keeps the current font when the file can't
	// be loaded. Call after Init.
	virtual bool SetTextFont(std::string_view path) = 0;
	virtual void DrawLine(glm::vec2 a, glm::vec2 b, float thickness, Rgba c) = 0;
	virtual void DrawCircle(glm::vec2 center, float radius, Rgba c) = 0;
	// Draws `texture` into `dest` (screen-space pixels), tiling it at native resolution (the source
	// rect is the dest size, repeating via the texture's wrap mode) so low-res art stays crisp — an
	// N64-style look rather than a stretched blur. Modulated by `tint` (use white for the texture's
	// own colours). Falls back to a solid `tint` rect when the handle is invalid. Handles come from
	// LoadTexture. Used by the 2D UI layer for textured panels/buttons/backgrounds.
	virtual void DrawTexturedRect(int texture, Rect dest, Rgba tint) = 0;
	// Restricts subsequent 2D drawing to the rectangle `r` (screen-space pixels) until EndScissor
	// is called. Used by the UI layer to clip scrollable content. Calls are not nestable at the
	// backend level — the UI render pass intersects rectangles and issues a single active region.
	virtual void BeginScissor(Rect r) = 0;
	virtual void EndScissor() = 0;

	// ── 3D drawing ──
	// All 3D draw calls must occur between BeginMode3D and EndMode3D.
	virtual void BeginMode3D(const Camera3DParams& camera) = 0;
	virtual void EndMode3D() = 0;
	// Uploads scene lighting + shadow configuration to the lit shader. Call once per frame after
	// BeginMode3D and before drawing primitives.
	virtual void SetLighting(const LightParams& lighting) = 0;
	// Uploads occluder-fade configuration to fade-aware shaders (upright cutout quads). Call once
	// per frame after BeginMode3D and before drawing the quads. A disabled value clears the effect.
	virtual void SetOcclusionFade(const OcclusionFadeParams& fade) = 0;
	virtual void DrawCube(const glm::mat4& transform, glm::vec3 size, const DrawMaterial& material) = 0;
	virtual void DrawSphere(const glm::mat4& transform, float radius, const DrawMaterial& material) = 0;
	virtual void DrawQuad(const glm::mat4& transform, glm::vec2 size, const DrawMaterial& material) = 0;
	virtual void DrawCapsule(const glm::mat4& transform, float radius, float height, const DrawMaterial& material) = 0;
	virtual void DrawMesh(int handle, const glm::mat4& transform, const DrawMaterial& material) = 0;

	// ── Batched instanced drawing ──
	// Returns a batchable mesh handle for a built-in unit primitive. Stable for the process
	// lifetime; use as RenderCommand::mesh.
	[[nodiscard]] virtual int PrimitiveMesh(PrimitiveShape shape) const = 0;
	// Draws a batch of commands, grouping by (mesh, shader, texture, flags) and GPU-instancing each
	// group in a single draw call. Must be called inside a Begin/EndMode3D pass, after SetLighting.
	// Returns the number of draw calls and instances issued.
	virtual BatchStats SubmitBatch(std::span<const RenderCommand> commands) = 0;

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
	virtual void DrawDynamicMesh(int handle, const glm::mat4& transform, const DrawMaterial& material) = 0;
	virtual void UpdateDynamicMeshUVs(int handle, const std::vector<glm::vec2>& uvs) = 0;
	virtual void UpdateDynamicMeshColors(int handle, const std::vector<glm::vec4>& colors) = 0;

	// ── Shaders ──
	[[nodiscard]] virtual std::string ShaderDirectory() const = 0;
	virtual int LoadShader(const std::vector<ShaderStage>& stages) = 0;
	virtual void UnloadShader(int handle) = 0;

	// ── Assets ──
	virtual int LoadMesh(std::string_view path) = 0;    // loads a model file, returns handle or -1
	virtual int LoadTexture(std::string_view path) = 0; // loads an image file, returns handle or -1
	// Upload a texture from raw RGBA8 pixel data (width × height × 4 bytes). Returns a handle or -1.
	// Use UpdateTexture to replace the pixels later without allocating a new GPU resource.
	virtual int UploadTexture(int width, int height, const std::vector<uint8_t>& rgba) = 0;
	// Replace the pixel data of a previously uploaded texture. `rgba` must be the same dimensions.
	virtual void UpdateTexture(int handle, const std::vector<uint8_t>& rgba) = 0;
	// Free a previously loaded resource. Safe to call with an out-of-range or already-unloaded
	// handle (no-op). Handles are not reused, so the freed slot stays valid-but-empty.
	virtual void UnloadMesh(int handle) = 0;
	virtual void UnloadTexture(int handle) = 0;
	// Configure filtering / wrap mode on an already-loaded texture.
	virtual void SetTextureFilter(int handle, bool linear) = 0; // false = NEAREST (default)
	virtual void SetTextureWrap(int handle, bool clamp) = 0;    // true = CLAMP_TO_EDGE

	// ── Audio ──
	virtual int LoadSound(std::string_view path) = 0; // returns handle, or -1
	virtual void UnloadSound(int handle) = 0;
	virtual void PlaySound(int handle) = 0;

	// Streamed music (one decoded chunk at a time, for long tracks). LoadMusic returns a handle or -1
	// and deduplicates by path. PlayMusic restarts the track from the start; `loop` makes it wrap.
	// UpdateMusic refills the stream's buffers and must be called every frame while it plays.
	// StopMusic and UnloadMusic with a bad handle are no-ops.
	virtual int LoadMusic(std::string_view path) = 0;
	virtual void UnloadMusic(int handle) = 0;
	virtual void PlayMusic(int handle, bool loop) = 0;
	virtual void StopMusic(int handle) = 0;
	virtual void UpdateMusic(int handle) = 0;
	virtual void SetMusicVolume(int handle, float volume) = 0; // 0..1

	// ── Procedural audio ──
	// Starts the single procedural output stream (mono f32 at audio::kSynthSampleRate), pulled by
	// the audio device straight from `source`->Render() on the audio thread. Replaces any running
	// source. `source` must outlive the matching StopSynth() (or Shutdown); once StopSynth returns,
	// Render() is guaranteed not to be running. No-op when the audio device is unavailable.
	virtual void StartSynth(audio::SynthSource* source) = 0;
	virtual void StopSynth() = 0;

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
