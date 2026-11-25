// Throwaway spike (render pipeline map, tickets 05 and 06): SDL3 + Dawn, one compute pass feeding an instanced triangle,
// GPU timestamps, an offscreen self-test. Written against the subset of webgpu.h that emdawnwebgpu also has, and as
// frame callbacks, because the browser can't block.
#include "pass.hpp"

namespace {

// 4x4 triangles in 256x256: every cell's center has that instance's color and every cell's corner the clear color.
struct OffscreenApp {
	static constexpr uint32_t kSize = 256, kCount = 16, kCell = kSize / 4;
	WGPUInstance instance = nullptr;
	WGPUDevice device = nullptr;
	Options options;
	Pass p{};
	WGPUTexture texture = nullptr;
	WGPUTextureView view = nullptr;
	WGPUBuffer readback = nullptr;
	int frame = 0, waited = 0;
	bool done = false, ok = false;

	bool Start() {
		p = MakePass(instance, device, WGPUTextureFormat_RGBA8Unorm, kCount);
		WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
		desc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
		desc.size = {kSize, kSize, 1};
		desc.format = WGPUTextureFormat_RGBA8Unorm;
		texture = wgpuDeviceCreateTexture(device, &desc);
		view = wgpuTextureCreateView(texture, nullptr);
		readback = MakeBuffer(device, kSize * kSize * 4, WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst);
		return true;
	}

	bool Frame() {
		if (frame < 8) {
			WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
			Pass::Read* read = Encode(p, encoder, view, kCount, 0.0f, false);
			if (frame == 7) {
				WGPUTexelCopyTextureInfo from = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
				from.texture = texture;
				WGPUTexelCopyBufferInfo to = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
				to.buffer = readback;
				to.layout.bytesPerRow = kSize * 4;
				to.layout.rowsPerImage = kSize;
				const WGPUExtent3D extent = {kSize, kSize, 1};
				wgpuCommandEncoderCopyTextureToBuffer(encoder, &from, &to, &extent);
			}
			Submit(p, encoder);
			MapTimestamps(p, read);
			if (++frame == 8) {
				WGPUBufferMapCallbackInfo callback = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
				callback.mode = WGPUCallbackMode_AllowProcessEvents;
				callback.userdata1 = this;
				callback.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* user, void*) {
					auto& app = *static_cast<OffscreenApp*>(user);
					app.done = true;
					app.ok = status == WGPUMapAsyncStatus_Success;
				};
				wgpuBufferMapAsync(readback, WGPUMapMode_Read, 0, kSize * kSize * 4, callback);
			}
			platform::PumpEvents(instance);
			return true;
		}
		platform::PumpEvents(instance);
		if (!done) return true;
		// The timestamp reads arrive on their own schedule.
		return ok && ++waited < 100 && std::any_of(std::begin(p.reads), std::end(p.reads), [](auto& r) { return r.busy; });
	}

	int Finish() {
		if (!ok) {
			std::printf("readback failed\n");
			return 1;
		}
		const auto* pixels = static_cast<const uint8_t*>(wgpuBufferGetConstMappedRange(readback, 0, kSize * kSize * 4));
		auto check = [&](uint32_t x, uint32_t y, int r, int g, int b) {
			const uint8_t* px = pixels + (y * kSize + x) * 4;
			const bool same = std::abs(px[0] - r) <= 1 && std::abs(px[1] - g) <= 1 && std::abs(px[2] - b) <= 1;
			if (!same) std::printf("pixel %u,%u is %d,%d,%d, expected %d,%d,%d\n", x, y, px[0], px[1], px[2], r, g, b);
			return same;
		};
		int bad = 0;
		for (uint32_t i = 0; i < kCount; ++i) {
			const uint32_t x = (i % 4) * kCell, y = (i / 4) * kCell;
			bad += !check(x + kCell / 2, y + kCell / 2, int(i) * 17, (15 - int(i)) * 17, 136);
			bad += !check(x + 1, y + 1, 51, 102, 153);
		}
		uint64_t hash = 14695981039346656037ull;
		for (uint32_t i = 0; i < kSize * kSize * 4; ++i) hash = (hash ^ pixels[i]) * 1099511628211ull;
		std::printf("offscreen: %s, %d bad of %u, image hash %016llx\n", bad ? "FAIL" : "PASS", bad, kCount * 2,
		            (unsigned long long)hash);
		if (!options.shot.empty()) {
			if (FILE* file = std::fopen(options.shot.c_str(), "wb")) {
				std::fprintf(file, "P6\n%u %u\n255\n", kSize, kSize);
				for (uint32_t i = 0; i < kSize * kSize; ++i) std::fwrite(pixels + i * 4, 1, 3, file);
				std::fclose(file);
			}
		}
		PrintTimes(p);
		return bad ? 1 : 0;
	}
};

struct WindowApp {
	WGPUInstance instance = nullptr;
	WGPUDevice device = nullptr;
	Options options;
	WGPUSurface surface = nullptr;
	WGPUSurfaceConfiguration config = WGPU_SURFACE_CONFIGURATION_INIT;
	Pass p{};
	Clock::time_point start;
	uint32_t frame = 0, reconfigured = 0, skipped = 0;
	double cpu_ms = 0;

