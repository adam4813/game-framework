#pragma once

#include <string>
#include <utility>

#include "engine/platform/platform.hpp"

// Headless Platform: no window, GPU or audio. Every query returns a neutral value and every draw is a
// no-op. `assetDirectory` is what AssetDirectory() reports, so asset-relative loads resolve under it.
class NullPlatform final : public engine::platform::Platform {
public:
	explicit NullPlatform(std::string assetDirectory = ".") : assetDirectory_(std::move(assetDirectory)) {}

	void Init(int, int, std::string_view) override {}
	[[nodiscard]] bool ShouldClose() const override { return false; }
	void Shutdown() override {}
	[[nodiscard]] int Width() const override { return 1280; }
	[[nodiscard]] int Height() const override { return 720; }
	[[nodiscard]] float DeltaTime() const override { return 0.016F; }
	[[nodiscard]] double Time() const override { return 0.0; }
	[[nodiscard]] std::string AssetDirectory() const override { return assetDirectory_; }
	[[nodiscard]] int Fps() const override { return 0; }

	void BeginFrame(Rgba) override {}
	void EndFrame() override {}

	[[nodiscard]] glm::vec2 MousePosition() const override { return {}; }
	[[nodiscard]] glm::vec2 MouseDelta() const override { return {}; }
	[[nodiscard]] glm::vec2 MouseWheel() const override { return {}; }
	[[nodiscard]] bool MousePressed(engine::platform::MouseButton) const override { return false; }
	[[nodiscard]] bool MouseReleased(engine::platform::MouseButton) const override { return false; }
	[[nodiscard]] bool MouseDown(engine::platform::MouseButton) const override { return false; }
	[[nodiscard]] bool IsKeyDown(int) const override { return false; }
	[[nodiscard]] bool IsKeyPressedRepeat(int) const override { return false; }
	[[nodiscard]] bool IsMouseButtonDown(int) const override { return false; }
	[[nodiscard]] int CharInput() const override { return 0; }
	[[nodiscard]] std::string TextInput() const override { return {}; }

	engine::platform::DebugUi& GetDebugUi() override { return debugUi_; }
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

	void BeginMode3D(const engine::platform::Camera3DParams&) override {}
	void EndMode3D() override {}
	void SetLighting(const engine::platform::LightParams&) override {}
	void SetOcclusionFade(const engine::platform::OcclusionFadeParams&) override {}
	void DrawCube(const glm::mat4&, glm::vec3, const engine::platform::DrawMaterial&) override {}
	void DrawSphere(const glm::mat4&, float, const engine::platform::DrawMaterial&) override {}
	void DrawQuad(const glm::mat4&, glm::vec2, const engine::platform::DrawMaterial&) override {}
	void DrawCapsule(const glm::mat4&, float, float, const engine::platform::DrawMaterial&) override {}
	void DrawMesh(int, const glm::mat4&, const engine::platform::DrawMaterial&) override {}

	[[nodiscard]] int PrimitiveMesh(engine::platform::PrimitiveShape) const override { return -1; }
	engine::platform::BatchStats SubmitBatch(std::span<const engine::platform::RenderCommand>) override {
		return {};
	}

	int UploadDynamicMesh(
		const std::vector<glm::vec3>&,
		const std::vector<uint32_t>&,
		const std::vector<glm::vec4>&,
		const std::vector<glm::vec2>&
	) override {
		return -1;
	}
	void UnloadDynamicMesh(int) override {}
	void DrawDynamicMesh(int, const glm::mat4&, const engine::platform::DrawMaterial&) override {}
	void UpdateDynamicMeshUVs(int, const std::vector<glm::vec2>&) override {}
	void UpdateDynamicMeshColors(int, const std::vector<glm::vec4>&) override {}

	[[nodiscard]] std::string ShaderDirectory() const override { return "."; }
	int LoadShader(const std::vector<engine::platform::ShaderStage>&) override { return -1; }
	void UnloadShader(int) override {}

	int LoadMesh(std::string_view) override { return -1; }
	int LoadTexture(std::string_view) override { return -1; }
	int UploadTexture(int, int, const std::vector<uint8_t>&) override { return -1; }
	void UpdateTexture(int, const std::vector<uint8_t>&) override {}
	void UnloadMesh(int) override {}
	void UnloadTexture(int) override {}
	void SetTextureFilter(int, bool) override {}
	void SetTextureWrap(int, bool) override {}

	int LoadSound(std::string_view) override { return -1; }
	void UnloadSound(int) override {}
	void PlaySound(int) override {}
	void SetSoundPitch(int, float) override {}
	int LoadMusic(std::string_view) override { return -1; }
	void UnloadMusic(int) override {}
	void PlayMusic(int, bool) override {}
	void StopMusic(int) override {}
	void UpdateMusic(int) override {}
	void SetMusicVolume(int, float) override {}

	void StartSynth(engine::audio::SynthSource*) override {}
	void StopSynth() override {}

	void WriteSave(std::string_view, std::string_view) override {}
	[[nodiscard]] std::string ReadSave(std::string_view) const override { return {}; }
	[[nodiscard]] bool HasSave(std::string_view) const override { return false; }
	void DeleteSave(std::string_view) override {}
	[[nodiscard]] std::vector<std::string> ListSaves() const override { return {}; }

private:
	std::string assetDirectory_;
	engine::platform::DebugUi debugUi_;
};
