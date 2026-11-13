#pragma once

#include <string>
#include <utility>
#include <vector>

#include "engine/platform/platform.hpp"

namespace engine::platform {

// The Platform with no window, GPU, input or audio: a headless server world, a tool, the tests. What "no window"
// means lives here. HasWindow() is false, every draw, audio and window call is a no-op, every query returns a neutral
// value (no keys down, mouse at the origin), and every load returns -1. Asset paths resolve against the directory
// given at construction, as the desktop Platform resolves them against its own, so a headless world can load
// colliders, ROMs and level files by asset path. An EngineContext created without a Platform gets one of these.
class HeadlessPlatform : public Platform {
public:
	// `assetDirectory` is what AssetDirectory() reports; relative paths are against the working directory.
	explicit HeadlessPlatform(std::string assetDirectory = ".") : assetDirectory_(std::move(assetDirectory)) {}

	// Init records the canvas size Width() and Height() report (1280 x 720 until called); there is no window.
	void Init(const int width, const int height, std::string_view) override {
		width_ = width;
		height_ = height;
	}
	[[nodiscard]] bool ShouldClose() const override { return false; }
	void Shutdown() override {}
	[[nodiscard]] bool HasWindow() const override { return false; }
	[[nodiscard]] int Width() const override { return width_; }
	[[nodiscard]] int Height() const override { return height_; }
	void SetFullscreen(bool) override {}
	[[nodiscard]] bool IsFullscreen() const override { return false; }
	// No frame clock: a fixed 1/60 s. A headless loop passes its own measured dt to fixed_step::RunFrame().
	[[nodiscard]] float DeltaTime() const override { return 1.0F / 60.0F; }
	[[nodiscard]] double Time() const override { return 0.0; }
	[[nodiscard]] std::string AssetDirectory() const override { return assetDirectory_; }
	[[nodiscard]] int Fps() const override { return 0; }
	// No window to pace: the headless loop decides how often to run a Frame.
	void SetFrameRate(const FrameRate&) override {}

	void BeginFrame(Rgba) override {}
	void EndFrame() override {}

	[[nodiscard]] glm::vec2 MousePosition() const override { return {}; }
	[[nodiscard]] glm::vec2 MouseDelta() const override { return {}; }
	[[nodiscard]] glm::vec2 MouseWheel() const override { return {}; }
	// Remembers the request, so a game that locks the cursor reads back what it asked for.
	void SetCursorLocked(const bool locked) override { cursorLocked_ = locked; }
	[[nodiscard]] bool IsCursorLocked() const override { return cursorLocked_; }
	[[nodiscard]] bool MousePressed(MouseButton) const override { return false; }
	[[nodiscard]] bool MouseReleased(MouseButton) const override { return false; }
	[[nodiscard]] bool MouseDown(MouseButton) const override { return false; }
	[[nodiscard]] bool IsKeyDown(int) const override { return false; }
	[[nodiscard]] bool IsKeyPressedRepeat(int) const override { return false; }
	[[nodiscard]] bool IsMouseButtonDown(int) const override { return false; }
	[[nodiscard]] int CharInput() const override { return 0; }
	[[nodiscard]] std::string TextInput() const override { return {}; }

	DebugUi& GetDebugUi() override { return debugUi_; }
	void DrawDebugUi() override {}

	void DrawRect(Rect, Rgba) override {}
	void DrawRectLines(Rect, float, Rgba) override {}
	void DrawRoundedRect(Rect, float, Rgba) override {}
	void DrawRoundedRectLines(Rect, float, float, Rgba) override {}
	void DrawText(std::string_view, float, float, float, Rgba) override {}
	[[nodiscard]] float MeasureText(std::string_view, float) const override { return 0.0F; }
	bool SetTextFont(std::string_view) override { return false; }
	void DrawLine(glm::vec2, glm::vec2, float, Rgba) override {}
	void DrawCircle(glm::vec2, float, Rgba) override {}
	void DrawTexturedRect(int, Rect, Rgba) override {}
	void BeginScissor(Rect) override {}
	void EndScissor() override {}

