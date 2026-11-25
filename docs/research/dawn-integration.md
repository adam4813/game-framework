# Research: Dawn on desktop and web from one C++ source

**Question.** How is Dawn used natively and in the browser with this repo's toolchain: the vcpkg port, emdawnwebgpu,
window surfaces, software adapters for CI, WGSL tooling, imgui, and wgpu-native as the alternative?

**Date:** 2026-11-25. Everything below was read on that date. **Nothing was built or run**: sentences marked
*(unconfirmed)* are inferences or were not found in a file or page that was opened.

**Pinned versions:** vcpkg baseline `2f42d24513a18cc81fdbd99ccd74a3cc5dae93bb`: `dawn` 20261111.144539 (Dawn commit
`e327e2a60b08179ee8a04056f58dad8ca4b69b5f`), `imgui` 1.93.0, `sdl3` 3.4.20, `glfw3` 3.5.1, `directx-dxc` 2026-09-29.
emsdk **4.0.10** locally and in CI. The latest Emscripten release is 7.0.1 (2026-11-21) [em-cl].

Source keys:

| Key | Source |
|-----|--------|
| [port] | vcpkg `ports/dawn/` (`vcpkg.json`, `portfile.cmake`, `DawnConfig.cmake`, patches) at the baseline |
| [port-x] | vcpkg `ports/imgui`, `ports/sdl3`, `ports/glfw3`, `ports/directx-dxc`, `triplets/`, `scripts/buildsystems/msbuild/applocal.ps1` at the baseline |
| [vcpkg-df] | https://learn.microsoft.com/en-us/vcpkg/concepts/default-features |
| [dawn] | Dawn source at `e327e2a6` (`https://github.com/google/dawn/blob/e327e2a6…/<path>`); paths given inline |
| [rel] | https://github.com/google/dawn/releases/tag/v20261111.144539 (published 2026-11-12), its notes and asset contents |
| [rel-run] | GitHub Actions run 43164379260 in `google/dawn`, the run that built [rel] (job and step times from the Actions API) |
| [emd] | `emdawnwebgpu_pkg-v20261111.144539.zip` from [rel] (SHA512 equals the one in [port]): `README.md`, `emdawnwebgpu.port.py`, `webgpu/include/webgpu/webgpu.h`, `webgpu/src/library_webgpu.js`, `webgpu/src/webgpu.cpp` |
| [emd-old] | `emdawnwebgpu_pkg-v20250531.224602.zip`, the one emsdk 4.0.10 pins |
| [em] | Emscripten 4.0.10 in the local emsdk (`src/`, `tools/ports/emdawnwebgpu.py`) |
| [em-cl] | https://github.com/emscripten-core/emscripten/blob/main/ChangeLog.md, plus `tools/ports/emdawnwebgpu.py` at tags 5.0.6, 6.0.8 and 7.0.1 |
| [sdl] | SDL release-3.4.20: `include/SDL3/SDL_video.h`, `SDL_metal.h`, `docs/README-emscripten.md`, `src/video/emscripten/SDL_emscriptenvideo.c` |
| [imgui] | Dear ImGui v1.93.0: `backends/imgui_impl_wgpu.{h,cpp}`, `examples/example_sdl3_wgpu/CMakeLists.txt`, `.github/workflows/build.yml` |
| [img] | https://github.com/actions/runner-images: `README.md`, `Ubuntu2604-Readme.md`, `Windows2025-VS2026-Readme.md` (image versions 20261116.x) |
| [gh] | GitHub docs: "GitHub-hosted runners reference", "Dependency caching reference" |
| [warp] | https://learn.microsoft.com/en-us/windows/win32/direct3darticles/directx-warp |
| [ubu] | https://packages.ubuntu.com/resolute/amd64/mesa-vulkan-drivers/filelist (Ubuntu 26.04, 26.0.3-1ubuntu1) |
| [vkl] | Vulkan-Loader `docs/LoaderInterfaceArchitecture.md` (main) |
| [wgpu] | `gfx-rs/wgpu-native` README and releases; `gfx-rs/wgpu` README (trunk) |

---

## 1. PLAN.md's claims, checked

