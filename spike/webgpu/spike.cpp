// Throwaway spike (render pipeline map, ticket 05): SDL3 + Dawn, one compute pass feeding an instanced triangle, GPU
// timestamps, an offscreen self-test. Written against the subset of webgpu.h that emdawnwebgpu also has.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "platform.hpp"
#include "shaders.hpp"
#ifdef SPIKE_IMGUI
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_wgpu.h>
#endif

std::vector<WGPUFeatureName> WantedFeatures() { return {WGPUFeatureName_TimestampQuery}; }

namespace {

using Clock = std::chrono::steady_clock;
double Ms(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }

struct Params {
	uint32_t count, side;
	float time, pad;
};

struct Pass {
	WGPUInstance instance;
	WGPUDevice device;
	WGPUQueue queue;
	WGPUBuffer params, instances;
	WGPUComputePipeline compute;
	WGPURenderPipeline draw;
	WGPUBindGroup compute_group, draw_group;
	WGPUQuerySet queries = nullptr;
	WGPUBuffer query_resolve = nullptr;
	struct Read {
		WGPUBuffer buffer = nullptr;
		bool busy = false;
	};
	Read reads[4];
	double compute_ms = 0, draw_ms = 0;
	int timed_frames = 0, negative = 0;
};

WGPUBuffer MakeBuffer(WGPUDevice device, uint64_t size, WGPUBufferUsage usage) {
	WGPUBufferDescriptor desc = WGPU_BUFFER_DESCRIPTOR_INIT;
	desc.size = size;
	desc.usage = usage;
	return wgpuDeviceCreateBuffer(device, &desc);
}

WGPUShaderModule MakeShader(WGPUDevice device, const char* label, const char* code) {
	WGPUShaderSourceWGSL source = WGPU_SHADER_SOURCE_WGSL_INIT;
	source.code = Sv(code);
	WGPUShaderModuleDescriptor desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
	desc.nextInChain = &source.chain;
	desc.label = Sv(label);
	return wgpuDeviceCreateShaderModule(device, &desc);
}

WGPUBindGroup MakeGroup(WGPUDevice device, WGPUBindGroupLayout layout, std::initializer_list<WGPUBuffer> buffers) {
	std::vector<WGPUBindGroupEntry> entries;
	for (WGPUBuffer buffer : buffers) {
		WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
		entry.binding = uint32_t(entries.size());
		entry.buffer = buffer;
		entries.push_back(entry);
	}
	WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
	desc.layout = layout;
	desc.entryCount = entries.size();
	desc.entries = entries.data();
	return wgpuDeviceCreateBindGroup(device, &desc);
}

Pass MakePass(WGPUInstance instance, WGPUDevice device, WGPUTextureFormat format, uint32_t count) {
	Pass p{};
	p.instance = instance;
	p.device = device;
	p.queue = wgpuDeviceGetQueue(device);
	p.params = MakeBuffer(device, sizeof(Params), WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst);
	p.instances = MakeBuffer(device, uint64_t(count) * 16, WGPUBufferUsage_Storage);

	WGPUComputePipelineDescriptor compute = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
	compute.compute.module = MakeShader(device, "compute.wgsl", kComputeWgsl);
	compute.compute.entryPoint = Sv("main");
	p.compute = wgpuDeviceCreateComputePipeline(device, &compute);
	p.compute_group =
	    MakeGroup(device, wgpuComputePipelineGetBindGroupLayout(p.compute, 0), {p.params, p.instances});

	WGPUShaderModule module = MakeShader(device, "draw.wgsl", kDrawWgsl);
	WGPUColorTargetState target = WGPU_COLOR_TARGET_STATE_INIT;
	target.format = format;
	WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
	fragment.module = module;
	fragment.entryPoint = Sv("fs");
	fragment.targetCount = 1;
	fragment.targets = &target;
	WGPURenderPipelineDescriptor draw = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
	draw.vertex.module = module;
	draw.vertex.entryPoint = Sv("vs");
	draw.fragment = &fragment;
	draw.primitive.topology = WGPUPrimitiveTopology_TriangleList;
	p.draw = wgpuDeviceCreateRenderPipeline(device, &draw);
	p.draw_group = MakeGroup(device, wgpuRenderPipelineGetBindGroupLayout(p.draw, 0), {p.instances});

	if (wgpuDeviceHasFeature(device, WGPUFeatureName_TimestampQuery)) {
		WGPUQuerySetDescriptor queries = WGPU_QUERY_SET_DESCRIPTOR_INIT;
		queries.type = WGPUQueryType_Timestamp;
		queries.count = 4;
		p.queries = wgpuDeviceCreateQuerySet(device, &queries);
		p.query_resolve = MakeBuffer(device, 32, WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc);
		for (auto& read : p.reads)
			read.buffer = MakeBuffer(device, 32, WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst);
	}
	return p;
}

Pass::Read* Encode(Pass& p, WGPUCommandEncoder encoder, WGPUTextureView view, uint32_t count, float time,
                   bool with_imgui) {
	const Params params{count, uint32_t(std::ceil(std::sqrt(float(count)))), time, 0};
	wgpuQueueWriteBuffer(p.queue, p.params, 0, &params, sizeof params);

	WGPUPassTimestampWrites stamps[2] = {WGPU_PASS_TIMESTAMP_WRITES_INIT, WGPU_PASS_TIMESTAMP_WRITES_INIT};
	for (uint32_t i = 0; i < 2; ++i) {
		stamps[i].querySet = p.queries;
		stamps[i].beginningOfPassWriteIndex = i * 2;
		stamps[i].endOfPassWriteIndex = i * 2 + 1;
	}

	WGPUComputePassDescriptor compute_desc = WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
	if (p.queries) compute_desc.timestampWrites = &stamps[0];
	WGPUComputePassEncoder compute = wgpuCommandEncoderBeginComputePass(encoder, &compute_desc);
	wgpuComputePassEncoderSetPipeline(compute, p.compute);
	wgpuComputePassEncoderSetBindGroup(compute, 0, p.compute_group, 0, nullptr);
	wgpuComputePassEncoderDispatchWorkgroups(compute, (count + 63) / 64, 1, 1);
	wgpuComputePassEncoderEnd(compute);
	wgpuComputePassEncoderRelease(compute);

	WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
	color.view = view;
	color.loadOp = WGPULoadOp_Clear;
	color.storeOp = WGPUStoreOp_Store;
	color.clearValue = {51.0 / 255.0, 102.0 / 255.0, 153.0 / 255.0, 1.0};
	WGPURenderPassDescriptor draw_desc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
	draw_desc.colorAttachmentCount = 1;
	draw_desc.colorAttachments = &color;
	if (p.queries) draw_desc.timestampWrites = &stamps[1];
	WGPURenderPassEncoder draw = wgpuCommandEncoderBeginRenderPass(encoder, &draw_desc);
	wgpuRenderPassEncoderSetPipeline(draw, p.draw);
	wgpuRenderPassEncoderSetBindGroup(draw, 0, p.draw_group, 0, nullptr);
	wgpuRenderPassEncoderDraw(draw, 3, count, 0, 0);
#ifdef SPIKE_IMGUI
	if (with_imgui) ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), draw);
