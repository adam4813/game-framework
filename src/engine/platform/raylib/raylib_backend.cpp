#include "raylib_backend.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <spdlog/spdlog.h>

#include "engine/audio/synth_source.hpp"
#include "engine/core/core.hpp"
#include "engine/input/input.hpp"
#include "engine/platform/imgui/imgui_debug_ui.hpp"
#include "engine/platform/raylib/rl_imgui.hpp"
#include "engine/platform/render_batch.hpp"
#include "engine/profiling/profiling.hpp"

using engine::core::Rect;
using engine::core::Rgba;

#if defined(__EMSCRIPTEN__)
#include <cstdlib>

#include <emscripten.h>

// Browser localStorage bridge for save persistence. Keys are namespaced with a prefix so they
// don't collide with anything else the page stores. Strings are marshalled through the heap; the
// char* returned by the read/list helpers is owned by the caller (freed with std::free).
// clang-format off
EM_JS(void, js_write_save, (const char* key, const char* data), {
	localStorage.setItem('game_save_' + UTF8ToString(key), UTF8ToString(data));
});
EM_JS(char*, js_read_save, (const char* key), {
	var v = localStorage.getItem('game_save_' + UTF8ToString(key));
	if (v === null) { return 0; }
	var len = lengthBytesUTF8(v) + 1;
	var buf = _malloc(len);
	stringToUTF8(v, buf, len);
	return buf;
});
EM_JS(int, js_has_save, (const char* key), {
	return localStorage.getItem('game_save_' + UTF8ToString(key)) === null ? 0 : 1;
});
EM_JS(void, js_delete_save, (const char* key), {
	localStorage.removeItem('game_save_' + UTF8ToString(key));
});
EM_JS(char*, js_list_saves, (), {
	var prefix = 'game_save_';
	var names = [];
	for (var i = 0; i < localStorage.length; i++) {
		var k = localStorage.key(i);
		if (k.indexOf(prefix) === 0) { names.push(k.substring(prefix.length)); }
	}
	var s = names.join('\n');
	var len = lengthBytesUTF8(s) + 1;
	var buf = _malloc(len);
	stringToUTF8(s, buf, len);
	return buf;
});
// clang-format on
#endif

namespace engine::platform {

namespace {

// Single-instance sound storage (one RaylibBackend per process). Keeps the
// raylib Sound type out of the header.
std::vector<Sound> g_sounds;
// Maps canonical path → sound handle; prevents re-loading the same file from disk.
std::unordered_map<std::string, int> g_sound_paths;
// Streamed music, stored the same way as sounds.
std::vector<Music> g_music;
std::unordered_map<std::string, int> g_music_paths;

// The procedural audio stream and the source it pulls from. raylib's AudioCallback carries no user
// pointer, so the static trampoline reaches the active source through this global.
constexpr unsigned int kSynthSampleBits = 32; // f32 samples
constexpr unsigned int kSynthChannels = 1;    // mono; raylib's mixer spreads it to the device's channels
AudioStream g_synth_stream{};
std::atomic<audio::SynthSource*> g_synth_source{nullptr};

// Runs on the audio thread under raylib's audio lock (on web: the main thread, between frames).
// Must not lock, allocate, log, or call raylib — see audio::SynthSource.
void SynthTrampoline(void* buffer, const unsigned int frames) {
	static constexpr profiling::ZoneSite
		kSite{.name = "Platform::SynthCallback", .function = __func__, .file = __FILE__, .line = __LINE__};
	profiling::StaticZone _zone{kSite};
	const std::span out{static_cast<float*>(buffer), frames};
	audio::SynthSource* source = g_synth_source.load(std::memory_order_acquire);
	if (source == nullptr) {
		std::ranges::fill(out, 0.0F);
		return;
	}
	source->Render(out);
}

// Loaded models, indexed by the handle returned from LoadMesh.
std::vector<Model> g_models;
// Maps canonical path → model handle.
std::unordered_map<std::string, int> g_model_paths;

// Dynamic meshes uploaded programmatically (for tilemaps, etc), indexed by handle
std::vector<Mesh> g_dynamic_meshes;

struct CustomShaderEntry {
	bool ready = false;
	Shader shader{};
	Material material{};
	Texture2D white{};
	// Optional occluder-fade uniform locations (-1 when the shader doesn't declare them).
	int loc_fade_focus = -1;
	int loc_fade_screen_pos = -1;
	int loc_fade_radius = -1;
	int loc_fade_strength = -1;
	int loc_fade_pass = -1;
};

std::vector<CustomShaderEntry> g_custom_shaders;

// The font DrawText/MeasureText use once SetTextFont succeeds; texture id 0 means raylib's built-in font.
Font g_text_font{};
// Pixel size the font atlas is rasterised at; text at other sizes is scaled from it.
constexpr int kTextFontBaseSize = 96;
// Extra space between glyphs, as a fraction of the font size.
constexpr float kTextFontSpacing = 0.02F;

// Loaded textures, indexed by the handle returned from LoadTexture.
std::vector<Texture2D> g_textures;
// Maps canonical path → handle
std::unordered_map<std::string, int> g_texture_paths;

// Characters typed this frame, drained from raylib's queue once in BeginFrame so the debug UI and
// the game (CharInput/TextInput) both see them. g_frame_char_next is the CharInput read cursor.
std::vector<int> g_frame_chars;
size_t g_frame_char_next = 0;

// The debug UI: the ImGui overlay when compiled in (created in Init), else the no-op base.
DebugUi g_null_debug_ui;
#ifdef ENGINE_DEBUG_UI
std::unique_ptr<ImGuiDebugUi> g_debug_ui;
#endif

// Keys the engine owns and the game never sees: the debug UI toggle, when the debug UI is built.
bool IsReservedKey([[maybe_unused]] const int key_code) {
#ifdef ENGINE_DEBUG_UI
	return key_code == input::kDebugUiToggleKey;
#else
	return false;
#endif
}

// Directory holding the GLSL variant matching the active GL backend.
#if defined(__EMSCRIPTEN__)
constexpr auto kGlslDir = "glsl100";
#else
constexpr auto kGlslDir = "glsl330";
#endif

// Lit-rendering resources: the Phong shader, its custom uniform locations, the shared material,
// cached unit primitive meshes, and the per-frame lighting/shadow state. Populated once GL is
// ready (first BeginMode3D) and torn down on Shutdown.
struct LitState {
	bool ready = false;
	Shader shader{};
	Material material{}; // shared material; diffuse colour/texture swapped per draw
	Texture2D white{};   // 1x1 white fallback so untextured primitives stay flat-coloured

	Mesh cube{};
	Mesh sphere{};   // unit radius
	Mesh plane{};    // 1x1 on the XZ plane, +Y normal
	Mesh cylinder{}; // unit radius, unit height, base at y=0

	// Custom uniform locations (matModel/matNormal/mvp/colDiffuse/texture0 are bound by raylib).
	int loc_light_dir = -1;
	int loc_light_color = -1;
	int loc_ambient = -1;
	int loc_view_pos = -1;
	int loc_specular = -1;
	int loc_shininess = -1;
	int loc_shadow_pass = -1;
	int loc_shadow_color = -1;
	int loc_unlit = -1;

	bool shadows_enabled = false;
	glm::mat4 shadow_matrix{1.0f};
};

LitState g_lit;

// Instanced-rendering resources: a variant of the lit shader whose vertex stage reads the model
// matrix and colour from per-instance vertex attributes, so many copies of one primitive draw in a
// single call. Populated alongside g_lit once GL is ready. Backend-local: these are raw GL/raylib
// resource handles; a future GLFW/Vulkan backend implements SubmitBatch with its own machinery.
struct InstancedState {
	bool ready = false;
	Shader shader{};

	// Custom vertex-attribute locations for the per-instance data (queried after link).
	int attr_transform = -1; // "instanceTransform" (mat4 → occupies attr_transform .. +3)
	int attr_color = -1;     // "instanceColor" (vec4)

