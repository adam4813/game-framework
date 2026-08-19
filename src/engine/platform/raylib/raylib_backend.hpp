#pragma once

#include "engine/platform/platform.hpp"

namespace engine::platform {

// Raylib-backed Platform implementation. Raylib headers are included only in
// the matching .cpp so raylib types never leak into the rest of the codebase.
class RaylibBackend final : public Platform {
public:
	void Init(int width, int height, std::string_view title) override;
	[[nodiscard]] bool ShouldClose() const override;
	void Shutdown() override;
	[[nodiscard]] int Width() const override;
	[[nodiscard]] int Height() const override;
	[[nodiscard]] float DeltaTime() const override;
	[[nodiscard]] double Time() const override;
	[[nodiscard]] std::string AssetDirectory() const override;

	void BeginFrame(Rgba clear) override;
	void EndFrame() override;

	[[nodiscard]] glm::vec2 MousePosition() const override;
	[[nodiscard]] glm::vec2 MouseDelta() const override;
	[[nodiscard]] glm::vec2 MouseWheel() const override;
	[[nodiscard]] bool MousePressed(MouseButton button) const override;
	[[nodiscard]] bool MouseReleased(MouseButton button) const override;
	[[nodiscard]] bool MouseDown(MouseButton button) const override;
	[[nodiscard]] bool IsKeyDown(int key_code) const override;
	[[nodiscard]] bool IsMouseButtonDown(int button) const override;
	[[nodiscard]] int CharInput() const override;
	[[nodiscard]] std::string TextInput() const override;

	void DrawRect(Rect r, Rgba c) override;
	void DrawRectLines(Rect r, float thickness, Rgba c) override;
	void DrawRoundedRect(Rect r, float roundness, Rgba c) override;
	void DrawRoundedRectLines(Rect r, float roundness, float thickness, Rgba c) override;
	void DrawText(std::string_view text, float x, float y, float size, Rgba c) override;
	[[nodiscard]] float MeasureText(std::string_view text, float size) const override;
	void DrawLine(glm::vec2 a, glm::vec2 b, float thickness, Rgba c) override;
	void DrawCircle(glm::vec2 center, float radius, Rgba c) override;
	void BeginScissor(Rect r) override;
	void EndScissor() override;

	void BeginMode3D(const Camera3DParams& camera) override;
	void EndMode3D() override;
	void SetLighting(const LightParams& lighting) override;
	void DrawCube(
		const glm::mat4& transform,
		glm::vec3 size,
		Rgba c,
		int texture,
		bool cast_shadow,
		bool wireframe
	) override;
	void DrawSphere(
		const glm::mat4& transform,
		float radius,
		Rgba c,
		int texture,
		bool cast_shadow,
		bool wireframe
	) override;
	void DrawQuad(
		const glm::mat4& transform,
		glm::vec2 size,
		Rgba c,
		int texture,
		bool cast_shadow,
		bool wireframe
	) override;
	void DrawCapsule(
		const glm::mat4& transform,
		float radius,
		float height,
		Rgba c,
		int texture,
		bool cast_shadow,
		bool wireframe
	) override;
	void
	DrawMesh(int handle, const glm::mat4& transform, Rgba tint, int texture, bool cast_shadow, bool wireframe) override;

	int UploadDynamicMesh(
		const std::vector<glm::vec3>& vertices,
		const std::vector<uint32_t>& indices,
		const std::vector<glm::vec4>& colors,
		const std::vector<glm::vec2>& uvs
	) override;
	void UnloadDynamicMesh(int handle) override;
	void DrawDynamicMesh(int handle, const glm::mat4& transform, int texture, bool wireframe) override;

	int LoadMesh(std::string_view path) override;
	int LoadTexture(std::string_view path) override;
	void UnloadMesh(int handle) override;
	void UnloadTexture(int handle) override;

	int LoadSound(std::string_view path) override;
	void UnloadSound(int handle) override;
	void PlaySound(int handle) override;

	void WriteSave(std::string_view name, std::string_view data) override;
	[[nodiscard]] std::string ReadSave(std::string_view name) const override;
	[[nodiscard]] bool HasSave(std::string_view name) const override;
	void DeleteSave(std::string_view name) override;
	[[nodiscard]] std::vector<std::string> ListSaves() const override;
};

} // namespace engine::platform