| Claim in PLAN.md | Verdict |
|---|---|
| The pinned `dawn` port serves native *and* emscripten | **Holds** [port] |
| It has a null device | **Holds.** The port sets `DAWN_ENABLE_NULL=ON` on native [port]. Not on web |
| imgui has `webgpu-binding` | **Holds**, and it depends on the same `dawn` port, on emscripten too [port-x] |
| "Local emsdk 4.0.10 is the minimum emdawnwebgpu documents" | **Half right, and misleading.** 4.0.10 is the minimum for the *remote* port file. The *local* package (what vcpkg installs) documents older Emscripten as possible but "may not work". The release was "Tested against emsdk release **5.0.6**" [emd README][rel]. The port *built into* emsdk 4.0.10 is the package of **2025-05-31** [em], 17 months older than the pinned native Dawn |
| Device acquired by the shell before `main`, start-up stays synchronous, no Asyncify | **Holds in the source**, with conditions (section 3) |
| DLL packaging in `build-games.yml` is a spike item | **Right to worry.** Dawn loads `dxcompiler.dll` at run time, so vcpkg's DLL copy step very likely misses it (section 2) |
| WARP and lavapipe for CI image tests | **Possible, but the selection route everyone reaches for does not work**: `forceFallbackAdapter` returns neither (section 5) |
| Google's release binaries as fallback | **Weak.** Static only, no DXC DLL, unsigned best effort; the Windows Release library is 624 MB (section 2) |
| Async pipeline creation for warm-up | **Holds** on web: `emwgpuDeviceCreateRenderPipelineAsync` exists in [emd] |
| One C++ source, only the surface differs | **Nearly.** Present, event pumping, blocking waits, and device start-up differ too (section 3) |

## 2. The vcpkg port

### Native

- **Linkage follows the triplet.** `VCPKG_LIBRARY_LINKAGE` static gives `DAWN_BUILD_MONOLITHIC_LIBRARY=STATIC`, otherwise
  `SHARED` [port]. `x64-windows` is dynamic, `x64-linux` is static [port-x]. So: `webgpu_dawn.dll` on Windows,
  `libwebgpu_dawn.a` on Linux. Consume it as `find_package(Dawn CONFIG)` + `dawn::webgpu_dawn` [port `usage`].
- **Default features** [port `vcpkg.json`]: Windows `d3d11`, `d3d12`, `vulkan`; Linux `gl`, `gles`, `vulkan`, `x11`;
  macOS `metal`. `wayland` and `tint-tools` are opt-in. The null backend is always on.
- **Trim them.** We need `d3d12` on Windows, `vulkan` + `x11` + `wayland` on Linux, `metal` on macOS. `d3d11` is documented
  as "not considered ready for production uses yet", Vulkan on Windows as best effort ([dawn] `docs/support.md`).
  `default-features: false` takes effect only when the project manifest lists `dawn` directly, not through imgui [vcpkg-df].
  `vulkan` on Windows is the only way to run a Vulkan driver there; it isn't needed for WARP.
- **Wayland mismatch.** The `sdl3` port defaults to X11 *and* Wayland on Linux [port-x]; `dawn` defaults to X11 only. On a
  Wayland session SDL3 may open a Wayland window that Dawn can't present to. Enable `dawn[wayland]` (or force SDL's X11
  driver).
- **Dependencies built or fetched:** abseil (not on emscripten), `directx-dxc` for `d3d12` (Microsoft's prebuilt DXC, not
  built from source), `vulkan-headers` + `vulkan-utility-libraries` for `vulkan`, `egl-registry`/`opengl-registry` for
  GL, plus SPIRV-Tools and SPIRV-Headers compiled inside Dawn [port]. Two dependencies are cloned from
  `chromium.googlesource.com` (jinja2, markupsafe) [port], so a cold build needs that host as well as GitHub.
- **C++ standard:** Dawn builds itself as C++20 ([dawn] `src/cmake/DawnCompilerChecks.cmake`); C++ modules are patched off
  [port `003`]. `webgpu.h` is a C header, so a C++23 consumer is not a conflict.
- **No GLFW helper:** `DAWN_USE_GLFW=OFF` [port]; `wgpu::glfw::CreateSurfaceForWindow` is not in the library.

### DLLs on Windows