#else
	(void)with_imgui;
#endif
	wgpuRenderPassEncoderEnd(draw);
	wgpuRenderPassEncoderRelease(draw);

	if (!p.queries) return nullptr;
	for (auto& read : p.reads) {
		if (read.busy) continue;
		wgpuCommandEncoderResolveQuerySet(encoder, p.queries, 0, 4, p.query_resolve, 0);
		wgpuCommandEncoderCopyBufferToBuffer(encoder, p.query_resolve, 0, read.buffer, 0, 32);
		read.busy = true;
		return &read;
	}
	return nullptr;
}

void MapTimestamps(Pass& p, Pass::Read* read) {
	if (!read) return;
	WGPUBufferMapCallbackInfo callback = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
	callback.mode = WGPUCallbackMode_AllowProcessEvents;
	callback.userdata1 = &p;
	callback.userdata2 = read;
	callback.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* user, void* read_ptr) {
		auto& pass = *static_cast<Pass*>(user);
		auto& slot = *static_cast<Pass::Read*>(read_ptr);
		slot.busy = false;
		if (status != WGPUMapAsyncStatus_Success) return;
		uint64_t t[4];
		std::memcpy(t, wgpuBufferGetConstMappedRange(slot.buffer, 0, 32), 32);
		wgpuBufferUnmap(slot.buffer);
		if (t[1] < t[0] || t[3] < t[2]) {
			++pass.negative;
			return;
		}
		pass.compute_ms += double(t[1] - t[0]) / 1e6;
		pass.draw_ms += double(t[3] - t[2]) / 1e6;
		++pass.timed_frames;
	};
	wgpuBufferMapAsync(read->buffer, WGPUMapMode_Read, 0, 32, callback);
}

void Submit(Pass& p, WGPUCommandEncoder encoder) {
	WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, nullptr);
	wgpuQueueSubmit(p.queue, 1, &commands);
	wgpuCommandBufferRelease(commands);
	wgpuCommandEncoderRelease(encoder);
}

void PrintTimes(const Pass& p) {
	if (!p.queries) {
		std::printf("gpu: no timestamp-query\n");
	} else if (p.timed_frames) {
		std::printf("gpu: compute %.4f ms, draw %.4f ms (mean of %d frames), negative %d\n",
		            p.compute_ms / p.timed_frames, p.draw_ms / p.timed_frames, p.timed_frames, p.negative);
	} else {
		std::printf("gpu: timestamps never arrived, negative %d\n", p.negative);
	}
}