	// Uniform locations. mvp/colDiffuse/texture0 are bound by raylib; the rest match lit.fs/vs.
	int loc_mvp = -1;
	int loc_col_diffuse = -1;
	int loc_light_dir = -1;
	int loc_light_color = -1;
	int loc_ambient = -1;
	int loc_view_pos = -1;
	int loc_specular = -1;
	int loc_shininess = -1;
	int loc_shadow_pass = -1;
	int loc_shadow_color = -1;
	int loc_shadow_matrix = -1;
	int loc_unlit = -1;
};

InstancedState g_inst;

// A mesh that can be GPU-instanced. Owns the base geometry plus persistent per-instance VBOs
// (transform + colour) attached to the mesh VAO, grown on demand. Built-in primitives register
// here at init; RenderCommand::mesh indexes this vector.
struct RenderMesh {
	Mesh mesh{};
	unsigned int transform_vbo = 0;
	unsigned int color_vbo = 0;
	int instance_capacity = 0;
};

std::vector<RenderMesh> g_render_meshes;

// Cached lighting state so the instanced shader (a separate program from g_lit) can be fed the same
// uniforms at batch time. Updated by SetLighting / BeginMode3D.
LightParams g_light_params;
glm::vec3 g_view_pos{0.0f};

// Cached occluder-fade state, fed to fade-aware custom shaders at draw time. Updated by
// SetOcclusionFade each frame; `enabled == false` makes DrawCustomMesh push a zero strength so the
// cutout renders opaque.
OcclusionFadeParams g_fade{};

// The raylib camera for the current 3D pass, cached in BeginMode3D so the occluder-fade path can
// project the focus world position to screen space (GetWorldToScreen) for the coverage test.
Camera3D g_camera3d{};

Texture2D ResolveTextureHandle(const int texture, const Texture2D& fallback) {
	return (texture >= 0 && texture < static_cast<int>(g_textures.size())) ? g_textures[static_cast<size_t>(texture)]
																		   : fallback;
}

CustomShaderEntry* ResolveCustomShaderEntry(const int shader) {
	if (shader < 0 || shader >= static_cast<int>(g_custom_shaders.size())) {
		return nullptr;
	}
	auto& entry = g_custom_shaders[static_cast<size_t>(shader)];
	return entry.ready ? &entry : nullptr;
}

// Pushes the cached occluder-fade uniforms to a fade-aware custom shader. No-op for shaders that
// don't declare the fade uniforms (their locations stay -1). `strength` is zeroed when the effect
// is disabled so the quad renders as a plain alpha-tested cutout. `transparentPass` selects the
// shader's ghost pass (draw only the covering fragments) vs. the opaque pass (punch the hole). The
// focus world position is projected to screen space here for a resolution-correct coverage test.
void ApplyOcclusionFade(const CustomShaderEntry& entry, const bool transparentPass) {
	if (entry.loc_fade_focus < 0) {
		return;
	}
	const float focus[3] = {g_fade.focus.x, g_fade.focus.y, g_fade.focus.z};
	const float strength = g_fade.enabled ? g_fade.strength : 0.0f;
	const float pass = transparentPass ? 1.0f : 0.0f;
	// GetWorldToScreen returns top-left-origin pixels; flip Y into gl_FragCoord space (bottom-left).
	const Vector2 sp = GetWorldToScreen(Vector3{g_fade.focus.x, g_fade.focus.y, g_fade.focus.z}, g_camera3d);
	const float screen_pos[2] = {sp.x, static_cast<float>(GetScreenHeight()) - sp.y};
	SetShaderValue(entry.shader, entry.loc_fade_focus, focus, SHADER_UNIFORM_VEC3);
	SetShaderValue(entry.shader, entry.loc_fade_screen_pos, screen_pos, SHADER_UNIFORM_VEC2);
	SetShaderValue(entry.shader, entry.loc_fade_radius, &g_fade.radius, SHADER_UNIFORM_FLOAT);
	SetShaderValue(entry.shader, entry.loc_fade_strength, &strength, SHADER_UNIFORM_FLOAT);
	if (entry.loc_fade_pass >= 0) {
		SetShaderValue(entry.shader, entry.loc_fade_pass, &pass, SHADER_UNIFORM_FLOAT);
	}
}

// glm is column-major; map column i to raylib Matrix column i (raylib is also column-major).
Matrix ToRayMatrix(const glm::mat4& m) {
	Matrix r;
	r.m0 = m[0][0];
	r.m1 = m[0][1];
	r.m2 = m[0][2];
	r.m3 = m[0][3];
	r.m4 = m[1][0];
	r.m5 = m[1][1];
	r.m6 = m[1][2];
	r.m7 = m[1][3];
	r.m8 = m[2][0];
	r.m9 = m[2][1];
	r.m10 = m[2][2];
	r.m11 = m[2][3];
	r.m12 = m[3][0];
	r.m13 = m[3][1];
	r.m14 = m[3][2];
	r.m15 = m[3][3];
	return r;
}

void DrawMeshWithMaterial(const Mesh& mesh, const Material& material, const glm::mat4& model, const bool wireframe) {
	if (wireframe) {
		rlEnableWireMode();
	}
	::DrawMesh(mesh, material, ToRayMatrix(model));
	if (wireframe) {
		rlDisableWireMode();
	}
}

// Projects world-space geometry onto the plane y = ground along the light direction `l`.
glm::mat4 BuildShadowMatrix(const glm::vec3& l, const float ground) {
	glm::mat4 s{1.0f};
	const float k = 1.0f / l.y;
	s[1][0] = -l.x * k;
	s[1][1] = 0.0f;
	s[1][2] = -l.z * k;
	s[3][0] = l.x * k * ground;
	s[3][1] = ground;
	s[3][2] = l.z * k * ground;
	return s;
}

Color ToRay(const Rgba& c) { return Color{.r = c.r, .g = c.g, .b = c.b, .a = c.a}; }

Rectangle ToRay(const Rect& r) { return Rectangle{.x = r.x, .y = r.y, .width = r.w, .height = r.h}; }

Vector2 ToRay(const glm::vec2& v) { return Vector2{.x = v.x, .y = v.y}; }

Vector3 ToRay(const glm::vec3& v) { return Vector3{.x = v.x, .y = v.y, .z = v.z}; }

int MouseToRay(const MouseButton b) { return b == MouseButton::Right ? MOUSE_BUTTON_RIGHT : MOUSE_BUTTON_LEFT; }

int KeyCodeToRay(const int key_code) {
	switch (key_code) {
	// Special keys
	case input::KeyCode::Space: return KEY_SPACE;
	case input::KeyCode::Apostrophe: return KEY_APOSTROPHE;
	case input::KeyCode::Comma: return KEY_COMMA;
	case input::KeyCode::Minus: return KEY_MINUS;
	case input::KeyCode::Period: return KEY_PERIOD;
	case input::KeyCode::Slash: return KEY_SLASH;

	// Numbers
	case input::KeyCode::Key0: return KEY_ZERO;
	case input::KeyCode::Key1: return KEY_ONE;
	case input::KeyCode::Key2: return KEY_TWO;
	case input::KeyCode::Key3: return KEY_THREE;
	case input::KeyCode::Key4: return KEY_FOUR;
	case input::KeyCode::Key5: return KEY_FIVE;
	case input::KeyCode::Key6: return KEY_SIX;
	case input::KeyCode::Key7: return KEY_SEVEN;
	case input::KeyCode::Key8: return KEY_EIGHT;
	case input::KeyCode::Key9: return KEY_NINE;

	// Symbols
	case input::KeyCode::Semicolon: return KEY_SEMICOLON;
	case input::KeyCode::Equals: return KEY_EQUAL;
	case input::KeyCode::LeftBracket: return KEY_LEFT_BRACKET;
	case input::KeyCode::Backslash: return KEY_BACKSLASH;
	case input::KeyCode::RightBracket: return KEY_RIGHT_BRACKET;
	case input::KeyCode::Grave: return KEY_GRAVE;

	// Letters
	case input::KeyCode::A: return KEY_A;
	case input::KeyCode::B: return KEY_B;
	case input::KeyCode::C: return KEY_C;
	case input::KeyCode::D: return KEY_D;
	case input::KeyCode::E: return KEY_E;
	case input::KeyCode::F: return KEY_F;
	case input::KeyCode::G: return KEY_G;
	case input::KeyCode::H: return KEY_H;
	case input::KeyCode::I: return KEY_I;
	case input::KeyCode::J: return KEY_J;
	case input::KeyCode::K: return KEY_K;
	case input::KeyCode::L: return KEY_L;
	case input::KeyCode::M: return KEY_M;
	case input::KeyCode::N: return KEY_N;
	case input::KeyCode::O: return KEY_O;
	case input::KeyCode::P: return KEY_P;
	case input::KeyCode::Q: return KEY_Q;
	case input::KeyCode::R: return KEY_R;
	case input::KeyCode::S: return KEY_S;
	case input::KeyCode::T: return KEY_T;
	case input::KeyCode::U: return KEY_U;
	case input::KeyCode::V: return KEY_V;
	case input::KeyCode::W: return KEY_W;
	case input::KeyCode::X: return KEY_X;
	case input::KeyCode::Y: return KEY_Y;
	case input::KeyCode::Z: return KEY_Z;

	// Navigation/Special keys
	case input::KeyCode::Escape: return KEY_ESCAPE;
	case input::KeyCode::Return: return KEY_ENTER;
	case input::KeyCode::Tab: return KEY_TAB;
	case input::KeyCode::Backspace: return KEY_BACKSPACE;
	case input::KeyCode::Insert: return KEY_INSERT;
	case input::KeyCode::Delete: return KEY_DELETE;

	// Arrow keys
	case input::KeyCode::Right: return KEY_RIGHT;
	case input::KeyCode::Left: return KEY_LEFT;
	case input::KeyCode::Down: return KEY_DOWN;
	case input::KeyCode::Up: return KEY_UP;

	// Navigation keys
	case input::KeyCode::PageUp: return KEY_PAGE_UP;
	case input::KeyCode::PageDown: return KEY_PAGE_DOWN;
	case input::KeyCode::Home: return KEY_HOME;
	case input::KeyCode::End: return KEY_END;

	// Lock keys
	case input::KeyCode::CapsLock: return KEY_CAPS_LOCK;
	case input::KeyCode::ScrollLock: return KEY_SCROLL_LOCK;
	case input::KeyCode::NumLock: return KEY_NUM_LOCK;
	case input::KeyCode::PrintScreen: return KEY_PRINT_SCREEN;
	case input::KeyCode::Pause: return KEY_PAUSE;

	// Function keys
	case input::KeyCode::F1: return KEY_F1;
	case input::KeyCode::F2: return KEY_F2;
	case input::KeyCode::F3: return KEY_F3;
	case input::KeyCode::F4: return KEY_F4;
	case input::KeyCode::F5: return KEY_F5;
	case input::KeyCode::F6: return KEY_F6;
	case input::KeyCode::F7: return KEY_F7;
	case input::KeyCode::F8: return KEY_F8;
	case input::KeyCode::F9: return KEY_F9;
	case input::KeyCode::F10: return KEY_F10;
	case input::KeyCode::F11: return KEY_F11;
	case input::KeyCode::F12: return KEY_F12;

	// Modifier keys
	case input::KeyCode::LeftShift: return KEY_LEFT_SHIFT;
	case input::KeyCode::LeftCtrl: return KEY_LEFT_CONTROL;
	case input::KeyCode::LeftAlt: return KEY_LEFT_ALT;
	case input::KeyCode::LeftCmd: return KEY_LEFT_SUPER;
	case input::KeyCode::RightShift: return KEY_RIGHT_SHIFT;
	case input::KeyCode::RightCtrl: return KEY_RIGHT_CONTROL;
	case input::KeyCode::RightAlt: return KEY_RIGHT_ALT;
	case input::KeyCode::RightCmd: return KEY_RIGHT_SUPER;

	// Numpad keys
	case input::KeyCode::NumPad0: return KEY_KP_0;
	case input::KeyCode::NumPad1: return KEY_KP_1;
	case input::KeyCode::NumPad2: return KEY_KP_2;
	case input::KeyCode::NumPad3: return KEY_KP_3;
	case input::KeyCode::NumPad4: return KEY_KP_4;
	case input::KeyCode::NumPad5: return KEY_KP_5;
	case input::KeyCode::NumPad6: return KEY_KP_6;
	case input::KeyCode::NumPad7: return KEY_KP_7;
	case input::KeyCode::NumPad8: return KEY_KP_8;
	case input::KeyCode::NumPad9: return KEY_KP_9;
	case input::KeyCode::NumPadDecimal: return KEY_KP_DECIMAL;
	case input::KeyCode::NumPadDivide: return KEY_KP_DIVIDE;
	case input::KeyCode::NumPadMultiply: return KEY_KP_MULTIPLY;
	case input::KeyCode::NumPadSubtract: return KEY_KP_SUBTRACT;
	case input::KeyCode::NumPadAdd: return KEY_KP_ADD;
	case input::KeyCode::NumPadEnter: return KEY_KP_ENTER;

	default: return 0;
	}
}

// Builds the Phong shader, cached primitive meshes, shared material, and white fallback texture.
// Safe to call once GL is initialised; a shader load failure leaves g_lit.ready false so the
// draw paths fall back to unlit immediate-mode rendering.
void InitLitResources() {
	if (g_lit.ready) return;

#if defined(__EMSCRIPTEN__)
	const std::string dir = std::string("/assets/shaders/") + kGlslDir;
#else
	const std::string dir = std::string(GetApplicationDirectory()) + "assets/shaders/" + kGlslDir;
#endif
	const std::string vs = dir + "/lit.vs";
	const std::string fs = dir + "/lit.fs";

	g_lit.shader = LoadShader(vs.c_str(), fs.c_str());
	if (g_lit.shader.id == rlGetShaderIdDefault()) {
		TraceLog(LOG_WARNING, "[RaylibBackend] Lit shader failed to load; falling back to unlit rendering");
		return;
	}

	// raylib binds matModel/matNormal/mvp/colDiffuse/texture0 automatically by name; resolve the
	// custom lighting uniforms here.
	g_lit.shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(g_lit.shader, "viewPos");
	g_lit.loc_view_pos = g_lit.shader.locs[SHADER_LOC_VECTOR_VIEW];
	g_lit.loc_light_dir = GetShaderLocation(g_lit.shader, "lightDir");
	g_lit.loc_light_color = GetShaderLocation(g_lit.shader, "lightColor");
	g_lit.loc_ambient = GetShaderLocation(g_lit.shader, "ambient");
	g_lit.loc_specular = GetShaderLocation(g_lit.shader, "specularStrength");
	g_lit.loc_shininess = GetShaderLocation(g_lit.shader, "shininess");
	g_lit.loc_shadow_pass = GetShaderLocation(g_lit.shader, "shadowPass");
	g_lit.loc_unlit = GetShaderLocation(g_lit.shader, "unlit");
	g_lit.loc_shadow_color = GetShaderLocation(g_lit.shader, "shadowColor");

	const Image white_img = GenImageColor(1, 1, WHITE);
	g_lit.white = LoadTextureFromImage(white_img);
	UnloadImage(white_img);

	g_lit.material = LoadMaterialDefault();
	g_lit.material.shader = g_lit.shader;
	g_lit.material.maps[MATERIAL_MAP_DIFFUSE].texture = g_lit.white;

	g_lit.cube = GenMeshCube(1.0f, 1.0f, 1.0f);
	g_lit.sphere = GenMeshSphere(1.0f, 24, 24);
	g_lit.plane = GenMeshPlane(1.0f, 1.0f, 1, 1);
	g_lit.cylinder = GenMeshCylinder(1.0f, 1.0f, 24);

	g_lit.ready = true;
}

void ShutdownLitResources() {
	if (!g_lit.ready) return;
	UnloadMesh(g_lit.cube);
	UnloadMesh(g_lit.sphere);
	UnloadMesh(g_lit.plane);
	UnloadMesh(g_lit.cylinder);
	UnloadTexture(g_lit.white);
	// Material shares g_lit.shader; unload the shader once and clear the material's copy so
	// UnloadMaterial doesn't double-free it (the default map textures are shared, too).
	UnloadShader(g_lit.shader);
	g_lit = LitState{};
}

// Draws a cached mesh with the lit material, then an optional planar projected shadow.
void DrawLitMeshWithFallbackTexture(
	const Mesh& mesh,
	const glm::mat4& model,
	const DrawMaterial& material,
	const Texture2D& fallback_texture
) {
	const Rgba color = material.tint;
	const int texture = material.texture;
	const bool cast_shadow = material.cast_shadow;
	const bool wireframe = material.wireframe;

	g_lit.material.maps[MATERIAL_MAP_DIFFUSE].color = ToRay(color);
	g_lit.material.maps[MATERIAL_MAP_DIFFUSE].texture = ResolveTextureHandle(texture, fallback_texture);

	constexpr float kOff = 0.0f;
	constexpr float kOn = 1.0f;
	SetShaderValue(g_lit.shader, g_lit.loc_shadow_pass, &kOff, SHADER_UNIFORM_FLOAT);
	const float unlit = material.unlit ? kOn : kOff;
	SetShaderValue(g_lit.shader, g_lit.loc_unlit, &unlit, SHADER_UNIFORM_FLOAT);

	if (wireframe) {
		DrawMeshWithMaterial(mesh, g_lit.material, model, true);
		return;
	}

	DrawMeshWithMaterial(mesh, g_lit.material, model, false);

	if (cast_shadow && g_lit.shadows_enabled) {
		const glm::mat4 shadow_model = g_lit.shadow_matrix * model;
		SetShaderValue(g_lit.shader, g_lit.loc_shadow_pass, &kOn, SHADER_UNIFORM_FLOAT);
		rlDisableDepthMask(); // shadows blend without fighting the receiver's depth
		DrawMeshWithMaterial(mesh, g_lit.material, shadow_model, false);
		rlEnableDepthMask();
		SetShaderValue(g_lit.shader, g_lit.loc_shadow_pass, &kOff, SHADER_UNIFORM_FLOAT);
	}
}

void DrawLitMesh(const Mesh& mesh, const glm::mat4& model, const DrawMaterial& material) {
	DrawLitMeshWithFallbackTexture(mesh, model, material, g_lit.white);
}

bool DrawCustomMesh(
	const Mesh& mesh,
	const glm::mat4& model,
	const DrawMaterial& material,
	const Texture2D& fallback_texture
) {
	const auto* entry = ResolveCustomShaderEntry(material.shader);
	if (entry == nullptr) {
		return false;
	}

	entry->material.maps[MATERIAL_MAP_DIFFUSE].color = ToRay(material.tint);
	entry->material.maps[MATERIAL_MAP_DIFFUSE].texture =
		ResolveTextureHandle(material.texture, fallback_texture.id != 0 ? fallback_texture : entry->white);
	ApplyOcclusionFade(*entry, material.fade_transparent);
	DrawMeshWithMaterial(mesh, entry->material, model, material.wireframe);
	return true;
}

bool DrawCustomMesh(const Mesh& mesh, const glm::mat4& model, const DrawMaterial& material) {
	if (const auto* entry = ResolveCustomShaderEntry(material.shader)) {
		entry->material.maps[MATERIAL_MAP_DIFFUSE].color = ToRay(material.tint);
		entry->material.maps[MATERIAL_MAP_DIFFUSE].texture = ResolveTextureHandle(material.texture, entry->white);
		ApplyOcclusionFade(*entry, material.fade_transparent);
		DrawMeshWithMaterial(mesh, entry->material, model, material.wireframe);
		return true;
	}
	return false;
}

// Builds the instanced lit shader and registers the built-in unit primitive meshes used for
// GPU-instanced batching. Call after InitLitResources; on shader-load failure g_inst.ready stays
// false and SubmitBatch falls back to per-item immediate draws.
void InitInstancedResources() {
	if (g_inst.ready) return;

#if defined(__EMSCRIPTEN__)
	const std::string dir = std::string("/assets/shaders/") + kGlslDir;
#else
	const std::string dir = std::string(GetApplicationDirectory()) + "assets/shaders/" + kGlslDir;
#endif
	const std::string vs = dir + "/lit_inst.vs";
	const std::string fs = dir + "/lit.fs";

	g_inst.shader = LoadShader(vs.c_str(), fs.c_str());
	if (g_inst.shader.id == rlGetShaderIdDefault()) {
		TraceLog(LOG_WARNING, "[RaylibBackend] Instanced shader failed to load; batching uses per-item draws");
		return;
	}

	// Per-instance attribute locations are assigned by the linker; query them by name.
	g_inst.attr_transform = GetShaderLocationAttrib(g_inst.shader, "instanceTransform");
	g_inst.attr_color = GetShaderLocationAttrib(g_inst.shader, "instanceColor");
	// raylib auto-binds mvp/colDiffuse/texture0 into shader.locs; the rest are custom uniforms.
	g_inst.loc_mvp = g_inst.shader.locs[SHADER_LOC_MATRIX_MVP];
	g_inst.loc_col_diffuse = g_inst.shader.locs[SHADER_LOC_COLOR_DIFFUSE];
	g_inst.loc_light_dir = GetShaderLocation(g_inst.shader, "lightDir");
	g_inst.loc_light_color = GetShaderLocation(g_inst.shader, "lightColor");
	g_inst.loc_ambient = GetShaderLocation(g_inst.shader, "ambient");
	g_inst.loc_view_pos = GetShaderLocation(g_inst.shader, "viewPos");
	g_inst.loc_specular = GetShaderLocation(g_inst.shader, "specularStrength");
	g_inst.loc_shininess = GetShaderLocation(g_inst.shader, "shininess");
	g_inst.loc_shadow_pass = GetShaderLocation(g_inst.shader, "shadowPass");
	g_inst.loc_unlit = GetShaderLocation(g_inst.shader, "unlit");
	g_inst.loc_shadow_color = GetShaderLocation(g_inst.shader, "shadowColor");
	g_inst.loc_shadow_matrix = GetShaderLocation(g_inst.shader, "shadowMatrix");

	if (g_inst.attr_transform < 0 || g_inst.attr_color < 0) {
		TraceLog(LOG_WARNING, "[RaylibBackend] Instanced shader missing per-instance attributes; batching disabled");
		UnloadShader(g_inst.shader);
		g_inst = InstancedState{};
		return;
	}

	// Register the built-in primitives. Order MUST match PrimitiveShape. These meshes are dedicated
	// to instancing so their VAO instance-attribute state never affects the immediate-mode path.
	g_render_meshes.clear();
	g_render_meshes.push_back({.mesh = GenMeshCube(1.0f, 1.0f, 1.0f)});   // PrimitiveShape::Cube
	g_render_meshes.push_back({.mesh = GenMeshSphere(1.0f, 24, 24)});     // PrimitiveShape::Sphere
	g_render_meshes.push_back({.mesh = GenMeshPlane(1.0f, 1.0f, 1, 1)});  // PrimitiveShape::Plane
	g_render_meshes.push_back({.mesh = GenMeshCylinder(1.0f, 1.0f, 24)}); // PrimitiveShape::Cylinder

	g_inst.ready = true;
}

void ShutdownInstancedResources() {
	if (!g_inst.ready) return;
	for (RenderMesh& rm : g_render_meshes) {
		if (rm.transform_vbo != 0) rlUnloadVertexBuffer(rm.transform_vbo);
		if (rm.color_vbo != 0) rlUnloadVertexBuffer(rm.color_vbo);
		if (rm.mesh.vertexCount != 0) UnloadMesh(rm.mesh);
	}
	g_render_meshes.clear();
	UnloadShader(g_inst.shader);
	g_inst = InstancedState{};
}

// Pushes the cached scene lighting onto the instanced shader (a separate program from g_lit, so it
// needs its own uniform uploads).
void UploadInstancedLighting() {
	const glm::vec3 dir = glm::normalize(g_light_params.direction);
	const float dir_v[3] = {dir.x, dir.y, dir.z};
	SetShaderValue(g_inst.shader, g_inst.loc_light_dir, dir_v, SHADER_UNIFORM_VEC3);

	const float light_col[4] = {
		static_cast<float>(g_light_params.light_color.r) / 255.0f,
		static_cast<float>(g_light_params.light_color.g) / 255.0f,
		static_cast<float>(g_light_params.light_color.b) / 255.0f,
		g_light_params.light_intensity,
	};
	SetShaderValue(g_inst.shader, g_inst.loc_light_color, light_col, SHADER_UNIFORM_VEC4);

	const float ambient[4] = {
		static_cast<float>(g_light_params.ambient_color.r) / 255.0f,
		static_cast<float>(g_light_params.ambient_color.g) / 255.0f,
		static_cast<float>(g_light_params.ambient_color.b) / 255.0f,
		g_light_params.ambient_intensity,
	};
	SetShaderValue(g_inst.shader, g_inst.loc_ambient, ambient, SHADER_UNIFORM_VEC4);

	const float view_pos[3] = {g_view_pos.x, g_view_pos.y, g_view_pos.z};
	SetShaderValue(g_inst.shader, g_inst.loc_view_pos, view_pos, SHADER_UNIFORM_VEC3);
	SetShaderValue(g_inst.shader, g_inst.loc_specular, &g_light_params.specular_strength, SHADER_UNIFORM_FLOAT);
	SetShaderValue(g_inst.shader, g_inst.loc_shininess, &g_light_params.shininess, SHADER_UNIFORM_FLOAT);
}

// (Re)allocates the per-instance VBOs on a RenderMesh when capacity must grow, attaching them to the
// mesh VAO with attribute divisor 1. instanceTransform occupies 4 consecutive vec4 attribute slots.
void EnsureInstanceBuffers(RenderMesh& rm, const int count) {
	if (count <= rm.instance_capacity && rm.transform_vbo != 0) return;

	rlEnableVertexArray(rm.mesh.vaoId);

	if (rm.transform_vbo != 0) rlUnloadVertexBuffer(rm.transform_vbo);
	if (rm.color_vbo != 0) rlUnloadVertexBuffer(rm.color_vbo);

	const auto tx = static_cast<unsigned int>(g_inst.attr_transform);
	rm.transform_vbo = rlLoadVertexBuffer(nullptr, count * static_cast<int>(sizeof(glm::mat4)), true);
	for (unsigned int c = 0; c < 4; ++c) {
		rlSetVertexAttribute(tx + c, 4, RL_FLOAT, false, sizeof(glm::mat4), static_cast<int>(c * sizeof(glm::vec4)));
		rlSetVertexAttributeDivisor(tx + c, 1);
		rlEnableVertexAttribute(tx + c);
	}

	const auto cl = static_cast<unsigned int>(g_inst.attr_color);
	rm.color_vbo = rlLoadVertexBuffer(nullptr, count * static_cast<int>(sizeof(glm::vec4)), true);
	rlSetVertexAttribute(cl, 4, RL_FLOAT, false, sizeof(glm::vec4), 0);
	rlSetVertexAttributeDivisor(cl, 1);
	rlEnableVertexAttribute(cl);

	rm.instance_capacity = count;
	rlDisableVertexArray();
}

// Issues one instanced draw (plus an optional planar-shadow pass) for a group of same-key commands.
// Returns the number of GL draw calls made.
int DrawInstancedGroup(
	RenderMesh& rm,
	const DrawMaterial& material,
	const std::vector<glm::mat4>& transforms,
	const std::vector<glm::vec4>& colors
) {
	const int count = static_cast<int>(transforms.size());
	if (count == 0) return 0;

	EnsureInstanceBuffers(rm, count);
	rlUpdateVertexBuffer(rm.transform_vbo, transforms.data(), count * static_cast<int>(sizeof(glm::mat4)), 0);
	rlUpdateVertexBuffer(rm.color_vbo, colors.data(), count * static_cast<int>(sizeof(glm::vec4)), 0);

	rlEnableShader(g_inst.shader.id);

	// mvp = view * projection; the per-instance model matrix is applied inside the shader.
	const Matrix mat_view = rlGetMatrixModelview();
	const Matrix mat_proj = rlGetMatrixProjection();
	rlSetUniformMatrix(g_inst.loc_mvp, MatrixMultiply(mat_view, mat_proj));

	UploadInstancedLighting();

	// The tint is carried per-instance, so colDiffuse stays white.
	const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	if (g_inst.loc_col_diffuse >= 0) {
		SetShaderValue(g_inst.shader, g_inst.loc_col_diffuse, white, SHADER_UNIFORM_VEC4);
	}

	// Bind the albedo texture (or the 1x1 white fallback) to sampler slot 0.
	const Texture2D tex = ResolveTextureHandle(material.texture, g_lit.white);
	rlActiveTextureSlot(0);
	rlEnableTexture(tex.id);
	const int slot0 = 0;
	if (g_inst.shader.locs[SHADER_LOC_MAP_DIFFUSE] >= 0) {
		SetShaderValue(g_inst.shader, g_inst.shader.locs[SHADER_LOC_MAP_DIFFUSE], &slot0, SHADER_UNIFORM_INT);
	}

	constexpr float kOff = 0.0f;
	constexpr float kOn = 1.0f;
	SetShaderValue(g_inst.shader, g_inst.loc_shadow_pass, &kOff, SHADER_UNIFORM_FLOAT);
	const float unlit = material.unlit ? kOn : kOff;
	SetShaderValue(g_inst.shader, g_inst.loc_unlit, &unlit, SHADER_UNIFORM_FLOAT);

	auto issue = [&] {
		rlEnableVertexArray(rm.mesh.vaoId);
		if (rm.mesh.indices != nullptr) {
			rlDrawVertexArrayElementsInstanced(0, rm.mesh.triangleCount * 3, nullptr, count);
		}
		else {
			rlDrawVertexArrayInstanced(0, rm.mesh.vertexCount, count);
		}
		rlDisableVertexArray();
	};

	issue();
	int draws = 1;

	if (material.cast_shadow && g_lit.shadows_enabled) {
		rlSetUniformMatrix(g_inst.loc_shadow_matrix, ToRayMatrix(g_lit.shadow_matrix));
		const float shadow_col[4] = {
			static_cast<float>(g_light_params.shadow_color.r) / 255.0f,
			static_cast<float>(g_light_params.shadow_color.g) / 255.0f,
			static_cast<float>(g_light_params.shadow_color.b) / 255.0f,
			static_cast<float>(g_light_params.shadow_color.a) / 255.0f,
		};
		SetShaderValue(g_inst.shader, g_inst.loc_shadow_color, shadow_col, SHADER_UNIFORM_VEC4);
		SetShaderValue(g_inst.shader, g_inst.loc_shadow_pass, &kOn, SHADER_UNIFORM_FLOAT);
		rlDisableDepthMask();
		issue();
		rlEnableDepthMask();
		SetShaderValue(g_inst.shader, g_inst.loc_shadow_pass, &kOff, SHADER_UNIFORM_FLOAT);
		++draws;
	}

	rlDisableTexture();
	rlDisableShader();
	return draws;
}
} // namespace

void RaylibBackend::Init(const int width, const int height, const std::string_view title) {
	InitWindow(width, height, std::string(title).c_str());
	SetExitKey(KEY_NULL); // we use Esc for pause, not window-close
	SetTargetFPS(60);
	InitAudioDevice();
	InitLitResources();
	InitInstancedResources();
#ifdef ENGINE_DEBUG_UI
	g_debug_ui = std::make_unique<ImGuiDebugUi>();
	rlimgui::Setup();
#endif
}

bool RaylibBackend::ShouldClose() const { return WindowShouldClose(); }

void RaylibBackend::Shutdown() {
#ifdef ENGINE_DEBUG_UI
	rlimgui::Shutdown();
	g_debug_ui.reset();
#endif
	ShutdownInstancedResources();
	ShutdownLitResources();
	for (const Texture2D& t : g_textures) {
		if (t.id != 0) {
			::UnloadTexture(t);
		}
	}
	g_textures.clear();
	g_texture_paths.clear();
	if (g_text_font.texture.id != 0) {
		UnloadFont(g_text_font);
		g_text_font = {};
	}
	for (const Sound& s : g_sounds) {
		if (s.frameCount != 0) {
			::UnloadSound(s);
		}
	}
	g_sounds.clear();
	for (const Music& m : g_music) {
		if (m.frameCount != 0) {
			::UnloadMusicStream(m);
		}
	}
	g_music.clear();
	g_music_paths.clear();
	for (const Model& m : g_models) {
		if (m.meshCount != 0) {
			::UnloadModel(m);
		}
	}
	g_models.clear();
	// Defensive: dynamic meshes are normally freed by the DeleteDynamicMesh observer when the world
	// is destroyed (before this runs), but free any survivors so a future teardown-order change can't
	// silently leak GPU buffers. UnloadDynamicMesh's guard makes double-free impossible.
	for (Mesh& dm : g_dynamic_meshes) {
		if (dm.vertexCount != 0) {
			::UnloadMesh(dm);
			dm.vertexCount = 0;
		}
	}
	g_dynamic_meshes.clear();
	for (CustomShaderEntry& e : g_custom_shaders) {
		if (e.ready) {
			::UnloadTexture(e.white);
			::UnloadShader(e.shader);
			MemFree(e.material.maps);
			e = CustomShaderEntry{};
		}
	}
	g_custom_shaders.clear();
	StopSynth();
	CloseAudioDevice();
	CloseWindow();
}

int RaylibBackend::Width() const { return GetScreenWidth(); }

int RaylibBackend::Height() const { return GetScreenHeight(); }

float RaylibBackend::DeltaTime() const { return GetFrameTime(); }

double RaylibBackend::Time() const { return GetTime(); }

int RaylibBackend::Fps() const { return GetFPS(); }

std::string RaylibBackend::AssetDirectory() const {
#if defined(__EMSCRIPTEN__)
	return "/assets";
#else
	return std::string(GetApplicationDirectory()) + "assets";
#endif
}

void RaylibBackend::BeginFrame(const Rgba clear) {
	profiling::Zone _zone{"Platform::BeginFrame"};
	BeginDrawing();
	ClearBackground(ToRay(clear));

	g_frame_chars.clear();
	g_frame_char_next = 0;
	for (int ch = GetCharPressed(); ch != 0; ch = GetCharPressed()) {
		g_frame_chars.push_back(ch);
	}

#ifdef ENGINE_DEBUG_UI
	// Open the debug UI frame before the world progresses, so panel systems can run in any phase
	// and InputModule can withhold captured input. The toggle key is reserved (see IsKeyDown); it is
	// ignored while typing into an ImGui field, and the toggle frame's typed characters are dropped
	// so it never lands in a text field.
	if (!g_debug_ui->CapturesKeyboard() && ::IsKeyPressed(KeyCodeToRay(input::kDebugUiToggleKey))) {
		g_debug_ui->SetVisible(!g_debug_ui->IsVisible());
		g_frame_chars.clear();
	}
	if (g_debug_ui->IsVisible()) {
		rlimgui::QueueInput(g_frame_chars);
	}
	g_debug_ui->BeginFrame({static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())}, GetFrameTime());
#endif
}

