#include "platform.hpp"

#include <cstdio>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

#include <SDL3/SDL.h>
#include <dawn/native/DawnNative.h>
#ifdef _WIN32
#include <dawn/native/D3DBackend.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#endif
#ifdef SPIKE_IMGUI
#include <imgui_impl_sdl3.h>
#endif

namespace {
// Dawn's C++ API. It passes std::vector over the DLL boundary, so a Debug exe can't use it with a Release Dawn.
std::unique_ptr<dawn::native::Instance> g_native;
WGPUInstance g_instance = nullptr;
SDL_Window* g_window = nullptr;

const char* BackendName(WGPUBackendType type) {
	switch (type) {
		case WGPUBackendType_Null: return "null";
		case WGPUBackendType_D3D11: return "d3d11";
		case WGPUBackendType_D3D12: return "d3d12";
		case WGPUBackendType_Metal: return "metal";
		case WGPUBackendType_Vulkan: return "vulkan";
		default: return "other";
	}
}

WGPUBackendType BackendFromName(const std::string& name) {
	for (auto type : {WGPUBackendType_Null, WGPUBackendType_D3D11, WGPUBackendType_D3D12, WGPUBackendType_Metal,
	                  WGPUBackendType_Vulkan})
		if (name == BackendName(type)) return type;
	return WGPUBackendType_Undefined;
}

std::vector<dawn::native::Adapter> Enumerate(const std::string& backend) {
	WGPURequestAdapterOptions options = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
	options.backendType = BackendFromName(backend);
	return g_native->EnumerateAdapters(&options);
}

void PrintAdapter(WGPUAdapter adapter) {
	WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;
	wgpuAdapterGetInfo(adapter, &info);
	std::printf("adapter: backend=%s type=%d vendor=0x%04x device=0x%04x '%s' '%s' arch='%s'\n",
	            BackendName(info.backendType), int(info.adapterType), info.vendorID, info.deviceID,
	            Str(info.device).c_str(), Str(info.description).c_str(), Str(info.architecture).c_str());
	wgpuAdapterInfoFreeMembers(info);
}

// C API only. A WARP vendor/device pair is turned into its LUID, because the C API can't enumerate.
WGPUAdapter RequestAdapter(const Options& options) {
	WGPURequestAdapterOptions request = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
	request.backendType = BackendFromName(options.backend);
	request.powerPreference = WGPUPowerPreference_HighPerformance;
	request.forceFallbackAdapter = options.fallback;
#ifdef _WIN32
	dawn::native::d3d::RequestAdapterOptionsLUID luid;
	if (options.vendor == 0x1414) {
		Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
		Microsoft::WRL::ComPtr<IDXGIAdapter> warp;
		DXGI_ADAPTER_DESC desc{};
		if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))) || FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))) ||
		    FAILED(warp->GetDesc(&desc)))
			return nullptr;
		luid.adapterLUID = desc.AdapterLuid;
		request.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&luid);
	}
#endif
	struct Result {
		WGPUAdapter adapter = nullptr;
		bool done = false;
	} result;
	WGPURequestAdapterCallbackInfo callback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
	callback.mode = WGPUCallbackMode_AllowProcessEvents;
	callback.userdata1 = &result;
	callback.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* user,
	                       void*) {
		auto& out = *static_cast<Result*>(user);
		out.done = true;
		if (status == WGPURequestAdapterStatus_Success)
			out.adapter = adapter;
		else
			std::fprintf(stderr, "requestAdapter failed: %s\n", Str(message).c_str());
	};
	wgpuInstanceRequestAdapter(g_instance, &request, callback);
	while (!result.done) wgpuInstanceProcessEvents(g_instance);
	return result.adapter;
}
}  // namespace

