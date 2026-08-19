#include "raylib_backend.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <raylib.h>
#include <rlgl.h>
#include <spdlog/spdlog.h>

#include "engine/input/input.hpp"

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

// Loaded models, indexed by the handle returned from LoadMesh.
std::vector<Model> g_models;
// Maps canonical path → model handle.
std::unordered_map<std::string, int> g_model_paths;

// Dynamic meshes uploaded programmatically (for tilemaps, etc), indexed by handle
std::vector<Mesh> g_dynamic_meshes;

// Unlit-rendering resources: simple shader for 2D/UI elements without Phong shading.
// Populated once GL is ready and torn down on Shutdown.
struct UnlitState {
	bool ready = false;
	Shader shader{};
	Material material{};
	Texture2D white{}; // 1x1 white fallback
};

UnlitState g_unlit;

// Loaded textures, indexed by the handle returned from LoadTexture.
std::vector<Texture2D> g_textures;
// Maps canonical path → handle
std::unordered_map<std::string, int> g_texture_paths;

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

	bool shadows_enabled = false;
	glm::mat4 shadow_matrix{1.0f};
};

LitState g_lit;

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

void InitUnlitResources() {
	if (g_unlit.ready) return;

#if defined(__EMSCRIPTEN__)
	const std::string dir = std::string("/assets/shaders/") + kGlslDir;
#else
	const std::string dir = std::string(GetApplicationDirectory()) + "assets/shaders/" + kGlslDir;
#endif
	const std::string vs = dir + "/unlit.vs";
	const std::string fs = dir + "/unlit.fs";

	g_unlit.shader = LoadShader(vs.c_str(), fs.c_str());
	if (g_unlit.shader.id == rlGetShaderIdDefault()) {
		TraceLog(LOG_WARNING, "[RaylibBackend] Unlit shader failed to load; falling back to default");
		return;
	}

	const Image white_img = GenImageColor(1, 1, WHITE);
	g_unlit.white = LoadTextureFromImage(white_img);
	UnloadImage(white_img);

	g_unlit.material = LoadMaterialDefault();
	g_unlit.material.shader = g_unlit.shader;
	g_unlit.material.maps[MATERIAL_MAP_DIFFUSE].texture = g_unlit.white;

	g_unlit.ready = true;
}

void ShutdownUnlitResources() {
	if (!g_unlit.ready) return;
	UnloadTexture(g_unlit.white);
	UnloadShader(g_unlit.shader);
	// LoadMaterialDefault allocated material.maps via RL_CALLOC. The shader and texture are already
	// unloaded above, so free just the maps array (a full UnloadMaterial would double-free them).
	MemFree(g_unlit.material.maps);
	g_unlit = UnlitState{};
}

// Draws a cached mesh with the lit material, then an optional planar projected shadow.
void DrawLitMesh(
	const Mesh& mesh,
	const glm::mat4& model,
	const Rgba color,
	const int texture,
	const bool cast_shadow,
	const bool wireframe
) {
	g_lit.material.maps[MATERIAL_MAP_DIFFUSE].color = ToRay(color);
	g_lit.material.maps[MATERIAL_MAP_DIFFUSE].texture = (texture >= 0 && texture < static_cast<int>(g_textures.size()))
															? g_textures[static_cast<std::size_t>(texture)]
															: g_lit.white;

	constexpr float kOff = 0.0f;
	constexpr float kOn = 1.0f;
	SetShaderValue(g_lit.shader, g_lit.loc_shadow_pass, &kOff, SHADER_UNIFORM_FLOAT);

	if (wireframe) {
		rlEnableWireMode();
		DrawMesh(mesh, g_lit.material, ToRayMatrix(model));
		rlDisableWireMode();
		return;
	}

	DrawMesh(mesh, g_lit.material, ToRayMatrix(model));

	if (cast_shadow && g_lit.shadows_enabled) {
		const glm::mat4 shadow_model = g_lit.shadow_matrix * model;
		SetShaderValue(g_lit.shader, g_lit.loc_shadow_pass, &kOn, SHADER_UNIFORM_FLOAT);
		rlDisableDepthMask(); // shadows blend without fighting the receiver's depth
		DrawMesh(mesh, g_lit.material, ToRayMatrix(shadow_model));
		rlEnableDepthMask();
		SetShaderValue(g_lit.shader, g_lit.loc_shadow_pass, &kOff, SHADER_UNIFORM_FLOAT);
	}
}
} // namespace