	void BeginMode3D(const Camera3DParams&) override {}
	void EndMode3D() override {}
	void SetLighting(const LightParams&) override {}
	void SetOcclusionFade(const OcclusionFadeParams&) override {}
	void DrawCube(const glm::mat4&, glm::vec3, const DrawMaterial&) override {}
	void DrawSphere(const glm::mat4&, float, const DrawMaterial&) override {}
	void DrawQuad(const glm::mat4&, glm::vec2, const DrawMaterial&) override {}
	void DrawCapsule(const glm::mat4&, float, float, const DrawMaterial&) override {}
	void DrawMesh(int, const glm::mat4&, const DrawMaterial&) override {}

	[[nodiscard]] int PrimitiveMesh(PrimitiveShape) const override { return -1; }
	BatchStats SubmitBatch(std::span<const RenderCommand>) override { return {}; }

	int UploadDynamicMesh(
		const std::vector<glm::vec3>&,
		const std::vector<uint32_t>&,
		const std::vector<glm::vec4>&,
		const std::vector<glm::vec2>&
	) override {
		return -1;
	}
	void UnloadDynamicMesh(int) override {}
	void DrawDynamicMesh(int, const glm::mat4&, const DrawMaterial&) override {}
	void UpdateDynamicMeshUVs(int, const std::vector<glm::vec2>&) override {}
	void UpdateDynamicMeshColors(int, const std::vector<glm::vec4>&) override {}

	[[nodiscard]] std::string ShaderDirectory() const override { return assetDirectory_ + "/shaders"; }
	int LoadShader(const std::vector<ShaderStage>&) override { return -1; }
	void UnloadShader(int) override {}

	int LoadMesh(std::string_view) override { return -1; }
	int LoadTexture(std::string_view) override { return -1; }
	int UploadTexture(int, int, const std::vector<uint8_t>&) override { return -1; }
	void UpdateTexture(int, const std::vector<uint8_t>&) override {}
	void UnloadMesh(int) override {}
	int LoadSkeletalAnimationSet(std::string_view) override { return -1; }
	void UnloadSkeletalAnimationSet(int) override {}
	[[nodiscard]] int SkeletalClipCount(int) const override { return 0; }
	[[nodiscard]] std::string SkeletalClipName(int, int) const override { return {}; }
	[[nodiscard]] float SkeletalClipDuration(int, int) const override { return 0.0F; }
	void DrawSkinnedMesh(int, int, const SkeletalPose&, const glm::mat4&, const DrawMaterial&) override {}
	void UnloadTexture(int) override {}
	void SetTextureFilter(int, bool) override {}
	void SetTextureWrap(int, bool) override {}

	int LoadSound(std::string_view) override { return -1; }
	void UnloadSound(int) override {}
	void PlaySound(int) override {}
	void SetSoundPitch(int, float) override {}
	void SetSoundVolume(int, float) override {}
	int LoadMusic(std::string_view) override { return -1; }
	void UnloadMusic(int) override {}
	void PlayMusic(int, bool) override {}
	void StopMusic(int) override {}
	void UpdateMusic(int) override {}
	void SetMusicVolume(int, float) override {}

	void StartSynth(audio::SynthSource*) override {}
	void StopSynth() override {}

	// No save storage: writes are dropped and every slot reads as missing.
	void WriteSave(std::string_view, std::string_view) override {}
	[[nodiscard]] std::string ReadSave(std::string_view) const override { return {}; }
	[[nodiscard]] bool HasSave(std::string_view) const override { return false; }
	void DeleteSave(std::string_view) override {}
	[[nodiscard]] std::vector<std::string> ListSaves() const override { return {}; }

private:
	std::string assetDirectory_;
	int width_ = 1280;
	int height_ = 720;
	bool cursorLocked_ = false;
	DebugUi debugUi_;
};

} // namespace engine::platform
