# Research: WebGPU in browsers (support, limits, mobile)

Ticket: "WebGPU in browsers: support, limits, mobile" (render pipeline, wayfinder research).
Question: which browsers ship WebGPU on by default, with which limits and features the render pipeline plan depends on,
what compatibility mode changes, what fails today, and how the page shell should detect support.
Out of scope: how Dawn, emdawnwebgpu and CI are set up (their own tickets), and running anything. No browser was
started for this note.

Date: 2026-11-25. Every page below was opened on that date; a second date is the page's own.
Current stable versions: Chrome 159 on Windows and Android [cr-dash], Firefox 161 [fx-ver], Safari 27.2 (27.3 in beta)
[saf-list].

## Sources

| Ref | Source |
|-----|--------|
| [spec] | WebGPU Editor's Draft, 21 November 2026 (`gpuweb@ca61f22`), https://gpuweb.github.io/gpuweb/ |
| [tr] | WebGPU W3C Candidate Recommendation Draft, 16 November 2026, https://www.w3.org/TR/webgpu/ |
| [wiki] | gpuweb Implementation Status, last edited 2 October 2026 (`6e41fe1`), https://github.com/gpuweb/gpuweb/wiki/Implementation-Status |
| [compat] | gpuweb `proposals/compatibility-mode.md` @ main, https://github.com/gpuweb/gpuweb/blob/main/proposals/compatibility-mode.md |
| [corr] | WebGPU Correspondence Reference, 21 November 2026, https://gpuweb.github.io/gpuweb/correspondence/ |
| [c119]…[c155] | Chrome "What's New in WebGPU" posts, `https://developer.chrome.com/blog/new-in-webgpu-<n>`; dated where cited |
| [cr-tips] | Chrome WebGPU troubleshooting tips, updated 17 October 2025, https://developer.chrome.com/docs/web-platform/webgpu/troubleshooting-tips |
| [cr-src] | Chromium main: `gpu/config/webgpu_blocklist_impl.cc`, `gpu/config/software_rendering_list.json` (entry 186), `gpu/config/gpu_util.cc`, `gpu/config/gpu_finch_features.cc`, `gpu/command_buffer/service/webgpu_decoder_impl.cc` |
| [cr-dash] | `chromiumdash.appspot.com/fetch_releases?channel=Stable` |
| [dawn] | Dawn main @ `3ea30c2` (25 November 2026): `src/dawn/native/Limits.cpp`, `d3d12/PhysicalDeviceD3D12.cpp`, `vulkan/PhysicalDeviceVk.cpp`, `metal/PhysicalDeviceMTL.mm` |
| [fx-src] | Firefox `release` branch @ `fb8002f` (161.0.1): `modules/libpref/init/StaticPrefList.yaml`, `dom/webgpu/Adapter.cpp` |
| [fx-main] | Firefox main (163): `dom/webgpu/Instance.cpp`, `gfx/thebes/gfxPlatform.cpp`, `widget/gtk/GfxInfo.cpp`, `widget/android/GfxInfo.cpp` |
| [fx-ver] | https://product-details.mozilla.org/1.0/firefox_versions.json |
| [fx-141] [fx-145] [fx-147] | `https://www.firefox.com/en-US/firefox/<n>.0/releasenotes/` |
| [bz] | Bugzilla REST: bugs 2006676 (Linux), 2077114 and 2009871 (Android), 2004105 (Intel Mac) |
| [wk-src] | WebKit main @ `2392246` (25 November 2026) and branch `safari-7625.2.8.18-branch` (22 October 2026): `Source/WTF/wtf/PlatformEnable.h`, `Source/WTF/Scripts/Preferences/UnifiedWebPreferences.yaml` (main only), `Source/WebGPU/WebGPU/HardwareCapabilities.mm` |
| [saf-26] [saf-27] | Apple Safari 26.0 and 27 release notes, https://developer.apple.com/documentation/safari-release-notes |
| [saf-list] | Apple release-notes index, same address |
| [wk-26] | WebKit blog, "WebKit Features in Safari 26.0", https://webkit.org/blog/17333/webkit-features-in-safari-26-0/ |
| [webdev] | web.dev, 25 November 2025, https://web.dev/blog/webgpu-supported-major-browsers |
| [bcd] | MDN browser-compat-data @ main: `api/GPU.json`, `GPUSupportedFeatures.json`, `GPUSupportedLimits.json` |
| [survey] | https://web3dsurvey.com/webgpu and its feature/limit pages. **Not primary**: visitors' reports, no date window or sample size given. Used only for "what real adapters report" |