void RaylibBackend::EndFrame() {
	profiling::Zone _zone{"Platform::EndFrame"}; // includes GPU present/vsync wait
#ifdef ENGINE_DEBUG_UI
	// Close a frame DrawDebugUi never rendered (e.g. the OnDebugUI phase didn't run) so ImGui's
	// NewFrame/Render pairing holds; nothing is drawn.
	g_debug_ui->EndFrame();
#endif
	EndDrawing();
}

DebugUi& RaylibBackend::GetDebugUi() {
#ifdef ENGINE_DEBUG_UI
	if (g_debug_ui) {
		return *g_debug_ui;
	}
#endif
	return g_null_debug_ui;
}

void RaylibBackend::DrawDebugUi() {
#ifdef ENGINE_DEBUG_UI
	profiling::Zone _zone{"Platform::DrawDebugUi"};
	if (g_debug_ui && g_debug_ui->EndFrame()) {
		rlimgui::RenderDrawData();
	}
#endif
}

glm::vec2 RaylibBackend::MousePosition() const {
	const auto [x, y] = GetMousePosition();
	return glm::vec2{x, y};
}

glm::vec2 RaylibBackend::MouseDelta() const {
	const auto [x, y] = GetMouseDelta();
	return glm::vec2{x, y};
}

glm::vec2 RaylibBackend::MouseWheel() const {
	const auto [x, y] = GetMouseWheelMoveV();
	return glm::vec2{x, y};
}

