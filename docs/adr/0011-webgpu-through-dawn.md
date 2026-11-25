# One WebGPU Renderer: Dawn on Desktop, the Browser's WebGPU on the Web

The renderer behind the seam of ADR 0010 is written once, against the `webgpu.h` C API and in WGSL. On desktop, Dawn
runs it on D3D12 (Windows), Vulkan (Linux) and Metal (macOS); on the web, the browser's own WebGPU runs it. The web
build becomes WebGPU only, as ADR 0009 made it WebGL 2 only. raylib stays the default until the new renderer reaches
parity and is swapped in.

Dynamic shadows, HDR with MSAA, and later software-traced global illumination are the aim, on desktop and web. The last
needs compute shaders and storage buffers on both, which OpenGL 3.3 and WebGL 2 don't have (ADR 0009), and WebGPU is the
only API that gives them to a browser.

This supersedes ADR 0009's GPU half at the swap: cluster lists as RGBA32F textures, the WebGL 2 requirement and the
raylib triplet. Its other half stays: clustered forward shading, the light `range`, the budgets and the CPU cluster
build, which feeds storage buffers instead. Moving that build to compute is a later choice.

**Dawn comes from vcpkg**, with one backend per system, Release-only (a Debug executable uses the Release Dawn), and the
same port provides the web's bindings. So the engine uses the C API alone: Dawn's C++ API passes standard-library types
and crashes across configurations. Windows ships D3D12 only; nothing outside the Dawn feature list and device start-up
may assume it, so that Vulkan on Windows can be added later.

Design to WebGPU's core default limits (4 bind groups, 8 storage buffers per stage): a device gets those unless more are
requested, every core adapter has them, and Chrome never raises the bind group count. Compatibility mode isn't used: it
drops RGBA16F MSAA and vertex-stage storage buffers.

Measured by a throwaway spike, 2026-11-25 (SDL3, Dawn 20261111, one compute pass, one instanced triangle, imgui):

| | Result |
|---|---|
| Windows, RTX 3080 Ti, D3D12, 10,000 instances at 1280x720 | 0.25 ms per Frame |
| Chrome, same machine | Same GPU code; vsync-locked, 0.3 ms CPU per Frame |
| Pixels | Same image hash on D3D12, Vulkan, D3D11, WARP, lavapipe, Chrome on the GPU and on SwiftShader |
| Hosted CI runners (lavapipe, WARP) | 240 of 240 offscreen runs drew that hash |
| Dawn, cold, trimmed and Release-only | 3.1 min on a 32-thread desktop; 15 to 16 min per CI leg |
| Warm CI leg | about 1 to 2 min |
| vcpkg cache, new ports less raylib | +39 MB Linux, +62 MB Windows, +2 MB web |
| Web, wasm without imgui / with | 0.78 / 1.69 MB |
| Windows, files shipped beside the game | `dxcompiler.dll` 22.3 MB, Dawn 6.5, SDL3 2.5, abseil 2.0 |

Not measured: many draw calls, MSAA, filtering, depth, other GPU vendors, Safari, Firefox, phones, macOS. The spike's
image was trivial, so the hash says nothing about tolerances for real scenes.

Rejected:

- **OpenGL 3.3 and WebGL 2 as they are.** Shadow maps fit, dynamic global illumination doesn't.
- **Vulkan on desktop and WebGL 2 on the web.** Two shader languages, two copies of each pass, and the web never gets
  compute.
- **SDL_GPU** (no web backend), **bgfx** and **sokol** (another layer and shader toolchain over the same APIs).
- **wgpu-native.** Not in vcpkg, and no C++ route to the web.
- **Google's Dawn release binaries.** Unsigned nightlies, static only (a 624 MB Windows library), no DXC. Dawn from
  vcpkg isn't slow enough to need them.

Consequences:

- **Reach shrinks.** Games in the browser lose Firefox on Linux, Android and Intel Macs, Safari on macOS 15 and older,
  Chrome on Linux outside its allow-list (no AMD), and older or unlisted Android GPUs. The page shows a message, as it
  does for WebGL 2 today.
- **Best effort**: Safari, Firefox, phones and macOS Intel are supported where they work and don't hold the swap.
- **Cold CI is slower.** After a vcpkg bump or a new runner compiler, each leg builds Dawn itself, up to eight at once.
- **Reopening.** Before the swap, the decision is reopened if the new renderer misses the parity bar (frame times
  included) on Windows or in Chrome, or stops drawing in Linux CI. No fallback is named; the seam of ADR 0010 stays
  either way.
