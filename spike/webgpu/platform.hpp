// The part of the spike that differs per platform: window, surface, device start-up, present and event pump.
// Ticket 06 adds platform_web.cpp beside platform_native.cpp.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <webgpu/webgpu.h>

struct SDL_Window;

struct Options {
	bool list = false;
	bool offscreen = false;
	bool imgui = false;
	bool immediate = false;       // present mode Immediate instead of Fifo
	bool quantized_time = false;  // leave Dawn's timestamp quantization on
	std::string backend;          // "", d3d12, d3d11, vulkan, null
	uint32_t vendor = 0;          // 0 = any
	uint32_t device = 0;          // 0 = any
	uint32_t frames = 300;        // window mode: stop after this many frames (0 = until closed)
	uint32_t instances = 10000;
	std::string shot;             // offscreen: write a .ppm here
};

// What both platforms ask of the device. Native passes it to wgpuAdapterRequestDevice, the web shell to requestDevice.
std::vector<WGPUFeatureName> WantedFeatures();

namespace platform {
WGPUInstance CreateInstance();
// Lists every adapter of every built backend with limits and features (native only).
void ListAdapters(WGPUInstance instance);
// Returns null when no adapter matches.
WGPUDevice CreateDevice(WGPUInstance instance, const Options& options);
bool OpenWindow(const char* title, int width, int height);
SDL_Window* Window();
WGPUSurface CreateSurface(WGPUInstance instance);
void WindowPixelSize(uint32_t& width, uint32_t& height);
bool PollEvents(bool to_imgui);  // false = quit
void Present(WGPUSurface surface);
void PumpEvents(WGPUInstance instance);
void CloseWindow();
}  // namespace platform

inline WGPUStringView Sv(const char* text) { return {text, WGPU_STRLEN}; }
inline std::string Str(WGPUStringView view) {
	if (!view.data) return {};
	return view.length == WGPU_STRLEN ? std::string(view.data) : std::string(view.data, view.length);
}
