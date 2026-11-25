#pragma once
// Throwaway spike (render pipeline map, tickets 05 and 06): the passes, GPU
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

namespace {  // included once, by spike.cpp

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

}  // namespace