bool RaylibBackend::MousePressed(const MouseButton button) const { return IsMouseButtonPressed(MouseToRay(button)); }

bool RaylibBackend::MouseReleased(const MouseButton button) const { return IsMouseButtonReleased(MouseToRay(button)); }

bool RaylibBackend::MouseDown(const MouseButton button) const { return IsMouseButtonDown(MouseToRay(button)); }

bool RaylibBackend::IsKeyDown(const int key_code) const {
	// Map our platform-agnostic key codes to Raylib key codes
	// For now, support ASCII keys and special keys from Raylib
	return !IsReservedKey(key_code) && ::IsKeyDown(KeyCodeToRay(key_code));
}

bool RaylibBackend::IsKeyPressedRepeat(const int key_code) const {
	return !IsReservedKey(key_code)
		&& (::IsKeyPressed(KeyCodeToRay(key_code)) || ::IsKeyPressedRepeat(KeyCodeToRay(key_code)));
}

bool RaylibBackend::IsMouseButtonDown(const int button) const {
	// 0 = left, 1 = right, 2 = middle
	switch (button) {
	case 0: return ::IsMouseButtonDown(MOUSE_BUTTON_LEFT);
	case 1: return ::IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
	case 2: return ::IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
	default: return false;
	}
}

int RaylibBackend::CharInput() const {
	// Returns the next codepoint typed this frame, or 0 once the frame's characters are used up.
	return g_frame_char_next < g_frame_chars.size() ? g_frame_chars[g_frame_char_next++] : 0;
}