void RaylibBackend::Init(const int width, const int height, const std::string_view title) {
	InitWindow(width, height, std::string(title).c_str());
	SetExitKey(KEY_NULL); // we use Esc for pause, not window-close
	SetTargetFPS(60);
	InitAudioDevice();
	InitLitResources();
	InitUnlitResources();
}

bool RaylibBackend::ShouldClose() const { return WindowShouldClose(); }

void RaylibBackend::Shutdown() {
	ShutdownLitResources();
	ShutdownUnlitResources();
	for (const Texture2D& t : g_textures) {
		if (t.id != 0) {
			::UnloadTexture(t);
		}
	}
	g_textures.clear();
	g_texture_paths.clear();
	for (const Sound& s : g_sounds) {
		if (s.frameCount != 0) {
			::UnloadSound(s);
		}
	}
	g_sounds.clear();
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
	CloseAudioDevice();
	CloseWindow();
}

int RaylibBackend::Width() const { return GetScreenWidth(); }

int RaylibBackend::Height() const { return GetScreenHeight(); }

float RaylibBackend::DeltaTime() const { return GetFrameTime(); }

double RaylibBackend::Time() const { return GetTime(); }

std::string RaylibBackend::AssetDirectory() const {
#if defined(__EMSCRIPTEN__)
	return "/assets";
#else
	return std::string(GetApplicationDirectory()) + "assets";
#endif
}

void RaylibBackend::BeginFrame(const Rgba clear) {
	BeginDrawing();
	ClearBackground(ToRay(clear));
}

void RaylibBackend::EndFrame() { EndDrawing(); }

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
	return ::IsKeyDown(KeyCodeToRay(key_code));
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
	// GetCharPressed returns a single codepoint this frame, or 0 if none.
	// This provides per-frame character input for text fields.
	return GetCharPressed();
}

std::string RaylibBackend::TextInput() const {
	// Raylib doesn't expose batched text input directly, so we build it from repeated GetCharPressed calls.
	// For most cases, CharInput() is sufficient. This returns all printable characters typed this frame.
	std::string result;
	int ch;
	while ((ch = GetCharPressed()) != 0) {
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
	::DrawText(std::string(text).c_str(), static_cast<int>(x), static_cast<int>(y), static_cast<int>(size), ToRay(c));
}

float RaylibBackend::MeasureText(const std::string_view text, const float size) const {
	return static_cast<float>(::MeasureText(std::string(text).c_str(), static_cast<int>(size)));
}

void RaylibBackend::DrawLine(const glm::vec2 a, const glm::vec2 b, const float thickness, const Rgba c) {
	DrawLineEx(ToRay(a), ToRay(b), thickness, ToRay(c));
}

void RaylibBackend::DrawCircle(const glm::vec2 center, const float radius, const Rgba c) {
	DrawCircleV(ToRay(center), radius, ToRay(c));
}

void RaylibBackend::BeginScissor(const Rect r) {
	BeginScissorMode(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.w), static_cast<int>(r.h));
}

void RaylibBackend::EndScissor() { EndScissorMode(); }

void RaylibBackend::BeginMode3D(const Camera3DParams& camera) {
	const Camera3D cam{
		.position = ToRay(camera.position),
		.target = ToRay(camera.target),
		.up = ToRay(camera.up),
		.fovy = camera.fov_y,
		.projection = CAMERA_PERSPECTIVE,
	};
	::BeginMode3D(cam);

	// Feed the camera position to the lit shader for specular highlights.
	if (g_lit.ready) {
		const float view_pos[3] = {camera.position.x, camera.position.y, camera.position.z};
		SetShaderValue(g_lit.shader, g_lit.loc_view_pos, view_pos, SHADER_UNIFORM_VEC3);
	}
}

void RaylibBackend::EndMode3D() { ::EndMode3D(); }

