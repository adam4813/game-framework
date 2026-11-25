// The browser's half: the page shell (shell.html) has requested the device before main runs; web_device.js (--pre-js)
// held main back with a run dependency until it arrived. No Asyncify.
#include "platform.hpp"

#include <cstdio>

#include <SDL3/SDL.h>
#include <emscripten.h>

namespace {
// The device belongs to `instance`, so wgpuInstanceProcessEvents runs its AllowProcessEvents callbacks.
EM_JS(WGPUDevice, ImportDevice, (WGPUInstance instance), {
	return WebGPU.importJsDevice(Module["webgpuDevice"], instance);
});
EM_JS(void, Finished, (int code), { Module["spikeFinished"] = code; });

struct Loop {
	bool (*frame)(void*);
	int (*finish)(void*);
	void* user;
};
}  // namespace

namespace platform {

WGPUInstance CreateInstance() { return wgpuCreateInstance(nullptr); }
WGPUInstance CreateNativeInstance() { return nullptr; }
void ListAdapters(WGPUInstance) { std::printf("--list is native only\n"); }

WGPUDevice CreateDevice(WGPUInstance instance, const Options&) {
	WGPUDevice device = ImportDevice(instance);
	if (!device) return nullptr;
	WGPULimits l = WGPU_LIMITS_INIT;
	wgpuDeviceGetLimits(device, &l);
	std::printf(
	    "device limits (from C++): texture2D=%u layers=%u bindGroups=%u bindingsPerGroup=%u sampledTex/stage=%u samplers/stage=%u "
	    "storageBuf/stage=%u storageTex/stage=%u uniformBuf/stage=%u\n",
	    l.maxTextureDimension2D, l.maxTextureArrayLayers, l.maxBindGroups, l.maxBindingsPerBindGroup,
	    l.maxSampledTexturesPerShaderStage, l.maxSamplersPerShaderStage, l.maxStorageBuffersPerShaderStage,
	    l.maxStorageTexturesPerShaderStage, l.maxUniformBuffersPerShaderStage);
	std::printf(
	    "  uniformBinding=%llu storageBinding=%llu bufferSize=%llu uniformAlign=%u storageAlign=%u vertexAttrs=%u "
	    "colorBytes/sample=%u wgStorage=%u wgInvocations=%u\n",
	    (unsigned long long)l.maxUniformBufferBindingSize, (unsigned long long)l.maxStorageBufferBindingSize,
	    (unsigned long long)l.maxBufferSize, l.minUniformBufferOffsetAlignment, l.minStorageBufferOffsetAlignment,
	    l.maxVertexAttributes, l.maxColorAttachmentBytesPerSample, l.maxComputeWorkgroupStorageSize,
	    l.maxComputeInvocationsPerWorkgroup);
	std::printf("  timestamp-query=%d\n", int(wgpuDeviceHasFeature(device, WGPUFeatureName_TimestampQuery)));
	WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;
	if (wgpuDeviceGetAdapterInfo(device, &info) == WGPUStatus_Success) {
		std::printf("  adapter: '%s' '%s' arch='%s' vendor='%s' type=%d\n", Str(info.device).c_str(),
		            Str(info.description).c_str(), Str(info.architecture).c_str(), Str(info.vendor).c_str(),
		            int(info.adapterType));
		wgpuAdapterInfoFreeMembers(info);
	} else {
		std::printf("  adapter info unavailable\n");
	}
	return device;
}

WGPUSurface CreateSurface(WGPUInstance instance) {
	const SDL_PropertiesID props = SDL_GetWindowProperties(Window());
	const char* selector = SDL_GetStringProperty(props, SDL_PROP_WINDOW_EMSCRIPTEN_CANVAS_ID_STRING, "#canvas");
	std::printf("canvas selector: %s\n", selector);
	WGPUEmscriptenSurfaceSourceCanvasHTMLSelector source = WGPU_EMSCRIPTEN_SURFACE_SOURCE_CANVAS_HTML_SELECTOR_INIT;
	source.selector = Sv(selector);
	WGPUSurfaceDescriptor desc = WGPU_SURFACE_DESCRIPTOR_INIT;
	desc.nextInChain = &source.chain;
	return wgpuInstanceCreateSurface(instance, &desc);
}

void Present(WGPUSurface) {}  // the browser presents; wgpuSurfacePresent aborts here
void PumpEvents(WGPUInstance instance) { wgpuInstanceProcessEvents(instance); }

int RunLoop(bool (*frame)(void*), int (*finish)(void*), void* user) {
	// main returns; the runtime stays alive for the frame callback.
	emscripten_set_main_loop_arg(
	    [](void* arg) {
		    auto* loop = static_cast<Loop*>(arg);
		    if (loop->frame(loop->user)) return;
		    emscripten_cancel_main_loop();
		    Finished(loop->finish(loop->user));
	    },
	    new Loop{frame, finish, user}, 0, false);
	return 0;
}

}  // namespace platform