std::string RaylibBackend::TextInput() const {
	// Returns the remaining printable characters typed this frame (shares the CharInput cursor).
	std::string result;
	int ch;
	while ((ch = CharInput()) != 0) {
		// Include all printable characters and common control chars (newline, tab)
		if ((ch >= 32 && ch <= 126) || ch == '\n' || ch == '\t') {
			result += static_cast<char>(ch);
		}
	}
	return result;
}

void RaylibBackend::DrawRect(const Rect r, const Rgba c) { DrawRectangleRec(ToRay(r), ToRay(c)); }

void RaylibBackend::DrawRectLines(const Rect r, const float thickness, const Rgba c) {
	DrawRectangleLinesEx(ToRay(r), thickness, ToRay(c));
}

void RaylibBackend::DrawRoundedRect(const Rect r, const float roundness, const Rgba c) {
	DrawRectangleRounded(ToRay(r), roundness, 8, ToRay(c));
}

void RaylibBackend::DrawRoundedRectLines(const Rect r, const float roundness, const float thickness, const Rgba c) {
	DrawRectangleRoundedLinesEx(ToRay(r), roundness, 8, thickness, ToRay(c));
}

void RaylibBackend::DrawText(
	const std::string_view text,
	const float x,
	const float y,
	const float size,
	const Rgba c
) {
	if (g_text_font.texture.id == 0) {
		::DrawText(std::string(text).c_str(), static_cast<int>(x), static_cast<int>(y), static_cast<int>(size), ToRay(c));
		return;
	}
	DrawTextEx(g_text_font, std::string(text).c_str(), Vector2{.x = x, .y = y}, size, size * kTextFontSpacing, ToRay(c));
}

float RaylibBackend::MeasureText(const std::string_view text, const float size) const {
	if (g_text_font.texture.id == 0) {
		return static_cast<float>(::MeasureText(std::string(text).c_str(), static_cast<int>(size)));
	}
	return MeasureTextEx(g_text_font, std::string(text).c_str(), size, size * kTextFontSpacing).x;
}

bool RaylibBackend::SetTextFont(const std::string_view path) {
	// Printable ASCII and Latin-1 (accented letters, « », ×, ·), so text isn't limited to plain ASCII.
	std::vector<int> codepoints;
	for (int c = 32; c <= 255; ++c) {
		if (c < 127 || c >= 160) {
			codepoints.push_back(c);
		}
	}
	const Font font = LoadFontEx(std::string(path).c_str(), kTextFontBaseSize, codepoints.data(), static_cast<int>(codepoints.size()));
	// LoadFontEx falls back to raylib's default font (not a failure it reports) when the file is missing.
	if (font.texture.id == 0 || font.texture.id == GetFontDefault().texture.id) {
		spdlog::warn("[Platform] Could not load font '{}'; keeping the current one", path);
		return false;
	}
	if (g_text_font.texture.id != 0) {
		UnloadFont(g_text_font);
	}
	g_text_font = font;
	// Rasterised once at a large size, so mipmaps keep small UI text from shimmering when scaled down.
	GenTextureMipmaps(&g_text_font.texture);
	::SetTextureFilter(g_text_font.texture, TEXTURE_FILTER_TRILINEAR);
	return true;
}

void RaylibBackend::DrawLine(const glm::vec2 a, const glm::vec2 b, const float thickness, const Rgba c) {
	DrawLineEx(ToRay(a), ToRay(b), thickness, ToRay(c));
}

void RaylibBackend::DrawCircle(const glm::vec2 center, const float radius, const Rgba c) {
	DrawCircleV(ToRay(center), radius, ToRay(c));
}

void RaylibBackend::DrawTexturedRect(const int texture, const Rect dest, const Rgba tint) {
	if (texture < 0 || texture >= static_cast<int>(g_textures.size())) {
		DrawRectangleRec(ToRay(dest), ToRay(tint));
		return;
	}
	const Texture2D& tex = g_textures[static_cast<std::size_t>(texture)];
	// Source rect the size of the destination: texels beyond the texture wrap (default REPEAT), so
	// the art tiles at 1:1 texel:pixel instead of stretching.
	const Rectangle src{.x = 0.0F, .y = 0.0F, .width = dest.w, .height = dest.h};
	const Rectangle dst{.x = dest.x, .y = dest.y, .width = dest.w, .height = dest.h};
	DrawTexturePro(tex, src, dst, Vector2{.x = 0.0F, .y = 0.0F}, 0.0F, ToRay(tint));
}

void RaylibBackend::BeginScissor(const Rect r) {
	BeginScissorMode(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.w), static_cast<int>(r.h));
}

void RaylibBackend::EndScissor() { EndScissorMode(); }

void RaylibBackend::BeginMode3D(const Camera3DParams& camera) {
	profiling::Zone _zone{"Platform::BeginMode3D"};
	const bool orthographic = camera.projection == CameraProjection::Orthographic;
	const Camera3D cam{
		.position = ToRay(camera.position),
		.target = ToRay(camera.target),
		.up = ToRay(camera.up),
		// For orthographic cameras raylib reuses `fovy` as the vertical view height in world units.
		.fovy = orthographic ? camera.ortho_size : camera.fov_y,
		.projection = orthographic ? CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE,
	};
	::BeginMode3D(cam);
	g_camera3d = cam; // cached for GetWorldToScreen in the occluder-fade path

	g_view_pos = camera.position;

	// Feed the camera position to the lit shader for specular highlights.
	if (g_lit.ready) {
		const float view_pos[3] = {camera.position.x, camera.position.y, camera.position.z};
		SetShaderValue(g_lit.shader, g_lit.loc_view_pos, view_pos, SHADER_UNIFORM_VEC3);
	}
}

void RaylibBackend::EndMode3D() {
	profiling::Zone _zone{"Platform::EndMode3D"};
	::EndMode3D();
}

void RaylibBackend::SetLighting(const LightParams& lighting) {
	g_light_params = lighting; // cached so the instanced shader can be fed the same values
	if (!g_lit.ready) return;

	const glm::vec3 dir = glm::normalize(lighting.direction);
	const float dir_v[3] = {dir.x, dir.y, dir.z};
	SetShaderValue(g_lit.shader, g_lit.loc_light_dir, dir_v, SHADER_UNIFORM_VEC3);

	const float light_col[4] = {
		static_cast<float>(lighting.light_color.r) / 255.0f,
		static_cast<float>(lighting.light_color.g) / 255.0f,
		static_cast<float>(lighting.light_color.b) / 255.0f,
		lighting.light_intensity,
	};
	SetShaderValue(g_lit.shader, g_lit.loc_light_color, light_col, SHADER_UNIFORM_VEC4);

	const float ambient[4] = {
		static_cast<float>(lighting.ambient_color.r) / 255.0f,
		static_cast<float>(lighting.ambient_color.g) / 255.0f,
		static_cast<float>(lighting.ambient_color.b) / 255.0f,
		lighting.ambient_intensity,
	};
	SetShaderValue(g_lit.shader, g_lit.loc_ambient, ambient, SHADER_UNIFORM_VEC4);

	SetShaderValue(g_lit.shader, g_lit.loc_specular, &lighting.specular_strength, SHADER_UNIFORM_FLOAT);
	SetShaderValue(g_lit.shader, g_lit.loc_shininess, &lighting.shininess, SHADER_UNIFORM_FLOAT);

	const float shadow_col[4] = {
		static_cast<float>(lighting.shadow_color.r) / 255.0f,
		static_cast<float>(lighting.shadow_color.g) / 255.0f,
		static_cast<float>(lighting.shadow_color.b) / 255.0f,
		static_cast<float>(lighting.shadow_color.a) / 255.0f,
	};
	SetShaderValue(g_lit.shader, g_lit.loc_shadow_color, shadow_col, SHADER_UNIFORM_VEC4);

	// Shadows require a light that points downward onto the ground plane. A small upward bias on
	// the plane keeps projected shadows from z-fighting the receiver surface.
	g_lit.shadows_enabled = lighting.shadows_enabled && dir.y < -0.05f;
	if (g_lit.shadows_enabled) {
		g_lit.shadow_matrix = BuildShadowMatrix(dir, lighting.shadow_ground_y + 0.01f);
	}
}

void RaylibBackend::DrawCube(const glm::mat4& transform, const glm::vec3 size, const DrawMaterial& material) {
	const glm::mat4 model = transform * glm::scale(glm::mat4(1.0f), size);
	if (DrawCustomMesh(g_lit.cube, model, material)) {
		return;
	}
	if (g_lit.ready) {
		DrawLitMesh(g_lit.cube, model, material);
		return;
	}
	rlPushMatrix();
	rlMultMatrixf(glm::value_ptr(transform));
	if (material.wireframe) {
		DrawCubeWiresV(Vector3{.x = 0.0F, .y = 0.0F, .z = 0.0F}, ToRay(size), ToRay(material.tint));
	}
	else {
		DrawCubeV(Vector3{.x = 0.0F, .y = 0.0F, .z = 0.0F}, ToRay(size), ToRay(material.tint));
	}
	rlPopMatrix();
}