// 4x4 triangles in 256x256: every cell's center has that instance's color and every cell's corner the clear color.
int Offscreen(WGPUInstance instance, WGPUDevice device, const Options& options) {
	constexpr uint32_t kSize = 256, kCount = 16, kCell = kSize / 4;
	Pass p = MakePass(instance, device, WGPUTextureFormat_RGBA8Unorm, kCount);
	WGPUTextureDescriptor tex_desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
	tex_desc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
	tex_desc.size = {kSize, kSize, 1};
	tex_desc.format = WGPUTextureFormat_RGBA8Unorm;
	WGPUTexture texture = wgpuDeviceCreateTexture(device, &tex_desc);
	WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);
	WGPUBuffer readback = MakeBuffer(device, kSize * kSize * 4, WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst);

	for (int frame = 0; frame < 8; ++frame) {
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
		platform::PumpEvents(instance);
	}

	struct Wait {
		bool done = false, ok = false;
	} wait;
	WGPUBufferMapCallbackInfo callback = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
	callback.mode = WGPUCallbackMode_AllowProcessEvents;
	callback.userdata1 = &wait;
	callback.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* user, void*) {
		auto& w = *static_cast<Wait*>(user);
		w.done = true;
		w.ok = status == WGPUMapAsyncStatus_Success;
	};
	wgpuBufferMapAsync(readback, WGPUMapMode_Read, 0, kSize * kSize * 4, callback);
	while (!wait.done) platform::PumpEvents(instance);
	if (!wait.ok) {
		std::printf("readback failed\n");
		return 1;
	}
	for (int i = 0; i < 1000 && std::any_of(std::begin(p.reads), std::end(p.reads), [](auto& r) { return r.busy; }); ++i)
		platform::PumpEvents(instance);

	const auto* pixels = static_cast<const uint8_t*>(wgpuBufferGetConstMappedRange(readback, 0, kSize * kSize * 4));
	auto check = [&](uint32_t x, uint32_t y, int r, int g, int b) {
		const uint8_t* px = pixels + (y * kSize + x) * 4;
		const bool ok = std::abs(px[0] - r) <= 1 && std::abs(px[1] - g) <= 1 && std::abs(px[2] - b) <= 1;
		if (!ok) std::printf("pixel %u,%u is %d,%d,%d, expected %d,%d,%d\n", x, y, px[0], px[1], px[2], r, g, b);
		return ok;
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

int Windowed(WGPUInstance instance, WGPUDevice device, const Options& options) {
	WGPUSurface surface = platform::CreateSurface(instance);
	if (!surface) {
		std::printf("no surface\n");
		return 1;
	}
	WGPUSurfaceConfiguration config = WGPU_SURFACE_CONFIGURATION_INIT;
	config.device = device;
	config.format = WGPUTextureFormat_BGRA8Unorm;
	config.presentMode = options.immediate ? WGPUPresentMode_Immediate : WGPUPresentMode_Fifo;
	platform::WindowPixelSize(config.width, config.height);
	wgpuSurfaceConfigure(surface, &config);
	Pass p = MakePass(instance, device, config.format, options.instances);

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

	const auto start = Clock::now();
	uint32_t frame = 0, reconfigured = 0, skipped = 0;
	for (; options.frames == 0 || frame < options.frames; ++frame) {
		if (!platform::PollEvents(options.imgui)) break;
		uint32_t width = 0, height = 0;
		platform::WindowPixelSize(width, height);
		if (width != config.width || height != config.height) {
			config.width = width;
			config.height = height;
			++reconfigured;
			if (width && height) wgpuSurfaceConfigure(surface, &config);
		}
		WGPUSurfaceTexture current = WGPU_SURFACE_TEXTURE_INIT;
		wgpuSurfaceGetCurrentTexture(surface, &current);
		if (current.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
		    current.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
			++skipped;
			platform::PumpEvents(instance);
			continue;
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
	}
	const double total = Ms(Clock::now() - start);
	std::printf("window: %u frames in %.0f ms = %.3f ms/frame, %ux%u, reconfigured %u, skipped %u\n", frame, total,
	            total / std::max(frame, 1u), config.width, config.height, reconfigured, skipped);
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

Options Parse(int argc, char** argv) {
	Options o;
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		auto next = [&] { return i + 1 < argc ? argv[++i] : ""; };
		if (arg == "--list") o.list = true;
		else if (arg == "--offscreen") o.offscreen = true;
		else if (arg == "--imgui") o.imgui = true;
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
	WGPUInstance instance = platform::CreateInstance();
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
	const int result = options.offscreen ? Offscreen(instance, device, options) : Windowed(instance, device, options);
	wgpuDeviceRelease(device);
	if (!options.offscreen) platform::CloseWindow();
	return result;
}