namespace platform {

WGPUInstance CreateInstance() {
	g_instance = wgpuCreateInstance(nullptr);
	return g_instance;
}

// The C++ instance: adapter enumeration, and SPIKE_SEARCH. This Dawn can't find system DLLs by bare name
// (LoadLibraryEx error 87), so SPIKE_SEARCH adds a folder, e.g. C:/Windows/System32/, for vulkan-1.dll and
// d3dcompiler_47.dll.
WGPUInstance CreateNativeInstance() {
	dawn::native::DawnInstanceDescriptor search;
	std::string_view path;
	if (const char* env = SDL_getenv("SPIKE_SEARCH")) {
		path = env;
		search.additionalRuntimeSearchPaths = std::span(&path, 1);
	}
	wgpu::InstanceDescriptor desc;
	desc.nextInChain = &search;
	g_native = std::make_unique<dawn::native::Instance>(&desc);
	g_instance = g_native->Get();
	return g_instance;
}

void ListAdapters(WGPUInstance) {
	for (const auto& native : Enumerate("")) {
		WGPUAdapter adapter = native.Get();
		PrintAdapter(adapter);
		WGPULimits l = WGPU_LIMITS_INIT;
		wgpuAdapterGetLimits(adapter, &l);
		std::printf(
		    "  texture2D=%u layers=%u bindGroups=%u bindingsPerGroup=%u sampledTex/stage=%u samplers/stage=%u "
		    "storageBuf/stage=%u storageTex/stage=%u uniformBuf/stage=%u\n",
		    l.maxTextureDimension2D, l.maxTextureArrayLayers, l.maxBindGroups, l.maxBindingsPerBindGroup,
		    l.maxSampledTexturesPerShaderStage, l.maxSamplersPerShaderStage, l.maxStorageBuffersPerShaderStage,
		    l.maxStorageTexturesPerShaderStage, l.maxUniformBuffersPerShaderStage);
		std::printf(
		    "  uniformBinding=%llu storageBinding=%llu bufferSize=%llu uniformAlign=%u storageAlign=%u vertexAttrs=%u "
		    "colorBytes/sample=%u wgStorage=%u wgInvocations=%u immediate=%u\n",
		    (unsigned long long)l.maxUniformBufferBindingSize, (unsigned long long)l.maxStorageBufferBindingSize,
		    (unsigned long long)l.maxBufferSize, l.minUniformBufferOffsetAlignment, l.minStorageBufferOffsetAlignment,
		    l.maxVertexAttributes, l.maxColorAttachmentBytesPerSample, l.maxComputeWorkgroupStorageSize,
		    l.maxComputeInvocationsPerWorkgroup, l.maxImmediateSize);
		const std::pair<WGPUFeatureName, const char*> features[] = {
		    {WGPUFeatureName_TimestampQuery, "timestamp-query"},
		    {WGPUFeatureName_DepthClipControl, "depth-clip-control"},
		    {WGPUFeatureName_Float32Filterable, "float32-filterable"},
		    {WGPUFeatureName_RG11B10UfloatRenderable, "rg11b10ufloat-renderable"},
		    {WGPUFeatureName_TextureCompressionBC, "bc"},
		    {WGPUFeatureName_IndirectFirstInstance, "indirect-first-instance"},
		    {WGPUFeatureName_ShaderF16, "shader-f16"},
		    {WGPUFeatureName_Subgroups, "subgroups"},
		    {WGPUFeatureName_TextureFormatsTier1, "texture-formats-tier1"},
		    {WGPUFeatureName_TextureFormatsTier2, "texture-formats-tier2"}};
		std::printf("  features:");
		for (auto [feature, name] : features)
			if (wgpuAdapterHasFeature(adapter, feature)) std::printf(" %s", name);
		std::printf("\n");
	}
}

WGPUDevice CreateDevice(WGPUInstance, const Options& options) {
	WGPUAdapter adapter = nullptr;
	std::vector<dawn::native::Adapter> candidates;  // owns the adapter in the C++ path
	if (g_native) {
		candidates = Enumerate(options.backend);
		for (const auto& candidate : candidates) {
			WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;
			wgpuAdapterGetInfo(candidate.Get(), &info);
			const bool match = (!options.vendor || info.vendorID == options.vendor) &&
			                   (!options.device || info.deviceID == options.device);
			wgpuAdapterInfoFreeMembers(info);
			if (match) {
				adapter = candidate.Get();
				break;
			}
		}
	} else {
		adapter = RequestAdapter(options);
	}
	if (!adapter) {
		std::fprintf(stderr, "no adapter matches\n");
		return nullptr;
	}
	PrintAdapter(adapter);

	std::vector<WGPUFeatureName> features;
	for (auto feature : WantedFeatures())
		if (wgpuAdapterHasFeature(adapter, feature)) features.push_back(feature);

	const char* disabled[] = {"timestamp_quantization"};
	WGPUDawnTogglesDescriptor toggles = WGPU_DAWN_TOGGLES_DESCRIPTOR_INIT;
	toggles.disabledToggleCount = options.quantized_time ? 0 : 1;
	toggles.disabledToggles = disabled;

	WGPUDeviceDescriptor desc = WGPU_DEVICE_DESCRIPTOR_INIT;
	desc.nextInChain = &toggles.chain;
	desc.requiredFeatureCount = features.size();
	desc.requiredFeatures = features.data();
	desc.uncapturedErrorCallbackInfo.callback = [](WGPUDevice const*, WGPUErrorType type, WGPUStringView message,
	                                               void*, void*) {
		std::fprintf(stderr, "uncaptured error %d: %s\n", int(type), Str(message).c_str());
	};
	desc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
	desc.deviceLostCallbackInfo.callback = [](WGPUDevice const*, WGPUDeviceLostReason reason, WGPUStringView message,
	                                          void*, void*) {
		std::fprintf(stderr, "device lost (%d): %s\n", int(reason), Str(message).c_str());
	};

	struct Result {
		WGPUDevice device = nullptr;
		bool done = false;
	} result;
	WGPURequestDeviceCallbackInfo callback = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
	callback.mode = WGPUCallbackMode_AllowProcessEvents;
	callback.userdata1 = &result;
	callback.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* user,
	                       void*) {
		auto& out = *static_cast<Result*>(user);
		out.done = true;
		if (status == WGPURequestDeviceStatus_Success)
			out.device = device;
		else
			std::fprintf(stderr, "requestDevice failed: %s\n", Str(message).c_str());
	};
	wgpuAdapterRequestDevice(adapter, &desc, callback);
	while (!result.done) wgpuInstanceProcessEvents(g_instance);
	return result.device;
}