void RaylibBackend::DrawSphere(const glm::mat4& transform, const float radius, const DrawMaterial& material) {
	const glm::mat4 model = transform * glm::scale(glm::mat4(1.0f), glm::vec3(radius));
	if (DrawCustomMesh(g_lit.sphere, model, material)) {
		return;
	}
	if (g_lit.ready) {
		DrawLitMesh(g_lit.sphere, model, material);
		return;
	}
	rlPushMatrix();
	rlMultMatrixf(glm::value_ptr(transform));
	if (material.wireframe) {
		DrawSphereWires(Vector3{.x = 0.0F, .y = 0.0F, .z = 0.0F}, radius, 16, 16, ToRay(material.tint));
	}
	else {
		::DrawSphere(Vector3{.x = 0.0F, .y = 0.0F, .z = 0.0F}, radius, ToRay(material.tint));
	}
	rlPopMatrix();
}

void RaylibBackend::DrawQuad(const glm::mat4& transform, const glm::vec2 size, const DrawMaterial& material) {
	const glm::mat4 model = transform * glm::scale(glm::mat4(1.0f), glm::vec3(size.x, 1.0f, size.y));
	if (DrawCustomMesh(g_lit.plane, model, material)) {
		return;
	}
	if (g_lit.ready) {
		DrawLitMesh(g_lit.plane, model, material);
		return;
	}
	rlPushMatrix();
	rlMultMatrixf(glm::value_ptr(transform));
	if (material.wireframe) {
		const float hx = size.x * 0.5F;
		const float hz = size.y * 0.5F;
		const Vector3 a{.x = -hx, .y = 0.0F, .z = -hz};
		const Vector3 b{.x = hx, .y = 0.0F, .z = -hz};
		const Vector3 d{.x = hx, .y = 0.0F, .z = hz};
		const Vector3 e{.x = -hx, .y = 0.0F, .z = hz};
		DrawLine3D(a, b, ToRay(material.tint));
		DrawLine3D(b, d, ToRay(material.tint));
		DrawLine3D(d, e, ToRay(material.tint));
		DrawLine3D(e, a, ToRay(material.tint));
	}
	else {
		DrawPlane(Vector3{.x = 0.0F, .y = 0.0F, .z = 0.0F}, Vector2{.x = size.x, .y = size.y}, ToRay(material.tint));
	}
	rlPopMatrix();
}

void RaylibBackend::DrawCapsule(
	const glm::mat4& transform,
	const float radius,
	const float height,
	const DrawMaterial& material
) {
	const float half = height * 0.5F;
	// Cylinder body: unit cylinder (base at y=0) scaled and shifted so it spans [-half, +half].
	const glm::mat4 body = transform
						   * glm::translate(glm::mat4(1.0f), {0.0f, -half, 0.0f})
						   * glm::scale(glm::mat4(1.0f), {radius, height, radius});
	const glm::mat4 top = transform
						  * glm::translate(glm::mat4(1.0f), {0.0f, half, 0.0f})
						  * glm::scale(glm::mat4(1.0f), glm::vec3(radius));
	const glm::mat4 bottom = transform
							 * glm::translate(glm::mat4(1.0f), {0.0f, -half, 0.0f})
							 * glm::scale(glm::mat4(1.0f), glm::vec3(radius));

	if (DrawCustomMesh(g_lit.cylinder, body, material)) {
		DrawCustomMesh(g_lit.sphere, top, material);
		DrawCustomMesh(g_lit.sphere, bottom, material);
		return;
	}
	if (g_lit.ready) {
		DrawLitMesh(g_lit.cylinder, body, material);
		DrawLitMesh(g_lit.sphere, top, material);
		DrawLitMesh(g_lit.sphere, bottom, material);
		return;
	}
	const Vector3 start{.x = 0.0F, .y = -half, .z = 0.0F};
	const Vector3 end{.x = 0.0F, .y = half, .z = 0.0F};
	rlPushMatrix();
	rlMultMatrixf(glm::value_ptr(transform));
	if (material.wireframe) {
		DrawCapsuleWires(start, end, radius, 16, 8, ToRay(material.tint));
	}
	else {
		::DrawCapsule(start, end, radius, 16, 8, ToRay(material.tint));
	}
	rlPopMatrix();
}

void RaylibBackend::DrawMesh(const int handle, const glm::mat4& transform, const DrawMaterial& material) {
	if (handle < 0 || handle >= static_cast<int>(g_models.size())) {
		return;
	}
	Model& model = g_models[static_cast<size_t>(handle)];

	if (ResolveCustomShaderEntry(material.shader) != nullptr || g_lit.ready) {
		auto draw_meshes = [&](auto&& draw_fn) {
			for (int i = 0; i < model.meshCount; ++i) {
				Texture2D fallback = g_lit.white;
				if (model.meshMaterial != nullptr
					&& model.materials != nullptr
					&& model.materialCount > 0
					&& model.meshMaterial[i] >= 0
					&& model.meshMaterial[i] < model.materialCount) {
					const Texture2D source = model.materials[model.meshMaterial[i]].maps[MATERIAL_MAP_DIFFUSE].texture;
					if (source.id != 0) {
						fallback = source;
					}
				}
				draw_fn(model.meshes[i], fallback);
			}
		};

		if (ResolveCustomShaderEntry(material.shader) != nullptr) {
			draw_meshes([&](const Mesh& mesh, const Texture2D& fallback) {
				DrawCustomMesh(mesh, transform, material, fallback);
			});
			return;
		}

		constexpr float off = 0.0f;
		SetShaderValue(g_lit.shader, g_lit.loc_shadow_pass, &off, SHADER_UNIFORM_FLOAT);

		draw_meshes([&](const Mesh& mesh, const Texture2D& fallback) {
			DrawLitMeshWithFallbackTexture(mesh, transform, material, fallback);
		});
		return;
	}

	// Optional per-draw diffuse texture override on the model's primary material for the default path.
	Texture2D original_texture{};
	bool restore_texture = false;
	if (material.texture >= 0 && material.texture < static_cast<int>(g_textures.size()) && model.materialCount > 0) {
		original_texture = model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture;
		model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = g_textures[static_cast<size_t>(material.texture)];
		restore_texture = true;
	}

	model.transform = ToRayMatrix(transform);
	if (material.wireframe) {
		rlEnableWireMode();
	}
	DrawModel(model, Vector3{.x = 0.0F, .y = 0.0F, .z = 0.0F}, 1.0F, ToRay(material.tint));
	if (material.wireframe) {
		rlDisableWireMode();
	}
	model.transform = ToRayMatrix(glm::mat4(1.0f));
	if (restore_texture) {
		model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = original_texture;
	}
}

int RaylibBackend::PrimitiveMesh(const PrimitiveShape shape) const {
	const auto index = static_cast<int>(shape);
	return index < static_cast<int>(g_render_meshes.size()) ? index : -1;
}

BatchStats RaylibBackend::SubmitBatch(const std::span<const RenderCommand> commands) {
	profiling::Zone _zone{"Platform::SubmitBatch"};
	BatchStats stats{};
	if (commands.empty()) return stats;

	const auto t0 = std::chrono::steady_clock::now();
	std::vector<uint32_t> order;
	const std::vector<RenderBatchGroup> groups = GroupRenderCommands(commands, order);
	const auto t1 = std::chrono::steady_clock::now();
	stats.group_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

	// Scratch buffers reused across groups to avoid per-group allocation.
	std::vector<glm::mat4> transforms;
	std::vector<glm::vec4> colors;

	for (const RenderBatchGroup& group : groups) {
		const RenderCommand& head = commands[order[group.begin]];
		const DrawMaterial& material = head.material;
		RenderMesh* rm = (head.mesh >= 0 && head.mesh < static_cast<int>(g_render_meshes.size()))
							 ? &g_render_meshes[static_cast<size_t>(head.mesh)]
							 : nullptr;

		// Instancing handles the default lit path (no custom shader, not wireframe). Everything else
		// falls back to per-item immediate draws, which already support those cases.
		const bool instanceable = rm != nullptr && g_inst.ready && material.shader < 0 && !material.wireframe;

		if (!instanceable) {
			for (uint32_t k = 0; k < group.count; ++k) {
				const RenderCommand& cmd = commands[order[group.begin + k]];
				if (rm == nullptr) continue;
				if (!DrawCustomMesh(rm->mesh, cmd.transform, cmd.material)) {
					if (g_lit.ready) {
						DrawLitMesh(rm->mesh, cmd.transform, cmd.material);
					}
				}
				stats.draw_calls += 1;
				stats.instances += 1;
			}
			continue;
		}

		transforms.clear();
		colors.clear();
		transforms.reserve(group.count);
		colors.reserve(group.count);
		for (uint32_t k = 0; k < group.count; ++k) {
			const RenderCommand& cmd = commands[order[group.begin + k]];
			transforms.push_back(cmd.transform);
			const core::Rgba t = cmd.material.tint;
			colors.emplace_back(
				static_cast<float>(t.r) / 255.0f,
				static_cast<float>(t.g) / 255.0f,
				static_cast<float>(t.b) / 255.0f,
				static_cast<float>(t.a) / 255.0f
			);
		}

		stats.draw_calls += DrawInstancedGroup(*rm, material, transforms, colors);
		stats.instances += static_cast<int>(group.count);
	}

	const auto t2 = std::chrono::steady_clock::now();
	stats.draw_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
	return stats;
}

int RaylibBackend::LoadMesh(const std::string_view path) {
	const std::string key{path};
	if (const auto it = g_model_paths.find(key); it != g_model_paths.end()) {
		return it->second;
	}
	const Model model = ::LoadModel(key.c_str());
	// Route the model through the lit shader so it receives the same Phong lighting as primitives.
	if (g_lit.ready) {
		for (int i = 0; i < model.materialCount; ++i) {
			model.materials[i].shader = g_lit.shader;
		}
	}
	const int handle = static_cast<int>(g_models.size());
	g_models.push_back(model);
	g_model_paths[key] = handle;
	return handle;
}