Dawn's D3D12 backend uses DXC by default when built with it (`Platform::IsFeatureEnabled(kWebGPUUseDXC)`; [dawn]
`src/dawn/platform/DawnPlatform.cpp`), and the port sets `DAWN_USE_BUILT_DXC=ON` with `d3d12` [port]. It opens
`dxcompiler.dll` with `LoadLibraryEx`, not through an import ([dawn] `src/dawn/native/d3d12/PlatformFunctionsD3D12.cpp`).
**Without the DLL, device creation fails; there is no fall-back to FXC** ([dawn] `d3d12/DeviceD3D12.cpp`
`EnsureCompilerLibraries`, and the comment in `PhysicalDeviceD3D12.cpp`: "the creation would just fail if required compiler
is not available").

vcpkg's `applocal.ps1` copies what `dumpbin /DEPENDENTS` lists [port-x], which a run-time load isn't in. So:

| DLL | How it reaches the exe folder |
|---|---|
| `webgpu_dawn.dll`, abseil's DLL | vcpkg applocal *(unconfirmed: no build was run)* |
| `dxcompiler.dll` (22.3 MB), `dxil.dll` (2.0 MB) | **Copy explicitly** in CMake, from `directx-dxc` (`find_package(directx-dxc)`). Sizes are from the DXC release archive that port downloads. Whether Dawn needs `dxil.dll` at all is *unconfirmed* |
| `d3dcompiler_47.dll` | Only if the `use_dxc` toggle is off or the adapter's shader model is below 6.0 |

Search order for run-time DLLs: next to the Dawn module, next to the exe, then no path ([dawn] `Instance.cpp` 407-415).
`build-games.yml` already stages every `*.dll` beside the exe, so once CMake copies DXC there the workflow needs no change.
The ticket 05 spike must test from a clean folder.

Linux links statically and `dlopen`s `libvulkan.so.1` ([dawn] `vulkan/BackendVk.cpp`): no `.so` to stage, but players and CI
need a Vulkan loader.

### Emscripten

The port **does wrap emdawnwebgpu**: it downloads Google's `emdawnwebgpu_pkg` zip of the *same Dawn version*, installs its
headers, sources and `emdawnwebgpu.port.py`, and provides `dawn::webgpu_dawn` as an interface target that adds
`--use-port=<installed>/emdawnwebgpu.port.py` to compile and link [port]. Nothing is compiled at install time;
`webgpu.cpp` is compiled into Emscripten's cache at first link [emd port file]. The package is 134 KB zipped [rel].

| Route | Version we'd get | Verdict |
|---|---|---|
| vcpkg `dawn::webgpu_dawn` | 2026-11-11, same as native | **Use this.** One CMake target name on both platforms, one version to bump |
| `--use-port=emdawnwebgpu` on 4.0.10 | 2025-05-31 | **Avoid.** Older C API than native: e.g. `wgpuGetInstanceCapabilities` instead of `wgpuGetInstanceFeatures`/`Limits`, no immediates ([emd-old] vs [emd] headers) |
| Same on newer Emscripten | 5.0.6 pins 2025-10-02, 6.0.8 2026-04-23, 7.0.1 2026-11-05 [em-cl] | Drifts from native |

Two quirks. vcpkg's patch points the port's C include folder at the *whole* vcpkg `include/` directory [port `000`], and the
port file walks that folder at every `emcc` call and hashes it at link [emd port file]: it costs some time, and any
changed header rebuilds the tiny library. And 4.0.10 still ships an old `webgpu.h` in its own sysroot [em], so a
source that includes `<webgpu/webgpu.h>` without linking `dawn::webgpu_dawn` silently gets the wrong header.

### Build time and size

No vcpkg build time was found; ticket 05 measures it. Evidence for expectations:

- Google's CI build step for [rel] on GitHub's standard-labeled runners [rel-run]: Windows Release MSVC **79 min**, Windows
  Debug 79 min, Ubuntu Release GCC 121 min, macOS Intel Release 199 min, macOS arm64 Release 64 min. These build tests,
  samples, Tint tools and DXC from source, which the port does not. Its library-only mobile jobs took 18 to 37 min.
- vcpkg builds Debug *and* Release unless `VCPKG_BUILD_TYPE` is set, and no triplet we use sets it [port-x]. So expect
  roughly 40 to 80 min cold on a 4-core runner *(unconfirmed estimate)*, against about 10 min for all ports today
  (`setup-environment` comment: "~600 s").
- **Debug size is the cache risk.** Google's packaged Ubuntu Debug build is 845 MB compressed against 37 MB Release; Windows 325 MB
  against 119 MB; macOS Intel 261 MB against 15 MB [rel]. The repo's Actions cache limit is 10 GB, evicted by last access [gh],
  every changed set saves a new immutable entry (`save-vcpkg-cache`), and the runner has a 14 GB disk [gh]. A per-port
  `if (PORT STREQUAL "dawn") set(VCPKG_BUILD_TYPE release)` overlay triplet halves the build and should avoid this. Debug games then
  link Release Dawn: fine on Windows through a DLL's C API *(unconfirmed)*, and natural for a static C library on Linux.
  Dawn's own asserts and error injection are lost in Debug.

### Google's release binaries

Nightly assets, "best-effort … not signed or guaranteed by Google or the Dawn team" [rel]. Contents of the Release archives:

| Archive | Download | Main library | Notes |
|---|---|---|---|
| `windows-latest-Release` | 119 MB | `webgpu_dawn.lib` **624 MB static**, all objects `/MD` Release | 932 MB unpacked. **No DLLs at all**, although built with `DAWN_USE_BUILT_DXC=ON`, so `dxcompiler.dll` must come from elsewhere. Separate 325 MB Debug archive for `/MDd` |
| `ubuntu-latest-Release` | 37 MB | `libwebgpu_dawn.a` 44 MB | GCC on manylinux_2_28, so old-glibc compatible ([dawn] `.github/workflows/ci.yml`) |
| `macos-15-intel-Release` | 15 MB | `libwebgpu_dawn.a` 24 MB | Deployment target 13.0 |

All include `bin/tint` and a Dawn CMake package. They remain a usable plan B, and a quick way to get `tint`
without building it.

## 3. emdawnwebgpu

### Emscripten version

- Documented: remote port 4.0.10+; local package older "may not work" [emd README]. Tested with 5.0.6 only [rel].
- Reading the package against 4.0.10, every Emscripten feature it was seen to use exists there (`JS_LIBRARIES`, `LINK_AS_CXX`,
  `CLOSURE_ARGS`, `__i53abi`, `makeHEAPView`, `emscripten_has_asyncify`) [em]. That it compiles and links is *unconfirmed*.
- imgui 1.93.0 requires 4.0.10+ for its WebGPU backend, and its SDL3 example refuses Emscripten below **4.0.15** [imgui]. That
  check is about Emscripten's own SDL3 port; we'd use vcpkg's. SDL says to use the latest stable release [sdl].
- **Recommendation:** start the web spike on 4.0.10 with the vcpkg package. On any toolchain trouble, move to **5.0.6** (what
  Google tests) before debugging. Skip 6.0.2: it defaulted `GROWABLE_ARRAYBUFFERS` on, reverted in 6.0.3 because of "issues
  with Web API compatibility" [em-cl], and we use memory growth.

### How `webgpu.h` differs

Comparing [emd]'s header with the native one in [rel]: emdawnwebgpu declares 208 functions, native 280. **Everything in the web
header exists natively**; the native extras are all Dawn extensions (`wgpuDeviceTick`, `wgpuDeviceInjectError`,
`wgpuDeviceGetAdapter`, shared textures, toggles, validation hooks). Feature enums: 48 web against 120 native entries, again a
subset. So write against the web header's subset and the same code compiles on both. Behavior differs, though:

| Topic | Native Dawn | Web ([emd]) |
|---|---|---|
| Surface source | HWND, Xlib, Wayland, Metal layer | `WGPUEmscriptenSurfaceSourceCanvasHTMLSelector` only |
| `wgpuSurfacePresent` | required each frame | **aborts**; the browser presents after `requestAnimationFrame` returns |
| Present modes, formats | adapter dependent | Fifo only; `RGBA8Unorm`, `BGRA8Unorm`, `RGBA16Float`. No sRGB surface format: use `viewFormats` or encode in the shader |
| `wgpuInstanceWaitAny` with a timeout | needs `TimedWaitAny` requested | only with Asyncify or JSPI |
| Callbacks | pumped by `wgpuInstanceProcessEvents` | `AllowSpontaneous` fires from the JS event loop between frames; `AllowProcessEvents` is pumped, as native |
| Mapped buffers | real pointer | `GetMappedRange` is a `malloc`ed copy in the wasm heap, copied back on unmap |
| `webgpu_cpp.h` | same generator, more API | "**NOT** considered stable" [emd README]; both come from one Dawn commit through vcpkg |

### Device before `main`, no Asyncify

**Supported.** `emscripten_webgpu_get_device()` returns a `WGPUDevice` made from `Module["preinitializedWebGPUDevice"]`
([emd] `library_webgpu.js` 681-692, declared in `webgpu.h`). It is present in both the 2025 and the 2026 packages. Conditions:

1. **It is marked for removal** ("TODO(crbug.com/374150686): Remove this once it has been fully deprecated in users"). The public
   replacement is `WebGPU.importJsDevice(device, parentPtr)`, reachable from `EM_JS`/`EM_ASM` (Dawn's own
   `src/emdawnwebgpu/tests/SpotTests.cpp` calls its siblings that way).
2. **Pass the instance as the parent.** With no parent the device belongs to the "null instance" ([emd] `webgpu.cpp`
   `EventManager`), and `wgpuInstanceProcessEvents` never runs its `AllowProcessEvents` callbacks. So
   `emscripten_webgpu_get_device()` is safe only with `AllowSpontaneous` callbacks (buffer maps, async pipelines, queue
   done). `importJsDevice(dev, instancePtr)` fixes that: an instance has `EventSource` as first base, as the code requires.
3. **The device's error and lost callbacks are not wired** for an imported device: only `requestDevice` through C installs
   `onuncapturederror` and `device.lost` handlers ([emd] 897-966), and `GetLostFuture` is invalid on it. The shell must attach both
   in JS and forward them.
4. **The shell picks features and limits** in `requestDevice({requiredFeatures, requiredLimits})`; C++ reads what it got
   with `wgpuDeviceGetLimits`, `wgpuDeviceHasFeature` and `wgpuDeviceGetAdapterInfo`, all implemented. Keep that list in one place
   that native also uses. No adapter handle is needed (`wgpuSurfaceGetCapabilities` ignores its adapter argument).
5. `wgpuCreateInstance` is still called, for the surface.

**Delaying `main`:** a `--pre-js` file (as `tools/wasm_abort.js` is today) that calls `addRunDependency("webgpu-device")` in
`preRun`, requests adapter and device, sets `Module["preinitializedWebGPUDevice"]`, then `removeRunDependency`. In 4.0.10
`addRunDependency` is always present [em `src/preamble.js`]. Emscripten 4.0.14 made it optional (#24974): from then on add it to
`DEFAULT_LIBRARY_FUNCS_TO_INCLUDE` [em-cl]. A pre-js is compiled by Closure together with the rest, so names stay consistent;
keep `Module["…"]` quoted. With no adapter, show the message and don't remove the dependency.

Native start-up can stay synchronous as well: Dawn's `RequestAdapter` finds the adapter before it returns and only
the callback is deferred ([dawn] `Instance.cpp` 485-489), so `WaitAny` right after it completes.

### Exceptions and `ALLOW_MEMORY_GROWTH`

- The package compiles its `webgpu.cpp` with `-fno-exceptions` [emd port file]. No exception flag is set in our CMake (so
  Emscripten's default, catching disabled, applies) and nothing in emdawnwebgpu needs one. Rule either way: never let a C++
  exception leave a WebGPU callback.
- Memory growth is supported: Dawn links its own emdawnwebgpu tests with `-sALLOW_MEMORY_GROWTH` and `--closure=1` ([dawn]
  `src/emdawnwebgpu/CMakeLists.txt` 313-316), and the JS re-reads `HEAPU8` at each call. A pointer from
  `GetMappedRange` is a heap allocation, so it stays valid across growth.
- Our link sets a **128 MB maximum** heap (`cmake/GameTarget.cmake`). Mapped-buffer copies count against it. Prefer
  `wgpuQueueWriteBuffer` (passes a heap view, no copy on our side) and `wgpuBufferWriteMappedRange`/`ReadMappedRange` (no shadow copy).
- `--closure 1` is what the README asks for in release [emd README]; from 4.0.7 the port adds its externs by itself.

## 4. Surfaces

`wgpu::SurfaceDescriptor` chained structs ([dawn] `src/dawn/native/Surface.cpp`, `src/dawn/glfw/utils.cpp`), with SDL3 window
properties [sdl `SDL_video.h`, `SDL_metal.h`]:

| Platform | Chained struct | SDL3 | GLFW |
|---|---|---|---|
| Windows | `SurfaceSourceWindowsHWND` | `SDL_PROP_WINDOW_WIN32_HWND_POINTER`, `…_WIN32_INSTANCE_POINTER` | `glfwGetWin32Window`, `GetModuleHandle(nullptr)` |
| Linux X11 | `SurfaceSourceXlibWindow` | `SDL_PROP_WINDOW_X11_DISPLAY_POINTER`, `…_X11_WINDOW_NUMBER` | `glfwGetX11Display`, `glfwGetX11Window` |
| Linux Wayland | `SurfaceSourceWaylandSurface` (needs `dawn[wayland]`) | `SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER`, `…_WAYLAND_SURFACE_POINTER` | `glfwGetWaylandDisplay`, `glfwGetWaylandWindow` |
| macOS | `SurfaceSourceMetalLayer` | `SDL_Metal_CreateView` + `SDL_Metal_GetLayer` (no Objective-C of ours) | attach a `CAMetalLayer` to the `NSWindow` in an `.mm` file |
| Web | `EmscriptenSurfaceSourceCanvasHTMLSelector` | selector from `SDL_PROP_WINDOW_EMSCRIPTEN_CANVAS_ID_STRING` (default `#canvas`) | `"#canvas"`; GLFW itself isn't involved |

- Dawn's GLFW helper refuses a window unless `GLFW_CLIENT_API` is `GLFW_NO_API`; Emscripten's built-in GLFW 3 (`-sUSE_GLFW=3`)
  honors that hint and then makes no WebGL context [em `src/lib/libglfw.js` 1097]. With SDL3, don't pass `SDL_WINDOW_OPENGL`.
- A canvas that already has a WebGL context can't give a `webgpu` one; [emd]'s `wgpuInstanceCreateSurface` returns null then.
  The current shell's `getContext("webgl2")` check uses a throwaway canvas, so it is harmless.
- `ImGui_ImplWGPU_CreateWGPUSurfaceHelper` implements the native rows (Windows, X11, Wayland, Cocoa) in the imgui port
  [imgui], but tying the window code to imgui isn't worth a 40-line function.
- Native also needs a size from `SDL_GetWindowSizeInPixels`; on web [emd]'s `wgpuSurfaceConfigure` **writes the canvas
  `width`/`height`**, which SDL also does. Ticket 06 should check that they don't fight.

## 5. Software adapters and tests

### WARP (D3D12)

- Windows 8 and later list the Microsoft Basic Render adapter in DXGI enumeration, so WARP is present without a GPU [warp];
  `windows-latest` is Windows Server 2025 build 26100 [img]. No install step should be needed *(unconfirmed on the runner)*.
- **`forceFallbackAdapter = true` returns an empty list on D3D** ([dawn] `d3d/BackendD3D.cpp` 139: `return {}`), and on Null.
- What works: `backendType = D3D12` and pick the adapter whose info is `vendorID 0x1414`, `deviceID 0x8C` ([dawn]
  `gpu_info.json`) or `AdapterType::CPU` (set from `DXGI_ADAPTER_FLAG_SOFTWARE`, `d3d/PhysicalDeviceD3D.cpp` 100).
  On a runner without a GPU it is the only D3D12 adapter. On a developer machine, enumerate with
  `dawn::native::Instance::EnumerateAdapters` (native C++ API) or chain `RequestAdapterOptionsLUID`.
- Dawn enables timestamp queries wherever a D3D12 queue reports a timestamp frequency, which WARP should do
  *(unconfirmed)*. Dawn's tests note "Flaky negative timestamps on … Windows WARP" (crbug.com/dawn/1852,
  `tests/end2end/QueryTests.cpp` 1450), so don't assert on timings. DXC is needed as on hardware (section 2).
- Known WARP special cases in Dawn at this commit: root-signature 1.1 check skipped, some subgroup-matrix shapes filtered
  ([dawn] `d3d12/PhysicalDeviceD3D12.cpp` 749, 1210), and tests with 2 GB or larger buffers are skipped (`tests/end2end/CopyTests.cpp`).
  A text search of 14 end-to-end test files (MSAA, depth, queries, storage textures, copies)
  found no other skip for WARP and none for lavapipe. Dawn's full expectations were not read.
- A newer WARP can be tried from the `Microsoft.Direct3D.WARP` NuGet package by placing its DLL next to the exe; it may not
  be redistributed [warp].

### lavapipe (Vulkan)

- **`ubuntu-latest` is now Ubuntu 26.04** [img]. Its image document lists no Vulkan or Mesa package [img]; install
  `mesa-vulkan-drivers libvulkan1` with apt. On 26.04 the first contains `libvulkan_lvp.so` and
  `/usr/share/vulkan/icd.d/lvp_icd.json` [ubu].
- **`forceFallbackAdapter` accepts only SwiftShader** ([dawn] `vulkan/BackendVk.cpp` 534-541), which the vcpkg port does not build.
- What works: `backendType = Vulkan` and `VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json` (the loader ignores every other
  driver; needs a loader from 1.3.207 on, `VK_ICD_FILENAMES` is the deprecated name) [vkl]. Check that the adapter info
  says vendor `0x10005`, device 0 ("Mesa", "Software") ([dawn] `gpu_info.json`) or `AdapterType::CPU`.
- Dawn requires Vulkan 1.1 plus a short feature list ([dawn] `docs/support.md`). That lavapipe 26.0 meets it and that our
  passes draw correctly there is *unconfirmed*; Dawn's own test harness has a predicate for it (`IsMesaSoftware`).
- Offscreen needs no window, so no `xvfb`.

Name the backend in tests. Without it Dawn lists every built backend and sorts by adapter type (CPU after GPUs), then backend
(D3D12 and Metal, Vulkan, D3D11, GL, Null last) ([dawn] `Adapter.cpp` `SortAdapters`), so a missing driver quietly falls through
to GL or Null instead of failing the test.

### Null backend

`backendType = Null` always yields one adapter, needs no driver, and claims **every** feature with default limits ([dawn]
`null/DeviceNull.cpp`). Front-end validation, Tint WGSL parsing and compute-pipeline IR checks run; buffer copies and writes
work; render and compute commands are **no-ops**, submits complete at once. Good for: resource lifetime, bind-group and pipeline
validation, upload ring logic, render-graph wiring, and "every WGSL file compiles", in unit tests without a GPU. Bad for: pixels, and
catching missing features or limits (request devices with the same explicit feature and limit list as the game).

## 6. WGSL tooling

- **`dawn[tint-tools]`** builds and installs the `tint` executable (Windows, Linux, macOS; pulls in `glslang`) [port]. It's
  also in Google's archives. Useful options ([dawn] `src/tint/cmd/tint/main.cc`): `--parse-only`, `--format`,
  `--validate`, `--diagnostics-format json`, `--minify`.
- **Cheaper in our tests:** the Null backend plus `wgpuDeviceCreateShaderModule` and `GetCompilationInfo` validates with the exact
  Tint we ship, with no extra tool to build and after our own `#include` step.
- **Locations:** `WGPUCompilationMessage` carries type, `lineNum` (1-based), `linePos` (1-based, in bytes natively), offset and
  length ([dawn] `CompilationMessages.cpp`). The uncaptured error text also holds the formatted source excerpt. On web `linePos`
  counts UTF-16 units ([emd], TODO crbug.com/435488557) and arrives asynchronously; identical for ASCII sources.
- WGSL has no `#line`, so with our `#include` expansion the engine must keep a (line → file, line) table and translate. Set each
  module's label to the file name.
- Tint is not the only compiler on web: Firefox and Safari use their own. Offline validation covers Chrome's.

## 7. imgui

`imgui[webgpu-binding]` in the pinned port compiles `imgui_impl_wgpu.cpp` with `IMGUI_IMPL_WEBGPU_BACKEND_DAWN` and links
`dawn::webgpu_dawn` [port-x]. It pins **no Dawn revision**: whatever `dawn` the baseline has. Every `wgpu*` function it calls exists
in both the pinned native header and the pinned emdawnwebgpu header (compared by name; compiling is *unconfirmed*). Its changelog's last
Dawn update is 2025-10-16, and the last wgpu-native one 2026-08-31 [imgui]. On emscripten the define selects the emdawnwebgpu
path. `sdl3-binding` is independent. The backend takes a device, a target format and a render pass encoder, so it draws
inside our last pass. It writes colors unchanged to non-sRGB
targets and applies a 2.2 gamma uniform for sRGB formats.

## 8. wgpu-native as the alternative

| | Dawn | wgpu-native |
|---|---|---|
| Web from C++ | emdawnwebgpu, same commit | None of its own: the README covers native platforms, and wgpu's web target is Rust compiled to wasm [wgpu]. Web would still be emdawnwebgpu, so two implementations of `webgpu.h` |
| vcpkg | `dawn` port | No `wgpu` or `wgpu-native` port in the pinned baseline |
| Release cadence | nightly | v29.0.1.1 on 2026-06-23 was the latest [wgpu] |
| Binaries | static; 15 to 119 MB downloads | 13 to 17 MB Release zips for Windows, Linux, macOS x86_64 and arm64 [wgpu] |
| Shader compiler | Tint, as in Chrome | Naga, which "is working on catching up to the WGSL specification" [wgpu] |
| Null device, imgui | yes; yes | imgui yes [imgui] |

**No reason to prefer it.** Its one advantage is a small, prebuilt library with no build time; keep it in mind only if
Dawn's build cost proves unacceptable *and* Google's archives fail.

**Intel macOS** (best effort): Dawn supports macOS 13.0+ with Metal 2.3+ ([dawn] `docs/support.md`), Google's CI builds
`macos-15-intel` nightly [rel], and the port has a `metal` feature [port]. The README's Intel Mac is on macOS 13.2 with
MacPorts LLVM 20, a compiler Dawn's CI doesn't use: building there is *unconfirmed*. Google's archive is the fallback, but it is
built against Apple's libc++ *(mixing it with MacPorts' libc++ is unconfirmed)*. wgpu-native also ships `macos-x86_64`.

## 9. Recommendation

1. vcpkg `dawn`, listed directly in `vcpkg.json` with `default-features: false` and per-platform features; the same port on web.
   Don't use `--use-port=emdawnwebgpu`.
2. An overlay triplet that builds `dawn` Release-only, if the spike's Debug sizes are as bad as Google's.
3. CMake copies `dxcompiler.dll` (and `dxil.dll`) next to the exe.
4. Write to the emdawnwebgpu header subset, C API or `webgpu_cpp.h`; `AllowProcessEvents` callbacks pumped once per
   Frame on both platforms (needs the instance parent, section 3); no `WaitAny` with a timeout on web. Small per-platform file: surface, present, device start-up, event pump.
5. Web start-up: pre-js + run dependency + `importJsDevice(device, instance)`; JS forwards lost and uncaptured errors.
6. CI tests name a backend and assert the adapter's vendor and device ID. Null backend for unit tests.
7. Pin `ubuntu-26.04`/`windows-2025` for image tests, so a moving `-latest` doesn't change the rasterizer under the golden images.

## 10. Not verified

- Anything that needs a build or a run: the port's build time and installed size on our machines, emdawnwebgpu on 4.0.10, wasm size,
  imgui compile, DLL copying, WARP and lavapipe on hosted runners, Debug linking against a Release Dawn.
- Whether a native `RequestAdapterOptionsLUID` needs the `allow_experimental_apis`/unsafe toggle.
- Whether lavapipe exposes every limit the renderer will request; whether Mesa on 24.04 uses the same ICD file name.
- Whether SDL3's canvas sizing conflicts with `wgpuSurfaceConfigure`.
- When `emscripten_webgpu_get_device` will actually be removed.
- The port's build under `clang` on Linux or MacPorts LLVM 20; Google's CI uses MSVC, GCC and Apple Clang.