bool OpenWindow(const char* title, int width, int height) {
	if (!SDL_Init(SDL_INIT_VIDEO)) return false;
	g_window = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
	return g_window != nullptr;
}

SDL_Window* Window() { return g_window; }

WGPUSurface CreateSurface(WGPUInstance instance) {
	WGPUSurfaceDescriptor desc = WGPU_SURFACE_DESCRIPTOR_INIT;
	const SDL_PropertiesID props = SDL_GetWindowProperties(g_window);
#ifdef _WIN32
	WGPUSurfaceSourceWindowsHWND source = WGPU_SURFACE_SOURCE_WINDOWS_HWND_INIT;
	source.hwnd = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
	source.hinstance = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr);
	desc.nextInChain = &source.chain;
#elif defined(__linux__)
	// Wayland isn't exercised by the spike: force SDL's X11 driver when presenting on Linux.
	WGPUSurfaceSourceXlibWindow source = WGPU_SURFACE_SOURCE_XLIB_WINDOW_INIT;
	source.display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
	source.window = uint64_t(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
	desc.nextInChain = &source.chain;
#endif
	return wgpuInstanceCreateSurface(instance, &desc);
}

void WindowPixelSize(uint32_t& width, uint32_t& height) {
	int w = 0, h = 0;
	SDL_GetWindowSizeInPixels(g_window, &w, &h);
	width = uint32_t(w);
	height = uint32_t(h);
}

bool PollEvents([[maybe_unused]] bool to_imgui) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
#ifdef SPIKE_IMGUI
		if (to_imgui) ImGui_ImplSDL3_ProcessEvent(&event);
#endif
		if (event.type == SDL_EVENT_QUIT) return false;
	}
	return true;
}

void Present(WGPUSurface surface) { wgpuSurfacePresent(surface); }
void PumpEvents(WGPUInstance instance) { wgpuInstanceProcessEvents(instance); }

void CloseWindow() {
	SDL_DestroyWindow(g_window);
	SDL_Quit();
}

}  // namespace platform