int RaylibBackend::UploadDynamicMesh(
	const std::vector<glm::vec3>& vertices,
	const std::vector<uint32_t>& indices,
	const std::vector<glm::vec4>& colors,
	const std::vector<glm::vec2>& uvs
) {
	if (vertices.empty() || indices.empty()) {
		return -1;
	}

	// Raylib's Mesh uses 16-bit (unsigned short) indices, so vertices must be addressable by an
	// unsigned short. Reject oversized meshes rather than silently truncating indices below.
	static constexpr size_t kMaxDynamicMeshVertices = 65536;
	if (vertices.size() > kMaxDynamicMeshVertices) {
		spdlog::error(
			"[Raylib] UploadDynamicMesh: {} vertices exceed the 16-bit index limit ({}); mesh not uploaded",
			vertices.size(),
			kMaxDynamicMeshVertices
		);
		return -1;
	}

	Mesh mesh = {0};
	mesh.vertexCount = static_cast<int>(vertices.size());
	mesh.triangleCount = static_cast<int>(indices.size()) / 3;

	// Allocate and copy vertex data. Buffers are allocated with Raylib's allocator (MemAlloc →
	// RL_MALLOC) so that Raylib's UnloadMesh (RL_FREE) frees memory from the matching allocator —
	// mixing new[]/free() is undefined behaviour and crashes under MSVC debug CRT / ASan.
	mesh.vertices = static_cast<float*>(MemAlloc(static_cast<unsigned int>(vertices.size() * 3 * sizeof(float))));
	for (size_t i = 0; i < vertices.size(); ++i) {
		mesh.vertices[i * 3 + 0] = vertices[i].x;
		mesh.vertices[i * 3 + 1] = vertices[i].y;
		mesh.vertices[i * 3 + 2] = vertices[i].z;
	}

	// Allocate and copy indices
	mesh.indices =
		static_cast<unsigned short*>(MemAlloc(static_cast<unsigned int>(indices.size() * sizeof(unsigned short))));
	for (size_t i = 0; i < indices.size(); ++i) {
		mesh.indices[i] = static_cast<unsigned short>(indices[i]);
	}

	// Allocate colors sized to vertexCount (Raylib uploads vertexCount*4 bytes). Vertices beyond the
	// provided colors default to opaque white, mirroring the texcoord fallback below — this prevents
	// a heap overread when fewer colors than vertices are supplied.
	mesh.colors = static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(vertices.size() * 4)));
	const auto color_count = colors.size();
	for (size_t i = 0; i < vertices.size(); ++i) {
		if (i < color_count) {
			mesh.colors[i * 4 + 0] = static_cast<unsigned char>(colors[i].r * 255.0f);
			mesh.colors[i * 4 + 1] = static_cast<unsigned char>(colors[i].g * 255.0f);
			mesh.colors[i * 4 + 2] = static_cast<unsigned char>(colors[i].b * 255.0f);
			mesh.colors[i * 4 + 3] = static_cast<unsigned char>(colors[i].a * 255.0f);
		}
		else {
			mesh.colors[i * 4 + 0] = 255;
			mesh.colors[i * 4 + 1] = 255;
			mesh.colors[i * 4 + 2] = 255;
			mesh.colors[i * 4 + 3] = 255;
		}
	}

	// Allocate texture coordinates; use provided uvs or default to (0,0)
	mesh.texcoords = static_cast<float*>(MemAlloc(static_cast<unsigned int>(vertices.size() * 2 * sizeof(float))));
	const auto uv_count = uvs.size();
	for (size_t i = 0; i < vertices.size(); ++i) {
		mesh.texcoords[i * 2 + 0] = i < uv_count ? uvs[i].x : 0.0f;
		mesh.texcoords[i * 2 + 1] = i < uv_count ? uvs[i].y : 0.0f;
	}

	// Upload to GPU as dynamic so UpdateMeshBuffer can update UV coords for streaming
	::UploadMesh(&mesh, true);

	// Store in dynamic mesh list
	const int handle = static_cast<int>(g_dynamic_meshes.size());
	g_dynamic_meshes.push_back(mesh);
	return handle;
}

void RaylibBackend::UnloadDynamicMesh(const int handle) {
	if (handle < 0 || handle >= static_cast<int>(g_dynamic_meshes.size())) {
		return;
	}
	Mesh& mesh = g_dynamic_meshes[static_cast<std::size_t>(handle)];
	if (mesh.vertexCount == 0) {
		return; // already unloaded
	}
	::UnloadMesh(mesh);
	mesh.vertexCount = 0;
}

void RaylibBackend::DrawDynamicMesh(const int handle, const glm::mat4& transform, const DrawMaterial& material) {
	if (handle < 0 || handle >= static_cast<int>(g_dynamic_meshes.size())) {
		return;
	}

	const Mesh& mesh = g_dynamic_meshes[static_cast<size_t>(handle)];
	if (mesh.vertexCount == 0) return;

	// Transparent pass: test depth but don't write it, so the alpha-blended ghost neither hides the
	// player nor disturbs the opaque z-sort. rlDrawRenderBatchActive() flushes raylib's queued
	// geometry so the depth-mask toggle brackets exactly this mesh's draw.
	const bool restore_depth_mask = !material.depth_write;
	if (restore_depth_mask) {
		rlDrawRenderBatchActive();
		rlDisableDepthMask();
	}

	const bool drawn = DrawCustomMesh(mesh, transform, material);
	if (!drawn && g_lit.ready) {
		DrawLitMesh(mesh, transform, material);
	}

	if (restore_depth_mask) {
		rlDrawRenderBatchActive();
		rlEnableDepthMask();
	}
	if (drawn || g_lit.ready) {
		return;
	}

	rlPushMatrix();
	rlMultMatrixf(glm::value_ptr(transform));
	if (material.wireframe) {
		rlEnableWireMode();
	}

	{
		// Immediate-mode fallback when shaders haven't been initialised yet
		for (int i = 0; i < mesh.triangleCount; i++) {
			rlBegin(RL_TRIANGLES);
			for (int v = 0; v < 3; v++) {
				const int vidx = mesh.indices[i * 3 + v];
				if (mesh.colors) {
					rlColor4ub(
						mesh.colors[vidx * 4],
						mesh.colors[vidx * 4 + 1],
						mesh.colors[vidx * 4 + 2],
						mesh.colors[vidx * 4 + 3]
					);
				}
				else {
					rlColor4ub(material.tint.r, material.tint.g, material.tint.b, material.tint.a);
				}
				rlVertex3f(mesh.vertices[vidx * 3], mesh.vertices[vidx * 3 + 1], mesh.vertices[vidx * 3 + 2]);
			}
			rlEnd();
		}
	}

	if (material.wireframe) {
		rlDisableWireMode();
	}
	rlPopMatrix();
}

void RaylibBackend::UpdateDynamicMeshUVs(const int handle, const std::vector<glm::vec2>& uvs) {
	if (handle < 0 || handle >= static_cast<int>(g_dynamic_meshes.size())) return;
	const Mesh& mesh = g_dynamic_meshes[static_cast<size_t>(handle)];
	if (mesh.vertexCount == 0 || !mesh.texcoords) return;

	const int count = std::min(static_cast<int>(uvs.size()), mesh.vertexCount);
	for (int i = 0; i < count; ++i) {
		mesh.texcoords[i * 2 + 0] = uvs[i].x;
		mesh.texcoords[i * 2 + 1] = uvs[i].y;
	}
	UpdateMeshBuffer(mesh, 1, mesh.texcoords, count * 2 * static_cast<int>(sizeof(float)), 0);
}

void RaylibBackend::UpdateDynamicMeshColors(const int handle, const std::vector<glm::vec4>& colors) {
	if (handle < 0 || handle >= static_cast<int>(g_dynamic_meshes.size())) return;
	const Mesh& mesh = g_dynamic_meshes[static_cast<size_t>(handle)];
	if (mesh.vertexCount == 0 || !mesh.colors) return;

	const int count = std::min(static_cast<int>(colors.size()), mesh.vertexCount);
	for (int i = 0; i < count; ++i) {
		mesh.colors[i * 4 + 0] = static_cast<unsigned char>(glm::clamp(colors[i].r, 0.0f, 1.0f) * 255.0f);
		mesh.colors[i * 4 + 1] = static_cast<unsigned char>(glm::clamp(colors[i].g, 0.0f, 1.0f) * 255.0f);
		mesh.colors[i * 4 + 2] = static_cast<unsigned char>(glm::clamp(colors[i].b, 0.0f, 1.0f) * 255.0f);
		mesh.colors[i * 4 + 3] = static_cast<unsigned char>(glm::clamp(colors[i].a, 0.0f, 1.0f) * 255.0f);
	}
	UpdateMeshBuffer(mesh, 3, mesh.colors, count * 4, 0);
}

std::string RaylibBackend::ShaderDirectory() const {
#if defined(__EMSCRIPTEN__)
	return std::string("/assets/shaders/") + kGlslDir;
#else
	return std::string(GetApplicationDirectory()) + "assets/shaders/" + kGlslDir;
#endif
}

int RaylibBackend::LoadShader(const std::vector<ShaderStage>& stages) {
	// Raylib needs vertex + fragment paths; pick them by stage type.
	const char* vs = nullptr;
	const char* fs = nullptr;
	std::string vs_str, fs_str;
	for (const auto& [type, path] : stages) {
		if (type == "vertex") {
			vs_str = path;
			vs = vs_str.c_str();
		}
		else if (type == "fragment") {
			fs_str = path;
			fs = fs_str.c_str();
		}
	}

	CustomShaderEntry entry;
	entry.shader = ::LoadShader(vs, fs);
	if (entry.shader.id == rlGetShaderIdDefault()) {
		spdlog::warn("[RaylibBackend] LoadShader: shader failed to load (vs={}, fs={})", vs ? vs : "", fs ? fs : "");
		return -1;
	}

	const Image white_img = GenImageColor(1, 1, WHITE);
	entry.white = LoadTextureFromImage(white_img);
	UnloadImage(white_img);

	entry.material = LoadMaterialDefault();
	entry.material.shader = entry.shader;
	entry.material.maps[MATERIAL_MAP_DIFFUSE].texture = entry.white;
	entry.ready = true;

	// Optional occluder-fade uniforms: present only in fade-aware shaders (e.g. cutout.fs). A missing
	// uniform returns -1, so ApplyOcclusionFade skips shaders that don't opt in.
	entry.loc_fade_focus = GetShaderLocation(entry.shader, "fadeFocus");
	entry.loc_fade_screen_pos = GetShaderLocation(entry.shader, "fadeScreenPos");
	entry.loc_fade_radius = GetShaderLocation(entry.shader, "fadeRadius");
	entry.loc_fade_strength = GetShaderLocation(entry.shader, "fadeStrength");
	entry.loc_fade_pass = GetShaderLocation(entry.shader, "fadePass");

	const int handle = static_cast<int>(g_custom_shaders.size());
	g_custom_shaders.push_back(entry);
	return handle;
}

void RaylibBackend::SetOcclusionFade(const OcclusionFadeParams& fade) { g_fade = fade; }

void RaylibBackend::UnloadShader(const int handle) {
	if (handle < 0 || handle >= static_cast<int>(g_custom_shaders.size())) return;
	auto& entry = g_custom_shaders[static_cast<size_t>(handle)];
	if (!entry.ready) return;
	::UnloadTexture(entry.white);
	::UnloadShader(entry.shader);
	MemFree(entry.material.maps);
	entry = CustomShaderEntry{};
}

int RaylibBackend::LoadTexture(const std::string_view path) {
	// Return an existing handle if this path was already loaded — prevents duplicate GPU
	// objects and dangling handles when observers re-fire (e.g. script texture toggle).
	const std::string key{path};
	if (const auto it = g_texture_paths.find(key); it != g_texture_paths.end()) {
		return it->second;
	}
	Texture2D texture = ::LoadTexture(key.c_str());
	if (texture.id == 0) {
		return -1;
	}
	GenTextureMipmaps(&texture);
	::SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
	const int handle = static_cast<int>(g_textures.size());
	g_textures.push_back(texture);
	g_texture_paths[key] = handle;
	return handle;
}

int RaylibBackend::UploadTexture(const int width, const int height, const std::vector<uint8_t>& rgba) {
	Image img{};
	img.data = const_cast<uint8_t*>(rgba.data()); // LoadTextureFromImage copies data; safe to use const ptr here
	img.width = width;
	img.height = height;
	img.mipmaps = 1;
	img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
	const Texture2D tex = ::LoadTextureFromImage(img);
	if (tex.id == 0) {
		return -1;
	}
	const int handle = static_cast<int>(g_textures.size());
	g_textures.push_back(tex);
	return handle;
}

