#ifdef ENGINE_DEBUG_UI

#include "rl_imgui.hpp"

#include <imgui.h>
#include <raylib.h>
#include <rlgl.h>

namespace engine::platform::rlimgui {

namespace {

Texture2D g_font_texture{};

struct KeyMapping {
	int raylibKey;
	ImGuiKey imguiKey;
};

// clang-format off
constexpr KeyMapping kKeyMap[] = {
	{KEY_TAB, ImGuiKey_Tab}, {KEY_LEFT, ImGuiKey_LeftArrow}, {KEY_RIGHT, ImGuiKey_RightArrow},
	{KEY_UP, ImGuiKey_UpArrow}, {KEY_DOWN, ImGuiKey_DownArrow}, {KEY_PAGE_UP, ImGuiKey_PageUp},
	{KEY_PAGE_DOWN, ImGuiKey_PageDown}, {KEY_HOME, ImGuiKey_Home}, {KEY_END, ImGuiKey_End},
	{KEY_INSERT, ImGuiKey_Insert}, {KEY_DELETE, ImGuiKey_Delete}, {KEY_BACKSPACE, ImGuiKey_Backspace},
	{KEY_SPACE, ImGuiKey_Space}, {KEY_ENTER, ImGuiKey_Enter}, {KEY_ESCAPE, ImGuiKey_Escape},
	{KEY_APOSTROPHE, ImGuiKey_Apostrophe}, {KEY_COMMA, ImGuiKey_Comma}, {KEY_MINUS, ImGuiKey_Minus},
	{KEY_PERIOD, ImGuiKey_Period}, {KEY_SLASH, ImGuiKey_Slash}, {KEY_SEMICOLON, ImGuiKey_Semicolon},
	{KEY_EQUAL, ImGuiKey_Equal}, {KEY_LEFT_BRACKET, ImGuiKey_LeftBracket},
	{KEY_BACKSLASH, ImGuiKey_Backslash}, {KEY_RIGHT_BRACKET, ImGuiKey_RightBracket},
	{KEY_GRAVE, ImGuiKey_GraveAccent}, {KEY_CAPS_LOCK, ImGuiKey_CapsLock},
	{KEY_SCROLL_LOCK, ImGuiKey_ScrollLock}, {KEY_NUM_LOCK, ImGuiKey_NumLock},
	{KEY_KP_0, ImGuiKey_Keypad0}, {KEY_KP_1, ImGuiKey_Keypad1}, {KEY_KP_2, ImGuiKey_Keypad2},
	{KEY_KP_3, ImGuiKey_Keypad3}, {KEY_KP_4, ImGuiKey_Keypad4}, {KEY_KP_5, ImGuiKey_Keypad5},
	{KEY_KP_6, ImGuiKey_Keypad6}, {KEY_KP_7, ImGuiKey_Keypad7}, {KEY_KP_8, ImGuiKey_Keypad8},
	{KEY_KP_9, ImGuiKey_Keypad9}, {KEY_KP_DECIMAL, ImGuiKey_KeypadDecimal},
	{KEY_KP_DIVIDE, ImGuiKey_KeypadDivide}, {KEY_KP_MULTIPLY, ImGuiKey_KeypadMultiply},
	{KEY_KP_SUBTRACT, ImGuiKey_KeypadSubtract}, {KEY_KP_ADD, ImGuiKey_KeypadAdd},
	{KEY_KP_ENTER, ImGuiKey_KeypadEnter},
	{KEY_A, ImGuiKey_A}, {KEY_B, ImGuiKey_B}, {KEY_C, ImGuiKey_C}, {KEY_D, ImGuiKey_D},
	{KEY_E, ImGuiKey_E}, {KEY_F, ImGuiKey_F}, {KEY_G, ImGuiKey_G}, {KEY_H, ImGuiKey_H},
	{KEY_I, ImGuiKey_I}, {KEY_J, ImGuiKey_J}, {KEY_K, ImGuiKey_K}, {KEY_L, ImGuiKey_L},
	{KEY_M, ImGuiKey_M}, {KEY_N, ImGuiKey_N}, {KEY_O, ImGuiKey_O}, {KEY_P, ImGuiKey_P},
	{KEY_Q, ImGuiKey_Q}, {KEY_R, ImGuiKey_R}, {KEY_S, ImGuiKey_S}, {KEY_T, ImGuiKey_T},
	{KEY_U, ImGuiKey_U}, {KEY_V, ImGuiKey_V}, {KEY_W, ImGuiKey_W}, {KEY_X, ImGuiKey_X},
	{KEY_Y, ImGuiKey_Y}, {KEY_Z, ImGuiKey_Z},
	{KEY_ZERO, ImGuiKey_0}, {KEY_ONE, ImGuiKey_1}, {KEY_TWO, ImGuiKey_2}, {KEY_THREE, ImGuiKey_3},
	{KEY_FOUR, ImGuiKey_4}, {KEY_FIVE, ImGuiKey_5}, {KEY_SIX, ImGuiKey_6}, {KEY_SEVEN, ImGuiKey_7},
	{KEY_EIGHT, ImGuiKey_8}, {KEY_NINE, ImGuiKey_9},
	{KEY_F1, ImGuiKey_F1}, {KEY_F2, ImGuiKey_F2}, {KEY_F3, ImGuiKey_F3}, {KEY_F4, ImGuiKey_F4},
	{KEY_F5, ImGuiKey_F5}, {KEY_F6, ImGuiKey_F6}, {KEY_F7, ImGuiKey_F7}, {KEY_F8, ImGuiKey_F8},
	{KEY_F9, ImGuiKey_F9}, {KEY_F10, ImGuiKey_F10}, {KEY_F11, ImGuiKey_F11}, {KEY_F12, ImGuiKey_F12},
};
// clang-format on

// Puts rlgl in the state ImGui geometry expects — screen-space ortho projection, no depth test,
// straight alpha blending — independent of what the scene left behind. Matrices are pushed so
// RenderDrawData can restore them.
void SetupRenderState(const ImDrawData& draw_data) {
	rlDrawRenderBatchActive();
	rlDisableDepthTest();
	rlDisableBackfaceCulling();
	rlSetBlendMode(RL_BLEND_ALPHA);

	rlMatrixMode(RL_PROJECTION);
	rlPushMatrix();
	rlLoadIdentity();
	const ImVec2 min = draw_data.DisplayPos;
	const ImVec2 max{min.x + draw_data.DisplaySize.x, min.y + draw_data.DisplaySize.y};
	rlOrtho(min.x, max.x, max.y, min.y, -1.0, 1.0);
	rlMatrixMode(RL_MODELVIEW);
	rlPushMatrix();
	rlLoadIdentity();
}

// Restores the matrices SetupRenderState pushed. rlgl has one stack shared by both matrix modes, so
// pop in reverse push order: modelview, then projection.
void PopRenderState() {
	rlMatrixMode(RL_MODELVIEW);
	rlPopMatrix();
	rlMatrixMode(RL_PROJECTION);
	rlPopMatrix();
	rlMatrixMode(RL_MODELVIEW);
}

} // namespace

void Setup() {
	ImGuiIO& io = ImGui::GetIO();
	io.BackendPlatformName = "rlImGui_minimal";

	unsigned char* pixels = nullptr;
	int width = 0;
	int height = 0;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	// Image only borrows the atlas pixels for the upload; ImGui keeps ownership.
	const Image image{
		.data = pixels,
		.width = width,
		.height = height,
		.mipmaps = 1,
		.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
	};
	g_font_texture = LoadTextureFromImage(image);
	SetTextureFilter(g_font_texture, TEXTURE_FILTER_BILINEAR);
	io.Fonts->SetTexID(static_cast<ImTextureID>(g_font_texture.id));
}

void Shutdown() {
	if (g_font_texture.id != 0) {
		UnloadTexture(g_font_texture);
	}
	g_font_texture = {};
}

void QueueInput(const std::span<const int> chars) {
	ImGuiIO& io = ImGui::GetIO();

	const auto [mx, my] = GetMousePosition();
	io.AddMousePosEvent(mx, my);
	io.AddMouseButtonEvent(0, IsMouseButtonDown(MOUSE_BUTTON_LEFT));
	io.AddMouseButtonEvent(1, IsMouseButtonDown(MOUSE_BUTTON_RIGHT));
	io.AddMouseButtonEvent(2, IsMouseButtonDown(MOUSE_BUTTON_MIDDLE));
	if (const auto [wx, wy] = GetMouseWheelMoveV(); wx != 0.0f || wy != 0.0f) {
		io.AddMouseWheelEvent(wx, wy);
	}

	io.AddKeyEvent(ImGuiMod_Ctrl, IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL));
	io.AddKeyEvent(ImGuiMod_Shift, IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
	io.AddKeyEvent(ImGuiMod_Alt, IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT));
	io.AddKeyEvent(ImGuiMod_Super, IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER));
	for (const auto& [raylibKey, imguiKey] : kKeyMap) {
		io.AddKeyEvent(imguiKey, IsKeyDown(raylibKey));
	}

	for (const int ch : chars) {
		io.AddInputCharacter(static_cast<unsigned int>(ch));
	}
}