void RaylibBackend::SetLighting(const LightParams& lighting) {
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

void RaylibBackend::DrawCube(
	const glm::mat4& transform,
	const glm::vec3 size,
	const Rgba c,
	const int texture,
	const bool cast_shadow,
	const bool wireframe
) {
	if (g_lit.ready) {
		DrawLitMesh(g_lit.cube, transform * glm::scale(glm::mat4(1.0f), size), c, texture, cast_shadow, wireframe);
		return;
	}
	rlPushMatrix();
	rlMultMatrixf(glm::value_ptr(transform));
	if (wireframe) {
		DrawCubeWiresV(Vector3{.x = 0.0F, .y = 0.0F, .z = 0.0F}, ToRay(size), ToRay(c));
	}
	else {
		DrawCubeV(Vector3{.x = 0.0F, .y = 0.0F, .z = 0.0F}, ToRay(size), ToRay(c));
	}
	rlPopMatrix();
}

void RaylibBackend::DrawSphere(
	const glm::mat4& transform,
	const float radius,
	const Rgba c,
	const int texture,
	const bool cast_shadow,
	const bool wireframe
) {
	if (g_lit.ready) {
		DrawLitMesh(
			g_lit.sphere,
			transform * glm::scale(glm::mat4(1.0f), glm::vec3(radius)),
			c,
			texture,
			cast_shadow,
			wireframe
		);
		return;
	}
	rlPushMatrix();
	rlMultMatrixf(glm::value_ptr(transform));
	if (wireframe) {
		DrawSphereWires(Vector3{.x = 0.0F, .y = 0.0F, .z = 0.0F}, radius, 16, 16, ToRay(c));
	}
	else {
		::DrawSphere(Vector3{.x = 0.0F, .y = 0.0F, .z = 0.0F}, radius, ToRay(c));
	}
	rlPopMatrix();
}

void RaylibBackend::DrawQuad(
	const glm::mat4& transform,
	const glm::vec2 size,
	const Rgba c,
	const int texture,
	const bool cast_shadow,
	const bool wireframe
) {
	if (g_lit.ready && !wireframe) {
		DrawLitMesh(
			g_lit.plane,
			transform * glm::scale(glm::mat4(1.0f), glm::vec3(size.x, 1.0f, size.y)),
			c,
			texture,
			cast_shadow,
			false
		);
		return;
	}
	rlPushMatrix();
	rlMultMatrixf(glm::value_ptr(transform));
	if (wireframe) {
		const float hx = size.x * 0.5F;
		const float hz = size.y * 0.5F;
		const Vector3 a{.x = -hx, .y = 0.0F, .z = -hz};
		const Vector3 b{.x = hx, .y = 0.0F, .z = -hz};
		const Vector3 d{.x = hx, .y = 0.0F, .z = hz};
		const Vector3 e{.x = -hx, .y = 0.0F, .z = hz};
		DrawLine3D(a, b, ToRay(c));
		DrawLine3D(b, d, ToRay(c));
		DrawLine3D(d, e, ToRay(c));
		DrawLine3D(e, a, ToRay(c));
	}
	else {
		DrawPlane(Vector3{.x = 0.0F, .y = 0.0F, .z = 0.0F}, Vector2{.x = size.x, .y = size.y}, ToRay(c));
	}
	rlPopMatrix();
}

void RaylibBackend::DrawCapsule(
	const glm::mat4& transform,
	const float radius,
	const float height,
	const Rgba c,
	const int texture,
	const bool cast_shadow,
	const bool wireframe
) {
	if (g_lit.ready && !wireframe) {
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
		DrawLitMesh(g_lit.cylinder, body, c, texture, cast_shadow, false);
		DrawLitMesh(g_lit.sphere, top, c, texture, cast_shadow, false);
		DrawLitMesh(g_lit.sphere, bottom, c, texture, cast_shadow, false);
		return;
	}
	const float half = height * 0.5F;
	const Vector3 start{.x = 0.0F, .y = -half, .z = 0.0F};
	const Vector3 end{.x = 0.0F, .y = half, .z = 0.0F};
	rlPushMatrix();
	rlMultMatrixf(glm::value_ptr(transform));
	if (wireframe) {
		DrawCapsuleWires(start, end, radius, 16, 8, ToRay(c));
	}
	else {
		::DrawCapsule(start, end, radius, 16, 8, ToRay(c));
	}
	rlPopMatrix();
}

void RaylibBackend::DrawMesh(
	const int handle,
	const glm::mat4& transform,
	const Rgba tint,
	const int texture,
	const bool cast_shadow,
	const bool wireframe
) {
	if (handle < 0 || handle >= static_cast<int>(g_models.size())) {
		return;
	}
	Model& model = g_models[static_cast<std::size_t>(handle)];

	// Optional per-draw diffuse texture override on the model's primary material.
	if (texture >= 0 && texture < static_cast<int>(g_textures.size()) && model.materialCount > 0) {
		model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = g_textures[static_cast<std::size_t>(texture)];
	}

	const bool lit = g_lit.ready && !wireframe;
	if (lit) {
		constexpr float off = 0.0f;
		SetShaderValue(g_lit.shader, g_lit.loc_shadow_pass, &off, SHADER_UNIFORM_FLOAT);
	}

	model.transform = ToRayMatrix(transform);
	if (wireframe) {
		rlEnableWireMode();
	}
	DrawModel(model, Vector3{.x = 0.0F, .y = 0.0F, .z = 0.0F}, 1.0F, ToRay(tint));
	if (wireframe) {
		rlDisableWireMode();
	}

	if (lit && cast_shadow && g_lit.shadows_enabled) {
		constexpr float on = 1.0f;
		constexpr float off = 0.0f;
		SetShaderValue(g_lit.shader, g_lit.loc_shadow_pass, &on, SHADER_UNIFORM_FLOAT);
		rlDisableDepthMask();
		model.transform = ToRayMatrix(g_lit.shadow_matrix * transform);
		DrawModel(model, Vector3{.x = 0.0F, .y = 0.0F, .z = 0.0F}, 1.0F, ToRay(tint));
		rlEnableDepthMask();
		SetShaderValue(g_lit.shader, g_lit.loc_shadow_pass, &off, SHADER_UNIFORM_FLOAT);
	}

	model.transform = ToRayMatrix(glm::mat4(1.0f));
}

int RaylibBackend::LoadMesh(const std::string_view path) {
	const std::string key{path};
	if (const auto it = g_model_paths.find(key); it != g_model_paths.end()) {
		return it->second;
	}
	Model model = ::LoadModel(key.c_str());
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

	// Upload to GPU
	::UploadMesh(&mesh, false);

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

void RaylibBackend::DrawDynamicMesh(
	const int handle,
	const glm::mat4& transform,
	const int texture,
	const bool wireframe
) {
	if (handle < 0 || handle >= static_cast<int>(g_dynamic_meshes.size())) {
		return;
	}

	const Mesh& mesh = g_dynamic_meshes[static_cast<size_t>(handle)];

	// Use unlit shader if available, otherwise fall back to default
	if (g_unlit.ready) {
		g_unlit.material.maps[MATERIAL_MAP_DIFFUSE].texture =
			(texture >= 0 && texture < static_cast<int>(g_textures.size()))
				? g_textures[static_cast<std::size_t>(texture)]
				: g_unlit.white;
	}

	if (wireframe) {
		rlEnableWireMode();
	}

	// Apply transformation matrix
	// rlPushMatrix();
	// rlMultMatrixf(glm::value_ptr(transform));

	// Draw mesh with material (vertex colors are automatically used by Raylib)
	if (g_unlit.ready) {
		::DrawMesh(mesh, g_unlit.material, ToRayMatrix(transform));
	}
	else {
		// Fallback: draw without shader
		rlSetMatrixModelview(rlGetMatrixTransform());
		for (int i = 0; i < mesh.triangleCount; i++) {
			rlBegin(RL_TRIANGLES);
			for (int v = 0; v < 3; v++) {
				int vidx = mesh.indices[i * 3 + v];
				if (mesh.colors) {
					rlColor4ub(
						mesh.colors[vidx * 4],
						mesh.colors[vidx * 4 + 1],
						mesh.colors[vidx * 4 + 2],
						mesh.colors[vidx * 4 + 3]
					);
				}
				rlVertex3f(mesh.vertices[vidx * 3], mesh.vertices[vidx * 3 + 1], mesh.vertices[vidx * 3 + 2]);
			}
			rlEnd();
		}
	}

	// rlPopMatrix();

	if (wireframe) {
		rlDisableWireMode();
	}
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
	SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
	const int handle = static_cast<int>(g_textures.size());
	g_textures.push_back(texture);
	g_texture_paths[key] = handle;
	return handle;
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