	bool Start() {
		surface = platform::CreateSurface(instance);
		if (!surface) {
			std::printf("no surface\n");
			return false;
		}
		config.device = device;
		config.format = WGPUTextureFormat_BGRA8Unorm;
		config.presentMode = options.immediate ? WGPUPresentMode_Immediate : WGPUPresentMode_Fifo;
		platform::WindowPixelSize(config.width, config.height);
		std::printf("surface %ux%u\n", config.width, config.height);
		wgpuSurfaceConfigure(surface, &config);
		p = MakePass(instance, device, config.format, options.instances);
#ifdef SPIKE_IMGUI
		if (options.imgui) {
			ImGui::CreateContext();
			ImGui_ImplSDL3_InitForOther(platform::Window());
			ImGui_ImplWGPU_InitInfo info;
			info.Device = device;
			info.RenderTargetFormat = config.format;
			ImGui_ImplWGPU_Init(&info);
		}
#endif
		start = Clock::now();
		return true;
	}

	bool Frame() {
		if (options.frames != 0 && frame >= options.frames) return false;
		++frame;
		const auto begin = Clock::now();
		if (!platform::PollEvents(options.imgui)) return false;
		uint32_t width = 0, height = 0;
		platform::WindowPixelSize(width, height);
		if (width != config.width || height != config.height) {
			config.width = width;
			config.height = height;
			++reconfigured;
			std::printf("reconfigure %ux%u\n", width, height);
			if (width && height) wgpuSurfaceConfigure(surface, &config);
		}
		WGPUSurfaceTexture current = WGPU_SURFACE_TEXTURE_INIT;
		wgpuSurfaceGetCurrentTexture(surface, &current);
		if (current.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
		    current.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
			++skipped;
			platform::PumpEvents(instance);
			return true;
		}
#ifdef SPIKE_IMGUI
		if (options.imgui) {
			ImGui_ImplWGPU_NewFrame();
			ImGui_ImplSDL3_NewFrame();
			ImGui::NewFrame();
			ImGui::ShowDemoWindow();
			ImGui::Render();
		}
#endif
		WGPUTextureView view = wgpuTextureCreateView(current.texture, nullptr);
		WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
		const float time = float(Ms(Clock::now() - start) / 1000.0);
		Pass::Read* read = Encode(p, encoder, view, options.instances, time, options.imgui);
		Submit(p, encoder);
		MapTimestamps(p, read);
		platform::Present(surface);
		wgpuTextureViewRelease(view);
		wgpuTextureRelease(current.texture);
		platform::PumpEvents(instance);
		cpu_ms += Ms(Clock::now() - begin);
		return true;
	}

	int Finish() {
		const double total = Ms(Clock::now() - start);
		std::printf("window: %u frames in %.0f ms = %.3f ms/frame (cpu %.3f), %ux%u, reconfigured %u, skipped %u\n", frame,
		            total, total / std::max(frame, 1u), cpu_ms / std::max(frame, 1u), config.width, config.height,
		            reconfigured, skipped);
		PrintTimes(p);
#ifdef SPIKE_IMGUI
		if (options.imgui) {
			ImGui_ImplWGPU_Shutdown();
			ImGui_ImplSDL3_Shutdown();
			ImGui::DestroyContext();
		}
#endif
		wgpuSurfaceUnconfigure(surface);
		wgpuSurfaceRelease(surface);
		return 0;
	}
};

// The app outlives main on the web, so it's never freed.
template <typename App>
int Run(WGPUInstance instance, WGPUDevice device, const Options& options) {
	auto* app = new App;
	app->instance = instance;
	app->device = device;
	app->options = options;
	if (!app->Start()) return 1;
	return platform::RunLoop([](void* user) { return static_cast<App*>(user)->Frame(); },
	                         [](void* user) {
		                         const int result = static_cast<App*>(user)->Finish();
		                         wgpuDeviceRelease(static_cast<App*>(user)->device);
		                         if (platform::Window()) platform::CloseWindow();
		                         return result;
	                         },
	                         app);
}

Options Parse(int argc, char** argv) {
	Options o;
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		auto next = [&] { return i + 1 < argc ? argv[++i] : ""; };
		if (arg == "--list") o.list = true;
		else if (arg == "--offscreen") o.offscreen = true;
		else if (arg == "--imgui") o.imgui = true;
		else if (arg == "--enumerate") o.enumerate = true;
		else if (arg == "--fallback") o.fallback = true;
		else if (arg == "--immediate") o.immediate = true;
		else if (arg == "--quantized-time") o.quantized_time = true;
		else if (arg == "--backend") o.backend = next();
		else if (arg == "--vendor") o.vendor = uint32_t(std::strtoul(next(), nullptr, 0));
		else if (arg == "--device") o.device = uint32_t(std::strtoul(next(), nullptr, 0));
		else if (arg == "--frames") o.frames = uint32_t(std::strtoul(next(), nullptr, 0));
		else if (arg == "--instances") o.instances = uint32_t(std::strtoul(next(), nullptr, 0));
		else if (arg == "--shot") o.shot = next();
	}
	return o;
}
}  // namespace

int main(int argc, char** argv) {
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	const Options options = Parse(argc, argv);
	const auto start = Clock::now();
	WGPUInstance instance = options.list || options.enumerate ? platform::CreateNativeInstance() : platform::CreateInstance();
	if (options.list) {
		platform::ListAdapters(instance);
		return 0;
	}
	if (!options.offscreen && !platform::OpenWindow("webgpu spike", 1280, 720)) {
		std::printf("no window\n");
		return 1;
	}
	WGPUDevice device = platform::CreateDevice(instance, options);
	if (!device) return 2;
	std::printf("device ready after %.0f ms\n", Ms(Clock::now() - start));
	return options.offscreen ? Run<OffscreenApp>(instance, device, options) : Run<WindowApp>(instance, device, options);
}