## 1. Who ships it, on by default

| | Windows | macOS | Linux | Android | iOS / iPadOS |
|---|---|---|---|---|---|
| **Chrome** | Yes, 113+ [wiki] | Yes, 113+ [wiki] | **Partly**: Intel Gen12+ from 144, NVIDIA (driver 535.183.01+, Wayland) from 147 [wiki, c144, c147-148] | Yes on Android 12+ with ARM, Qualcomm or Intel GPUs (121+); Imagination on Android 16+ (139+) [wiki] | As Safari (unconfirmed) |
| **Edge** | Yes, 113+ [webdev] | Yes, 113+ [webdev] | As Chrome [wiki groups it under Chromium] | As Chrome [wiki groups it] | As Safari (unconfirmed) |
| **Firefox** | Yes, 141+ [fx-141] | Apple silicon only: macOS 26 from 145 [fx-145], all versions from 147 [fx-147] | **No** (Nightly) | **No** (blocked) | As Safari (unconfirmed) |
| **Safari** | none | 26+ **on macOS 26+** | none | none | 26+ [saf-26, wk-26] |

iOS browsers were not checked separately. [bcd] marks `webview_ios` as following Safari; whether Chrome on iOS has
`navigator.gpu` was not opened.

Details behind the table:

- **Chrome on Linux** is gated by an allow-list inside the blocklist: Intel generation ≥ 12 with Mesa ≥ 22.0, or NVIDIA
  driver ≥ 535.183.01. Everything else on Linux, **all AMD GPUs included**, is off [cr-src, entry 186]. The path is also
  off whenever Chrome isn't on its OpenGL context or runs SwiftShader [cr-src, `GetWebGPUOnVulkanViaGLInterop`], unless
  the rest of Chrome runs on Vulkan (`webgpu_decoder_impl.cc`). Other systems need
  `--enable-unsafe-webgpu --ozone-platform=x11 --use-angle=vulkan --enable-features=Vulkan,VulkanFromANGLE` [wiki]. The
  "Wayland" condition for NVIDIA is in [wiki] and [c147-148], not in entry 186.