void RaylibBackend::UpdateTexture(const int handle, const std::vector<uint8_t>& rgba) {
	if (handle < 0 || handle >= static_cast<int>(g_textures.size())) {
		return;
	}
	const Texture2D& tex = g_textures[static_cast<size_t>(handle)];
	if (tex.id == 0) {
		return;
	}
	::UpdateTexture(tex, rgba.data());
}

int RaylibBackend::LoadSound(const std::string_view path) {
	if (!IsAudioDeviceReady()) {
		return -1;
	}
	const std::string key{path};
	if (const auto it = g_sound_paths.find(key); it != g_sound_paths.end()) {
		return it->second;
	}
	const Sound s = ::LoadSound(key.c_str());
	const int handle = static_cast<int>(g_sounds.size());
	g_sounds.push_back(s);
	g_sound_paths[key] = handle;
	return handle;
}

void RaylibBackend::PlaySound(const int handle) {
	if (handle < 0 || handle >= static_cast<int>(g_sounds.size())) {
		return;
	}
	::PlaySound(g_sounds[static_cast<std::size_t>(handle)]);
}

void RaylibBackend::UnloadTexture(const int handle) {
	if (handle < 0 || handle >= static_cast<int>(g_textures.size())) {
		return;
	}
	Texture2D& texture = g_textures[static_cast<std::size_t>(handle)];
	if (texture.id == 0) {
		return; // already unloaded — slots are never reused so this stays a safe no-op
	}
	::UnloadTexture(texture);
	texture.id = 0;
	std::erase_if(g_texture_paths, [handle](const auto& kv) { return kv.second == handle; });
}

void RaylibBackend::SetTextureFilter(const int handle, const bool linear) {
	if (handle < 0 || handle >= static_cast<int>(g_textures.size())) return;
	const Texture2D& tex = g_textures[static_cast<size_t>(handle)];
	if (tex.id == 0) return;
	::SetTextureFilter(tex, linear ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT);
}

void RaylibBackend::SetTextureWrap(const int handle, const bool clamp) {
	if (handle < 0 || handle >= static_cast<int>(g_textures.size())) return;
	const Texture2D& tex = g_textures[static_cast<size_t>(handle)];
	if (tex.id == 0) return;
	::SetTextureWrap(tex, clamp ? TEXTURE_WRAP_CLAMP : TEXTURE_WRAP_REPEAT);
}

void RaylibBackend::UnloadMesh(const int handle) {
	if (handle < 0 || handle >= static_cast<int>(g_models.size())) {
		return;
	}
	Model& model = g_models[static_cast<std::size_t>(handle)];
	if (model.meshCount == 0) {
		return; // already unloaded
	}
	::UnloadModel(model);
	model.meshCount = 0;
	std::erase_if(g_model_paths, [handle](const auto& kv) { return kv.second == handle; });
}

void RaylibBackend::UnloadSound(const int handle) {
	if (handle < 0 || handle >= static_cast<int>(g_sounds.size())) {
		return;
	}
	Sound& sound = g_sounds[static_cast<std::size_t>(handle)];
	if (sound.frameCount == 0) {
		return; // already unloaded
	}
	::UnloadSound(sound);
	sound.frameCount = 0;
	std::erase_if(g_sound_paths, [handle](const auto& kv) { return kv.second == handle; });
}

void RaylibBackend::SetSoundPitch(const int handle, const float pitch) {
	if (handle < 0 || handle >= static_cast<int>(g_sounds.size())) {
		return;
	}
	if (const Sound& sound = g_sounds[static_cast<std::size_t>(handle)]; sound.frameCount != 0) {
		::SetSoundPitch(sound, pitch);
	}
}

void RaylibBackend::SetSoundVolume(const int handle, const float volume) {
	if (handle < 0 || handle >= static_cast<int>(g_sounds.size())) {
		return;
	}
	if (const Sound& sound = g_sounds[static_cast<std::size_t>(handle)]; sound.frameCount != 0) {
		::SetSoundVolume(sound, volume);
	}
}

int RaylibBackend::LoadMusic(const std::string_view path) {
	if (!IsAudioDeviceReady()) {
		return -1;
	}
	const std::string key{path};
	if (const auto it = g_music_paths.find(key); it != g_music_paths.end()) {
		return it->second;
	}
	const Music m = ::LoadMusicStream(key.c_str());
	if (m.frameCount == 0) {
		return -1;
	}
	const int handle = static_cast<int>(g_music.size());
	g_music.push_back(m);
	g_music_paths[key] = handle;
	return handle;
}

void RaylibBackend::UnloadMusic(const int handle) {
	if (handle < 0 || handle >= static_cast<int>(g_music.size())) {
		return;
	}
	Music& music = g_music[static_cast<std::size_t>(handle)];
	if (music.frameCount == 0) {
		return; // already unloaded
	}
	::UnloadMusicStream(music);
	music.frameCount = 0;
	std::erase_if(g_music_paths, [handle](const auto& kv) { return kv.second == handle; });
}

void RaylibBackend::PlayMusic(const int handle, const bool loop) {
	if (handle < 0 || handle >= static_cast<int>(g_music.size())) {
		return;
	}
	Music& music = g_music[static_cast<std::size_t>(handle)];
	if (music.frameCount == 0) {
		return;
	}
	music.looping = loop;
	::PlayMusicStream(music);
}

void RaylibBackend::StopMusic(const int handle) {
	if (handle < 0 || handle >= static_cast<int>(g_music.size())) {
		return;
	}
	if (const Music& music = g_music[static_cast<std::size_t>(handle)]; music.frameCount != 0) {
		::StopMusicStream(music);
	}
}

void RaylibBackend::UpdateMusic(const int handle) {
	if (handle < 0 || handle >= static_cast<int>(g_music.size())) {
		return;
	}
	if (const Music& music = g_music[static_cast<std::size_t>(handle)]; music.frameCount != 0) {
		::UpdateMusicStream(music);
	}
}

void RaylibBackend::SetMusicVolume(const int handle, const float volume) {
	if (handle < 0 || handle >= static_cast<int>(g_music.size())) {
		return;
	}
	if (const Music& music = g_music[static_cast<std::size_t>(handle)]; music.frameCount != 0) {
		::SetMusicVolume(music, volume);
	}
}

// ── Procedural audio ──

void RaylibBackend::StartSynth(audio::SynthSource* source) {
	StopSynth();
	if (source == nullptr || !IsAudioDeviceReady()) {
		return;
	}
	// Publish the source before the stream can call back into it.
	g_synth_source.store(source, std::memory_order_release);
	g_synth_stream =
		LoadAudioStream(static_cast<unsigned int>(audio::kSynthSampleRate), kSynthSampleBits, kSynthChannels);
	if (!IsAudioStreamValid(g_synth_stream)) {
		g_synth_source.store(nullptr, std::memory_order_release);
		return;
	}
	SetAudioStreamCallback(g_synth_stream, SynthTrampoline);
	PlayAudioStream(g_synth_stream);
}

void RaylibBackend::StopSynth() {
	if (IsAudioStreamValid(g_synth_stream)) {
		StopAudioStream(g_synth_stream);
		// Unloading takes raylib's audio lock, so no callback is mid-flight once it returns.
		UnloadAudioStream(g_synth_stream);
		g_synth_stream = AudioStream{};
	}
	g_synth_source.store(nullptr, std::memory_order_release);
}

// ── Persistence ──

namespace {
// Reject names that could escape the save directory / storage namespace.
bool IsValidSaveName(const std::string_view name) {
	return !name.empty()
		   && name.find('/') == std::string_view::npos
		   && name.find('\\') == std::string_view::npos
		   && name.find("..") == std::string_view::npos;
}
} // namespace

#if defined(__EMSCRIPTEN__)

void RaylibBackend::WriteSave(const std::string_view name, const std::string_view data) {
	if (!IsValidSaveName(name)) {
		return;
	}
	js_write_save(std::string(name).c_str(), std::string(data).c_str());
}

std::string RaylibBackend::ReadSave(const std::string_view name) const {
	if (!IsValidSaveName(name)) {
		return {};
	}
	char* raw = js_read_save(std::string(name).c_str());
	if (raw == nullptr) {
		return {};
	}
	std::string result(raw);
	std::free(raw);
	return result;
}

bool RaylibBackend::HasSave(const std::string_view name) const {
	return IsValidSaveName(name) && js_has_save(std::string(name).c_str()) != 0;
}

void RaylibBackend::DeleteSave(const std::string_view name) {
	if (!IsValidSaveName(name)) {
		return;
	}
	js_delete_save(std::string(name).c_str());
}

std::vector<std::string> RaylibBackend::ListSaves() const {
	std::vector<std::string> names;
	if (char* raw = js_list_saves(); raw != nullptr) {
		std::string joined(raw);
		std::free(raw);
		size_t start = 0;
		while (start <= joined.size()) {
			const size_t nl = joined.find('\n', start);
			std::string entry = joined.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
			if (!entry.empty()) {
				names.push_back(std::move(entry));
			}
			if (nl == std::string::npos) {
				break;
			}
			start = nl + 1;
		}
	}
	std::ranges::sort(names);
	return names;
}

#else

namespace {
// Directory holding save-slot files (created on demand), next to the executable.
std::filesystem::path SaveDir() {
	const std::filesystem::path dir = std::filesystem::path(GetApplicationDirectory()) / "saves";
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	return dir;
}
} // namespace

void RaylibBackend::WriteSave(const std::string_view name, const std::string_view data) {
	if (!IsValidSaveName(name)) {
		return;
	}
	const std::filesystem::path path = SaveDir() / (std::string(name) + ".json");
	if (std::ofstream out(path, std::ios::binary | std::ios::trunc); out) {
		out.write(data.data(), static_cast<std::streamsize>(data.size()));
	}
}

std::string RaylibBackend::ReadSave(const std::string_view name) const {
	if (!IsValidSaveName(name)) {
		return {};
	}
	const std::ifstream in(SaveDir() / (std::string(name) + ".json"), std::ios::binary);
	if (!in) {
		return {};
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

bool RaylibBackend::HasSave(const std::string_view name) const {
	if (!IsValidSaveName(name)) {
		return false;
	}
	std::error_code ec;
	return std::filesystem::exists(SaveDir() / (std::string(name) + ".json"), ec);
}

void RaylibBackend::DeleteSave(const std::string_view name) {
	if (!IsValidSaveName(name)) {
		return;
	}
	std::error_code ec;
	std::filesystem::remove(SaveDir() / (std::string(name) + ".json"), ec);
}

std::vector<std::string> RaylibBackend::ListSaves() const {
	std::vector<std::string> names;
	for (std::error_code ec; const auto& entry : std::filesystem::directory_iterator(SaveDir(), ec)) {
		if (entry.is_regular_file() && entry.path().extension() == ".json") {
			names.push_back(entry.path().stem().string());
		}
	}
	std::ranges::sort(names);
	return names;
}

#endif

} // namespace engine::platform
