#pragma once
// One compute pass writes per-instance data; the vertex stage reads it as a read-only storage buffer.
inline constexpr const char* kComputeWgsl = R"(
struct Instance { offset: vec2f, scale: f32, hue: f32 };
struct Params { count: u32, side: u32, time: f32, pad: f32 };
@group(0) @binding(0) var<uniform> params: Params;
@group(0) @binding(1) var<storage, read_write> instances: array<Instance>;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) id: vec3u) {
	if (id.x >= params.count) { return; }
	let cell = vec2f(f32(id.x % params.side), f32(id.x / params.side));
	let size = 2.0 / f32(params.side);
	var out: Instance;
	out.offset = vec2f(-1.0 + size * (cell.x + 0.5), 1.0 - size * (cell.y + 0.5));
	out.scale = size * 0.45 * (1.0 - 0.3 * abs(sin(params.time + f32(id.x))) * step(0.0001, params.time));
	out.hue = f32(id.x % 16u);
	instances[id.x] = out;
}
)";

inline constexpr const char* kDrawWgsl = R"(
struct Instance { offset: vec2f, scale: f32, hue: f32 };
@group(0) @binding(0) var<storage, read> instances: array<Instance>;
struct VsOut { @builtin(position) position: vec4f, @location(0) @interpolate(flat) color: vec3f };

@vertex
fn vs(@builtin(vertex_index) v: u32, @builtin(instance_index) i: u32) -> VsOut {
	// A triangle that covers its cell's center and stays off its corners.
	var corners = array<vec2f, 3>(vec2f(0.0, 1.0), vec2f(-1.0, -1.0), vec2f(1.0, -1.0));
	let instance = instances[i];
	var out: VsOut;
	out.position = vec4f(instance.offset + corners[v] * instance.scale, 0.0, 1.0);
	// Exactly representable in RGBA8: multiples of 17/255 per channel.
	out.color = vec3f(instance.hue, 15.0 - instance.hue, 8.0) / 15.0;
	return out;
}

@fragment
fn fs(in: VsOut) -> @location(0) vec4f { return vec4f(in.color, 1.0); }
)";