void RenderDrawData() {
	const ImDrawData* draw_data = ImGui::GetDrawData();
	if (!draw_data) {
		return;
	}

	SetupRenderState(*draw_data);
	rlEnableScissorTest();

	const float display_height = draw_data->DisplaySize.y;
	for (const ImDrawList* cmd_list : draw_data->CmdLists) {
		for (const ImDrawCmd& cmd : cmd_list->CmdBuffer) {
			// Clip rect → GL scissor (origin bottom-left).
			const float clip_x = cmd.ClipRect.x - draw_data->DisplayPos.x;
			const float clip_y = cmd.ClipRect.y - draw_data->DisplayPos.y;
			const float clip_w = cmd.ClipRect.z - cmd.ClipRect.x;
			const float clip_h = cmd.ClipRect.w - cmd.ClipRect.y;
			if (clip_w <= 0.0f || clip_h <= 0.0f) {
				continue; // fully clipped; GL rejects a negative scissor and would keep the last one
			}
			rlScissor(
				static_cast<int>(clip_x),
				static_cast<int>(display_height - (clip_y + clip_h)),
				static_cast<int>(clip_w),
				static_cast<int>(clip_h)
			);

			if (cmd.UserCallback == ImDrawCallback_ResetRenderState) {
				// A sentinel, not a function: re-apply our state (pop first to keep the stack balanced).
				rlDrawRenderBatchActive();
				PopRenderState();
				SetupRenderState(*draw_data);
				continue;
			}
			if (cmd.UserCallback) {
				cmd.UserCallback(cmd_list, &cmd);
				continue;
			}

			// Texture after rlBegin: a mode change in rlBegin opens a new draw call bound to the
			// default (white) texture, which would discard an earlier rlSetTexture.
			rlBegin(RL_TRIANGLES);
			rlSetTexture(static_cast<unsigned int>(cmd.GetTexID()));
			for (unsigned int i = 0; i < cmd.ElemCount; ++i) {
				const ImDrawIdx idx = cmd_list->IdxBuffer[static_cast<int>(cmd.IdxOffset + i)];
				const ImDrawVert& v = cmd_list->VtxBuffer[static_cast<int>(cmd.VtxOffset + idx)];
				rlColor4ub(
					static_cast<unsigned char>(v.col >> IM_COL32_R_SHIFT),
					static_cast<unsigned char>(v.col >> IM_COL32_G_SHIFT),
					static_cast<unsigned char>(v.col >> IM_COL32_B_SHIFT),
					static_cast<unsigned char>(v.col >> IM_COL32_A_SHIFT)
				);
				rlTexCoord2f(v.uv.x, v.uv.y);
				rlVertex2f(v.pos.x, v.pos.y);
			}
			rlEnd();
			rlDrawRenderBatchActive();
		}
	}

	rlSetTexture(0);
	rlDisableScissorTest();
	rlEnableBackfaceCulling();
	PopRenderState();
}

} // namespace engine::platform::rlimgui

#endif // ENGINE_DEBUG_UI