- **Chrome refuses software adapters.** `AdapterType::CPU` is blocklisted, and SwiftShader is returned only with
  `--enable-unsafe-webgpu`; `forceFallbackAdapter: true` returns null without it [cr-src]. A browser on a GPU-less CI
  runner needs that flag. (WARP and lavapipe through *native* Dawn don't go through this code.)
- **Chrome on Windows ARM64**: [wiki] says behind a flag. [cr-src] main now allows Snapdragon 8380 with a driver
  ≥ 31.0.117 and blocks the older parts. In which version that ships is unconfirmed.
- **Chrome on Windows otherwise** accepts NVIDIA, AMD, Intel and Microsoft adapters over D3D12 and blocks other vendors
  and 32-bit (x86) builds on NVIDIA or Shader Model 6 [cr-src]. D3D11 is blocked.
- **Chrome on Android** additionally has `kAAPMBlocksWebGPU`: Android's Advanced Protection Mode turns WebGPU off, on by
  default in the source [cr-src]; not tested. Samsung Xclipse is "TBD" [wiki].
- **Firefox on Linux**: `dom.webgpu.enabled` is true only for Windows and Apple-silicon Mac. Elsewhere it is
  early-beta-or-earlier in 161 and Nightly-only in 162 and 163 [fx-src, fx-main]. Mesa older than 25.0.4 is blocklisted
  even then [fx-main]. The meta bug is still NEW, with no target release [bz 2006676, last changed 2026-11-25]. [wiki]
  says "expects to ship on Linux in 2026".
- **Firefox on Android**: blocklisted by default [fx-main]. "Enable on Nightly" (2009871) and "Release on Android"
  (2077114) are both NEW [bz]. [wiki] says "expects to do work on Android in 2026".
- **Firefox on Intel Macs**: off; bug 2004105 NEW [bz]. Firefox also needs a 64-bit build and, on Windows, macOS and
  Android, a working GPU process [fx-main].
- **Safari is tied to the OS, not only to the Safari version.** `ENABLE_WEBGPU_BY_DEFAULT` needs macOS ≥ 26, or iOS or
  visionOS [wk-src, main and 7625.2]. Safari 27 also ships for macOS Sequoia [saf-27], so **Safari 27 on macOS 15 has
  WebGPU off** (read from source, not run). Lockdown Mode turns it off (`disableInLockdownMode: true`) [wk-src]. In
  WebKit, Intel Macs with a Mac2-family GPU are covered [wk-src, `mac2()`], so Safari on macOS 26 is a possible route
  there; not run.
- **Chrome on Intel Macs** has no exclusion in [wiki] or [cr-src]. Dawn needs macOS 13+ as of Chrome 153-154 [c153-154,
  15 September 2026].

How much of the web this is, per [survey] (share of reports with a working adapter; undated, self-selected): Windows 88%,
macOS 91%, iOS 90%, Android 70%, ChromeOS 93%, **Linux 18%**; 83% overall. By browser: Edge 93%, Chrome 84%, Safari 84%,
Firefox 59%.

## 2. Limits

A device gets the **defaults** below unless the page asks for more in `requiredLimits`. A large `adapter.limits` value
changes nothing on its own [spec §3.6.2; c133].

| Limit | Core default | Compat default | Chrome/Edge raise to (tiers) | Safari source (Apple6+/Mac2) | Reports [survey] |
|---|---|---|---|---|---|
| `maxBindGroups` | 4 | 4 | **4 (no higher tier)** | 11 | 22% ≥ 7 |
| `maxStorageBuffersPerShaderStage` | 8 | 8 | 10, 16 | 44 | ≥ 10: 98%. ≥ 16: 80% |
| `maxStorageBuffersInVertexStage` | 8 | **0** | 10, 16 | 44 | ≥ 10: 98% |
| `maxStorageBuffersInFragmentStage` | 8 | **4** | 10, 16 | 44 | ≥ 10: 98% |
| `maxStorageTexturesPerShaderStage` | 4 | 4 | 8 | 44 | ≥ 8: 94% |
| `maxSampledTexturesPerShaderStage` | 16 | 16 | 16, 48 | 44 | ≥ 44: 92% |
| `maxSamplersPerShaderStage` | 16 | 16 | 16 | 22 | 20% ≥ 22 |
| `maxUniformBuffersPerShaderStage` | 12 | 12 | 12 | 44 | 20% ≥ 14 |
| `maxUniformBufferBindingSize` | 64 KiB | **16 KiB** | **64 KiB (no higher tier)** | `maxBufferSize` | 18% ≥ 128 MiB |
| `maxStorageBufferBindingSize` | 128 MiB | 128 MiB | 256 and 512 MiB, 1 GiB, 2 GiB − 4, 4 GiB − 4 | `maxBufferSize` | ≥ 256 MiB: 97% |
| `maxBufferSize` | 256 MiB | 256 MiB | 1, 2, 4 GiB − 4 | macOS: at least 1 GiB where the GPU allows. iOS: ⅓ of Metal's maximum, capped at 1 GiB | ≥ 512 MiB: 100% |
| `maxTextureDimension2D` | 8192 | **4096** | 16384 | 16384 | ≥ 16383: 99% |
| `maxTextureArrayLayers` | 256 | 256 | 2048 | 2048 | ≥ 1024: 99% |
| `maxComputeWorkgroupStorageSize` | 16 KiB | 16 KiB | 32, 48, 64 KiB | 32 KiB | ≥ 32 KiB: 99% |
| `maxComputeInvocationsPerWorkgroup` | 256 | **128** | 1024 | 1024 | ≥ 768: 93% |
| `maxColorAttachmentBytesPerSample` | 32 | 32 | 64, 128 | 64 | ≥ 64: 99% |
| `maxInterStageShaderVariables` | 16 | 15 | 28 | 31 | ≥ 27: 90% |
| `min{Uniform,Storage}BufferOffsetAlignment` | 256 | 256 | 256 | 32 (iOS), 256 (Mac2) | 256 works everywhere |

Default columns: [spec §3.6.2]. Chrome columns: [dawn] `Limits.cpp`, whose tiers apply to whole groups of limits, so one
low value drops the rest of its group. Safari column: [wk-src]. Apple4 and Apple5 GPUs get the plain defaults there.

Notes that matter to the design:

- **Four bind groups is the ceiling in Chrome and Edge, not just the guarantee.** The plan's fixed 0–3 layout uses all of
  them. `maxBindGroupsPlusVertexBuffers` is 24 [spec].
- **8 storage buffers per stage is the only safe number.** In a core device the vertex and fragment limits equal the
  per-stage limit [spec, "a new device"]. They count across all four groups of the pipeline layout. Chrome exposes more
  since 120 (10) and 146 (16) [c120; c146, 25 February 2026], but 20% of reports stop below 16, and macOS reports fall to
  27% at ≥ 16 [survey]. Safari's reports list only ≥ 8 and ≥ 9 [survey], although its source says 44; don't rely on more
  than 8.
- The per-stage `maxStorageBuffersInVertexStage` family appeared in Chrome 146, Firefox 151 (correct from 159) and
  Safari 27 [bcd; saf-27 "Restored `maxStorageBuffersInFragmentStage`"]. **Don't request those names**: they are
  undefined in older Safari and raising `maxStorageBuffersPerShaderStage` raises them in core.
- Android: 27% of reports have a storage binding below 256 MiB, 60% below 2 GiB [survey].

## 3. Features

| Feature | Guaranteed? | Chrome/Edge | Firefox | Safari | Reports [survey] |
|---|---|---|---|---|---|
| `timestamp-query` | No | D3D12 and Metal where supported; Vulkan needs `timestampComputeAndGraphics` [dawn]; since 121 [c121] | In 161 source [fx-src] | If the GPU supports stage-boundary counters; never on Intel Mac GPUs [wk-src] | 98.9% (Android 97.9, iOS 100, macOS 96.3) |
| `float32-filterable` | No | D3D12 always; Vulkan and Metal by GPU [dawn] | In source [fx-src] | By GPU (`supports32BitFloatFiltering`) [wk-src] | 91.3% (**Android 71, iOS 60**, Safari 64, Qualcomm 48) |
| `rg11b10ufloat-renderable` | No | D3D12 and Metal always; Vulkan by format support [dawn] | In source [fx-src] | Always [wk-src] | 99.96% |
| `depth-clip-control` | No | D3D12 and Metal always; Vulkan needs `depthClamp` [dawn] | In source [fx-src] | Always [wk-src] | 98.1% (Android 90.7, **ARM Bifrost 0%**, ChromeOS 84) |

`rg11b10ufloat-renderable` covers rendering, blending, multisampling and resolve [spec §25.12]; the format has no alpha
channel. `depth-clip-control` gives `primitive.unclippedDepth` [spec §25.2].

- **RGBA16F with 4x MSAA, resolve and blending: yes, guaranteed in core.** The format table marks `rgba16float`
  renderable and blendable unconditionally, and "multisampling" plus "resolve" as "If `core-features-and-limits` is
  enabled" [tr and spec §26.1.1, same row]. Every core device has that feature. It's also filterable and usable as a
  write-only or read-only storage texture. It costs 8 bytes of the 32-byte color attachment budget [spec].
- **Depth textures**: `depth32float`, `depth24plus` and `depth16unorm` all support multisampling, and depth can be
  compared with filtering through a `comparison` sampler but never through a `filtering` sampler. `depth32float` and
  `depth24plus` can't be a copy destination [spec §26.1.2].
- **Timestamps are coarse.** The spec coarsens every timestamp, never with the cross-origin-isolated resolution
  [spec §20.4]. Chrome quantizes to **100 µs** unless the user sets `chrome://flags/#enable-webgpu-developer-features`
  [c120, c121; cr-src `timestamp_quantization`]. Deltas can be negative; discard those [spec, c121]. So browser per-pass
  timings below about 0.1 ms mean nothing from a single frame. Firefox's and Safari's resolutions are unconfirmed.
- [bcd] lists Firefox as lacking `timestamp-query`, `depth-clip-control`, `float32-filterable` and
  `rg11b10ufloat-renderable`. The 161 source and [survey] (Firefox 99.9%, 100%, 98.3%, 100%) both contradict it, so MDN's
  table is wrong on these; trust `adapter.features`.
- Useful but not in the ticket: `texture-formats-tier1` adds `rg11b10ufloat` and `rg16float` storage textures
  (92.5%; Safari 26.2+, not in Firefox [bcd]); `subgroups` 67% and unavailable in Firefox 161 [fx-src].

## 4. Chrome's compatibility mode

- **Shipped in Chrome 146** (post dated 25 February 2026), Android only, over OpenGL ES 3.1; ChromeOS and Windows D3D11
  are "exploring" [c146]. [cr-src] still blocks D3D11, and says "Deliberately disable compat on linux". Firefox ignores the
  request with a warning and returns core [fx-main]. [bcd]: Safari and Firefox false.
- It is **opt-in** through `requestAdapter({ featureLevel: "compatibility" })` and is now in the CRD and Editor's Draft
  [tr, spec §4.2]. The proposal file's "must not expose" status line is stale.
- A browser that can't do it returns a core adapter. A browser that can may return a compatibility-defaulting adapter even
  on capable hardware; the device then keeps the restrictions unless the page requests `core-features-and-limits`
  [spec, c146]. **Asking for compatibility and not requesting that feature silently loses the limits and formats below.**

What a compatibility-only device removes, against the plan:

| Plan uses | Compatibility mode | Result |
|---|---|---|
| RGBA16F with 4x MSAA | Multisampled `rgba16float` and `r32float` disallowed [compat 22; tr] | **Lost.** MSAA on `rg11b10ufloat` works if that feature exists |
| Instances and skin in storage buffers read by the vertex shader (bind group 3) | `maxStorageBuffersInVertexStage` default 0 [compat 18] | **Lost** unless the adapter offers more; use vertex buffers or textures |
| Clusters, lights, materials as storage buffers in the fragment stage | Default 4 [compat 19] | Tight |
| Reading one mip of a texture while another is bound (Hi-Z, bloom chain) | One mip range, aspect and swizzle per texture per draw [compat 5] | Limited; separate passes still work |
| `textureLoad` on depth textures, depth with a plain sampler | Disallowed [compat 16, 20] | Depth prepass reads (AO, GI) need a copy or a comparison sampler |
| Cube-array views for point shadows | Disallowed [compat 4] | Not used by the plan's atlas; array views in bind groups must cover the whole array [compat 6] |
| Uniform blocks over 16 KiB, textures over 4096, 256 invocations | 16 KiB, 4096, 128 | Lower caps |
| `@interpolate(flat)` | Must be `flat, either` [compat 17] | Trivial to write everywhere |
| Depth bias clamp on shadow passes | Must be 0 [compat 9] | Use bias and slope only |

**Conclusion: compatibility mode isn't a free widening.** It reaches Android devices whose Vulkan is missing or blocked, and
it takes out the HDR MSAA target and the vertex storage buffers. Treat it as a separate, lower tier, or leave it out. Don't
pass `featureLevel`.

## 5. What fails today, and on mobile

| Gap | State |
|---|---|
| Firefox, Linux | No stable support, no date (section 1) |
| Firefox, Android | Blocklisted, no date |
| Firefox, Intel Mac | Off |
| Safari before 26; **any Safari on macOS 15 or older** | Off |
| Chrome, Linux | Works only on Intel Gen12+ (Mesa 22+) or recent NVIDIA; AMD and software off |
| Chrome, Android | Needs Android 12+ and an ARM, Qualcomm or Intel GPU, or Android 16+ with Imagination; ES 3.1-only devices need compatibility mode |
| Any browser | Insecure origin: `navigator.gpu` is missing [spec §4.1, SecureContext; cr-tips]. `file://` and a plain-`http` LAN address don't count; `localhost` does [cr-tips] |
| Any browser | Graphics acceleration off, driver blocklist, Lockdown Mode, Advanced Protection: `navigator.gpu` missing or adapter null |

Mobile-specific:

- iOS maximum buffer size and binding size are ⅓ of Metal's maximum, from 256 MiB up to 1 GiB [wk-src].
  Memory pressure and background-tab behavior on iOS are unconfirmed.
- `float32-filterable` is missing on 40% of iOS and 29% of Android reports [survey]. Use RGBA16F, or
  `textureLoad` plus a manual filter, for probe data.
- ARM Bifrost has no `depth-clip-control` [survey], so cascade "pancaking" needs a fallback: clamp depth in the vertex
  shader, which gives the same shadow at the cost of a distorted caster near the plane.
- 4x MSAA of RGBA16F costs 32 bytes per pixel before depth (8 bytes × 4 samples). `TRANSIENT_ATTACHMENT` (tile memory) is
  Chrome 146+ and transient attachments can't be resolve targets [c146; c149-150, 17 June 2026]. Check
  `'TRANSIENT_ATTACHMENT' in GPUTextureUsage`.
- Chrome on Windows ignores `powerPreference` and uses the adapter it already picked, normally the integrated GPU on
  laptops [cr-tips; cr-src LUID code]. On other platforms an unset preference means high performance on mains power and low
  power on battery [cr-src].

## 6. Feature detection for the page shell

Derived from [spec §4.1–4.3]; the API facts are cited, the order is this note's recommendation. Nothing here was run.

```js
async function acquireDevice() {
  if (!globalThis.isSecureContext) return fail("insecure-context");
  if (!navigator.gpu) return fail("no-webgpu");                  // browser, OS or blocklist

  let adapter = null;
  try { adapter = await navigator.gpu.requestAdapter(); }        // no featureLevel: core only
  catch (e) { return fail("adapter-error", e); }
  if (!adapter) return fail("no-adapter");                       // GPU, driver, settings

  if (adapter.info?.isFallbackAdapter) warn("software adapter"); // Chrome 136+; may be undefined

  const want = { maxStorageBufferBindingSize: 256 << 20 };       // optional raises only
  const need = { /* limits the engine cannot run without */ };
  for (const [k, v] of Object.entries(need))
    if (!(adapter.limits[k] >= v)) return fail("limit:" + k);    // also catches undefined
  const requiredLimits = { ...need };
  for (const [k, v] of Object.entries(want))
    if (k in adapter.limits) requiredLimits[k] = Math.min(v, adapter.limits[k]);

  const requiredFeatures = ["timestamp-query", "float32-filterable",
    "rg11b10ufloat-renderable", "depth-clip-control"].filter(f => adapter.features.has(f));

  let device;
  try { device = await adapter.requestDevice({ requiredFeatures, requiredLimits }); }
  catch (e) { return fail("device-error", e); }                  // TypeError: feature, OperationError: limit or consumed
  let lost = false;
  device.lost.then(info => { lost = true; onDeviceLost(info); });
  await Promise.resolve();
  if (lost) return fail("device-lost-at-birth");                 // request a new adapter once, then give up

  return { device, format: navigator.gpu.getPreferredCanvasFormat() };
}
```

Rules behind it:

1. `requestAdapter` resolves to **null**, it doesn't throw, when there's no adapter [spec]. `navigator.gpu` existing proves
   nothing. The same machine can return null after a GPU-process crash or a driver update [cr-tips].
2. **Don't require `core-features-and-limits` in the check.** Calling without `featureLevel` always gives a core adapter
   [spec §25.1], but Chrome before 139 doesn't list the feature [c139].
3. `requestDevice` rejects with `TypeError` for an unsupported or unknown feature and `OperationError` for a limit beyond
   the adapter, an unknown limit name, or an already consumed adapter. So filter features first, and read every limit
   before asking. An adapter is **consumed by one `requestDevice`**; a second call needs a new adapter [spec §4.3, c140].
4. A device can resolve **already lost** (an expired adapter). The spec then settles `device.lost` before the promise, so
   check it right after the await [spec §4.3].
5. Adapter limits aren't granted unless asked for [spec §3.6.2, c133].
6. Don't call `getContext` on the game's canvas before the decision: a canvas keeps one context type
   (HTML spec, not opened for this note). `tools/wasm_shell.html` already probes WebGL 2 on a throwaway canvas.
7. Keep the failure codes separate in the message: "use a newer browser" (`no-webgpu`), "update your driver or turn on
   hardware acceleration" (`no-adapter`), "open over https or localhost" (`insecure-context`).
8. Register `device.lost` and `uncapturederror` before the first frame. The page can't assume a lost device is an error it
   caused: the user agent may revoke one at any time [spec §3.5].
9. **The fallback adapter is a warning, not a gate**: real browsers return one in 0.05% of working reports [survey].

## 7. PLAN.md's browser claims

| PLAN.md | Verdict |
|---|---|
| Gap: Firefox on Linux and Android | **Holds** on 2026-11-25. Incomplete: also Firefox on Intel Macs |
| Gap: Safari before 26 | **Half right.** Needs iOS 26+ or **macOS 26+**; Safari 26 or 27 on macOS 15 and older doesn't have it (from source) |
| Gap: some Linux Chrome setups | **Understated.** Linux Chrome is allow-listed: Intel Gen12+ and recent NVIDIA only; AMD and software are off, and 18% of Linux reports have WebGPU [survey] |
| Compatibility mode (GLES 3.1) "may widen this later" | **Wrong in tense and size.** Shipped in 146 (February 2026), Android only, and it removes RGBA16F MSAA and vertex-stage storage buffers, which the plan uses |
| WebGPU has four bind groups | **Holds**, and Chrome never raises it |
| 4x MSAA on linear RGBA16F (passes table) | **Holds**: guaranteed in core, resolve and blending included |
| "Depth prepass", clustered storage buffers, 2048² and 1024² shadow maps | **Hold** within default limits: 8192² textures, 256 layers, 8 storage buffers per stage |
| GPU timestamps per pass into the profiler | **Holds with a caveat**: 100 µs steps in Chrome and not guaranteed (98.9% [survey]). Fine for frame-level and GI-tier decisions averaged over frames, not for small passes |
| "The shell shows a message, as it does now for WebGL 2" | Holds; use the sequence in section 6 |
| Missing from the gaps list | Chrome on Android below 12, or with Xclipse and other GPUs; Windows ARM64 (moving); insecure origins; Lockdown Mode and Advanced Protection; Chrome needs `--enable-unsafe-webgpu` for software adapters |

Not judged here: the emdawnwebgpu minimum, the vcpkg Dawn port and SDL_GPU's lack of web.

## 8. What this means for the plan

- **Nothing here blocks WebGPU-only web** for the spike's bar on Windows and Chrome. The web audience it excludes is
  mostly Linux and older Macs and phones.
- Design to the **core defaults** (4 groups, 8 storage buffers per stage, 64 KiB uniform, 128 MiB storage binding,
  8192² textures, 256 layers, 16 KiB workgroup memory) and treat everything above as an optional raise.
- Don't depend on `float32-filterable`, `subgroups` or `timestamp-query`. `rg11b10ufloat-renderable` is safe in practice
  but not guaranteed. Keep a fallback for `depth-clip-control`.
- Uniform buffers hold at most 64 KiB in Chrome, so large per-instance data must be storage, vertex or texture data.
- A browser smoke test on a GPU-less Linux runner must pass `--enable-unsafe-webgpu`; that is a different code path from what
  players run.

## 9. Not confirmed

- Nothing was run in any browser; support, features and limits come from pages and source.
- Safari on macOS 15 being off, and on Intel Macs with macOS 26 working, are read from [wk-src].
- The WebKit branch `safari-7625.2` matching Safari 27.2 is an inference from its version number.
- Chrome on Windows ARM64: shipping version. Advanced Protection Mode behavior on Android.
- Any Chrome or Firefox on iOS, Samsung Internet and Opera: only [survey] figures, no vendor page opened.
- Timestamp resolution in Firefox and Safari.
- Edge: no Microsoft page naming WebGPU support was found; from [webdev] and [wiki] only.
- [survey] gives no date window, sample size or method, and folds similar values into one row, so treat its percentages as
  rough. It isn't primary.
- Chromium and Firefox `main` files are unversioned: code there may ship later than the stable versions named at the top.
- Frame cost of 4x RGBA16F on phones, and the iOS background and memory-pressure behavior.
