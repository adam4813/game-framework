# Research: What AO and probe GI need from a forward renderer

Ticket: "What AO and probe GI need from a forward renderer" (render pipeline, wayfinder research). Question: which
inputs do screen-space AO, DDGI-style software-traced probe GI, GPU cluster building and TAA need from the renderer
underneath, so that phases 1 to 4 can leave room for them, and what does WebGPU allow? Out of scope: choosing the
design. Section 8 only says which candidate constraints the sources support.

Date: 2026-11-25. All sources were opened on that date. Line numbers refer to the exact commits listed below. Statements
marked **(inference)** are reasoning from the cited facts, not something a source says. Statements marked
**(unconfirmed)** were not found on any page or file opened here.

## Sources (primary)

| Ref | Source |
|-----|--------|
| [wgpu] | WebGPU spec, gpuweb/gpuweb `spec/index.bs` @ `ca61f22` (2026-11-21). The published W3C draft is dated 2026-11-16, https://www.w3.org/TR/webgpu/ |
| [wgsl] | WGSL spec, gpuweb/gpuweb `wgsl/index.bs` @ `ca61f22` |
| [gtao] | Jimenez et al. 2016, *Practical Realtime Strategies for Accurate Indirect Occlusion*, https://www.activision.com/cdn/research/Practical_Real_Time_Strategies_for_Accurate_Indirect_Occlusion_NEW%20VERSION_COLOR.pdf |
| [xegtao] | GameTechDev/XeGTAO @ `a5b1686` (2024-04-22), `README.md` and `Source/Rendering/Shaders/XeGTAO.hlsli`. The README says Intel no longer maintains it |
| [ddgi] | Majercik et al. 2019, *Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields*, JCGT 8(2), https://jcgt.org/published/0008/02/01/ |
| [scaling] | Majercik et al. 2021, *Scaling Probe-Based Real-Time Dynamic Global Illumination for Production*, JCGT 10(2), https://jcgt.org/published/0010/02/01/ |
| [rtxgi] | NVIDIAGameWorks/RTXGI-DDGI @ `f33e496` (2024-02-05): `docs/`, `rtxgi-sdk/shaders/ddgi/`, `samples/test-harness/shaders/` |
| [bevy] | bevyengine/bevy @ `93af76a` (2026-11-25): a clustered forward renderer on wgpu |
| [godot] | godotengine/godot @ `25d0522` (2026-11-24): the Forward+ renderer (clustered forward, MSAA, SSAO, SDFGI) |
| [filament] | google/filament @ `863e7d7` (2026-11-25): a forward renderer with AO, MSAA and TAA |
| [three] | mrdoob/three.js @ `46ad70d` (2026-11-25): WebGPU GTAO, TRAA and clustered lights nodes |
| [speedball] | cl0nazepamm/speedball @ `a9b8e07` (2026-11-16): BVH-traced DDGI on WebGPU (the one PLAN.md cites) |
| [wicked] | turanszkij/WickedEngine @ `ee910b0` (2026-11-24): DDGI with a software BVH path |
| [flax] | FlaxEngine/FlaxEngine @ `5c4fba6` (2026-11-25): DDGI with a surface atlas |
| [olsson] | Olsson et al. 2012, *Clustered Deferred and Forward Shading*, https://www.cse.chalmers.se/~uffe/clustered_shading_preprint.pdf |
| [unity] | Unity 6000.3 manual, *Meta Pass* (Built-In pipeline), https://docs.unity.com/en-us/engine/6000.3/manual/lighting-overview/lighting-birp/meta-pass (read through a summarizing fetch) |
| [lumen] | Unreal Engine, *Lumen Technical Details*, https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine (read through a summarizing fetch) |
| [local] | This repo, `origin/main` @ `2af9cdf`: `src/engine/platform/light_clusters.hpp`, `light_clusters.cpp`, `assets/shaders/lighting.glsl`, `docs/adr/0009-clustered-forward-lighting.md` |

## 1. WebGPU facts that decide the rest

### Default limits [wgpu L1666–1921]

| Limit | Default (core) | Compatibility mode |
|---|---|---|
| `maxBindGroups` | 4 | 4 |
| `maxSampledTexturesPerShaderStage` | 16 | 16 |
| `maxSamplersPerShaderStage` | 16 | 16 |
| `maxStorageBuffersPerShaderStage` | 8 | 8 |
| ... in vertex / fragment stage | 8 / 8 | **0 / 4** |
| `maxStorageTexturesPerShaderStage` | 4 | 4 |
| `maxUniformBuffersPerShaderStage` | 12 | 12 |
| `maxStorageBufferBindingSize` | 128 MiB | 128 MiB |
| `maxTextureDimension2D` / array layers | 8192 / 256 | 4096 / 256 |
| `maxComputeInvocationsPerWorkgroup` | 256 | 128 |
| `maxColorAttachmentBytesPerSample` | 32 | 32 |

The texture, sampler and buffer limits count entries **across the whole pipeline layout per stage**, not per bind group
[wgpu L1737–1800]. So group 1 (shadows, clusters, GI) and group 2 (material) draw on the same 16 textures.

### Multisampled depth

- **There is no depth resolve.** `resolveTarget` exists on color attachments only [wgpu L12570]; the depth/stencil
  attachment dictionary has no such member [wgpu L12688].
- It can be **read**: a binding with `multisampled: true` and a sample type other than `float` [wgpu L6531–6535], as
  `texture_depth_multisampled_2d`, through `textureLoad` only [wgsl L4984, L18766]. No sampler, no gather.
- A multisampled texture has one mip and one layer, can't be a storage texture, and must be a render attachment; sample
  count is 1 or 4 [wgpu L4661, L4708–4713].
- All attachments of one pass have the same sample count [wgpu L12511]. **A 1x depth prepass therefore can't prime a 4x
  opaque pass.**
- A depth texture can be a read-only attachment and a texture binding in the same pass [wgpu L1162–1172].
- `TRANSIENT_ATTACHMENT` forbids every other usage, and requires clear and discard [wgpu L4722, L12619]. A depth buffer
  that AO reads can't use it.
- Sample positions are fixed and **none is the pixel center** [wgpu L16511–16520], so a depth taken from sample 0 is
  offset from the pixel center that view-space reconstruction assumes **(inference)**.
- The 4x color target at 1920×1080 is 66 MB in `rgba16float` and the 4x depth 33 MB in `depth32float` **(arithmetic: 8 ×
  4 and 4 × 4 bytes per pixel)**.

### Storage texture formats without optional features [wgpu "Plain color formats", L17594 on]

| Format | Filterable | Renderable | Storage write | Storage read-write |
|---|---|---|---|---|
| `rgba16float` | yes | yes | yes | `texture-formats-tier2` |
| `rg16float`, `rgb10a2unorm`, `r16float`, `r8unorm` | yes | yes | `texture-formats-tier1` | tier2 for `r16float` and `r8unorm`; none for the other two |
| `rg11b10ufloat` | yes | `rg11b10ufloat-renderable` | tier1 | no |
| `r32float` | `float32-filterable` | yes | yes | yes |
| `rg32float` | `float32-filterable` | yes | yes (core) | no |
| `rgba8unorm` | yes | yes | yes | tier2 |

The formats the DDGI paper and RTXGI use for probes (RGB10A2 or R11G11B10F irradiance, RG16F distance) [ddgi §3, Table
2; rtxgi `DDGIVolume.h` L45–47] and the usual AO formats (R8, R16F) are **not storage-writable in core WebGPU**. They
can be written as render targets.

### Usage scopes

- A subresource can't be sampled and storage-written in the same scope. A scope is one dispatch, or one whole render
  pass [wgpu L1136–1150, L1236–1262]. Probe tracing that samples last frame's irradiance and a blend that writes it need
  separate dispatches, and in-place blending needs read-write storage, a buffer, or ping-pong textures.
- A buffer written by a compute pass and then read in a render pass is fine: they are different scopes (same lines).

### WGSL

- Derivatives, `textureSample`, `textureSampleBias`, `textureSampleCompare` and `discard` exist in the **fragment stage
  only** [wgsl L8914, L18037, L18965, L19145, L19264]. Compute code uses `textureSampleLevel`, `textureSampleGrad`,
  `textureLoad`.
- Concrete integers are `i32` and `u32` [wgsl L2927–2929, L2981]. Today's `uint16_t` index list [local
  `light_clusters.hpp`] must be packed two per `u32` or widened.
- `@invariant` on the vertex `position` output guarantees equal results across pipelines only "if the data and control
  flow match" [wgsl L10075–10082]. A prepass that the opaque pass tests against needs it.
- `binding_array` doesn't appear in the WGSL spec at this commit (full-text search): there are no bindless textures.

## 2. Screen-space AO

### Inputs

- **Depth is the only required input.** GTAO takes "only the depth buffer and surface normals (which can be derived from
  it by differentiation or can be supplied separately)" [gtao, the paragraph introducing Section 4]. XeGTAO: depth and
  *optional* screen-space normals, but "it is always advisable to provide screen space normals" [xegtao README,
  "Implementation and integration overview", "Misc"]. Three.js reconstructs normals from depth when none are given
  [three `GTAONode.js` L76–80].
- Implementations that supply normals get them from the prepass: Godot switches its prepass to depth + normal/roughness
  when SSAO, SSIL, SSR or SDFGI is on [godot `render_forward_clustered.cpp` L2011–2026]; Bevy's SSAO requires
  `DepthPrepass` and `NormalPrepass` [bevy `ssao/mod.rs` L113].
- **Motion vectors are not needed.** XeGTAO uses only a 5×5 spatial filter and relies on TAA "when available"; the GTAO
  paper itself reprojects over 6 rotations [xegtao README "Denoising"; gtao Fig. 5].
- The view-space reconstruction in XeGTAO assumes a perspective camera (`xy * viewspaceDepth`) [xegtao `XeGTAO.hlsli`
  L104–110]. Bevy unprojects with the inverse projection matrix [bevy `ssao.wesl`, `reconstruct_view_space_position`],
  which also covers an orthographic camera **(inference)**. The engine has both [local].

### Reading 4x depth

| Implementation | With MSAA |
|---|---|
| Godot | Renders the prepass at the MSAA count, then a compute shader reads the samples: plain depth is **averaged**, depth + normal picks **one sample**, the one whose depth fewest others share [godot `effects/resolve.glsl`] |
| Filament | A separate single-sample depth pass ("structure pass") at a scaled resolution, shared by AO and contact shadows, independent of the color pass [filament `PostProcessManager.cpp` L606–666; `Renderer.cpp` L1228–1238] |
| Bevy | SSAO refuses to run unless `Msaa::Off` [bevy `ssao/mod.rs` L510–514] |

Godot's source format for the resolved depth is R32F storage, which core WebGPU allows. A WebGPU resolve can be a
compute pass to `r32float`, or a full-screen pass to `r32float` or to `frag_depth` **(inference from section 1)**.

### WebGPU limits the published versions hit

- XeGTAO-style depth prefiltering writes 5 mips in one dispatch. Bevy disables SSAO when
  `max_storage_textures_per_shader_stage < 5`, and documents it as not supported on WebGPU [bevy `ssao/mod.rs` L61–66,
  L110]. The default is 4, so the mip chain needs two dispatches (or a higher limit the adapter reports).
- Bevy writes AO to R16F when the adapter allows it, else R32F [bevy `ssao.wesl` L30–33]. Three.js draws AO as a
  full-screen render pass into a one-channel target, with no storage texture [three `GTAONode.js`, `_aoRenderTarget`].

### Where it is applied

- **Indirect light only.** Bevy: "SSAO does not apply to direct lighting" [bevy `ssao/mod.rs` L97]; the shader keeps
  `direct_light` and `indirect_light` in separate sums and multiplies irradiance, environment diffuse, environment
  specular and ambient by the occlusion [bevy `pbr_functions.wesl` L660–756]. Godot multiplies `ambient_light`, and
  scales direct light only by an opt-in `ao_light_affect`, default 0 [godot `scene_forward_clustered.glsl` L2098–2103,
  L2221, L2314; `BaseMaterial3D.xml`].
- **It combines with probe GI**, it isn't replaced by it: Godot blends SDFGI into `ambient_light` and then applies AO to
  it [godot same file L2091–2103]. RTXGI pairs DDGI with ray-traced AO because probe data is low frequency [rtxgi
  `Algorithms.md`].
- Multi-bounce AO needs the surface **albedo**, and specular occlusion needs **roughness and N·V**, at the place AO is
  applied [bevy `pbr_fragment.wesl` L541–551]. In a forward shader all three are at hand; a late full-screen composite
  would not have them.
- **Opaque only.** Bevy turns AO off for materials that don't write depth: "Transparent meshes don't contribute to the
  depth prepass, so SSAO should not be applied" [bevy `mesh.rs` L3591–3594].
- It costs the opaque shader **one texture**, read with `textureLoad` at the pixel, no sampler [bevy `pbr_fragment.wesl`
  L542].

### Depth prepass: required, and what it costs

- In a forward renderer AO is an input to opaque shading, so **depth must exist before the opaque pass**. Godot forces
  the prepass on when SSAO is used, whatever the project setting says [godot `render_forward_clustered.cpp` L2198–2199].
  Filament draws its structure pass before the color pass. The alternatives are last frame's depth, or applying AO after
  shading (which needs indirect light in its own target) **(inference)**.
- Cost or saving depends on overdraw. Godot: it "increases performance significantly in scenes with high overdraw, when
  complex materials and lighting are used", but "in scenes with few occluded surfaces, the depth prepass may reduce
  performance", and it suggests turning it off for fixed top-down or side-scrolling views. It is off by default on
  PowerVR, Mali, Adreno and Apple GPUs [godot `ProjectSettings.xml` L2944–2949].
- Clustered forward shading doesn't itself need the prepass here. Olsson's clustered forward requires a pre-z pass
  because it clusters the *visible samples* [olsson §4], while this engine builds lists for every cluster [local ADR
  0009, which rejects Forward+ partly for that reason].
- The prepass has to run everything that moves a vertex or drops a fragment: Godot discards alpha-scissor and alpha-hash
  in its depth passes [godot `scene_forward_clustered.glsl` L1356, L1411]; Bevy runs the prepass per material [bevy
  `bevy_core_pipeline/src/prepass/mod.rs` L22–23]. A dithered occluder fade is the same case **(inference)**.
- Not measured here: prepass cost with 4x MSAA on our scenes **(unconfirmed)**.

## 3. DDGI-style probes

### Per-frame inputs

From RTXGI's integration steps (update constants, trace, update probes, optional relocate / classify / variability,
query) and the two papers [rtxgi `Integration.md`; ddgi §4; scaling §6.3]:

| Input | What sources use it for |
|---|---|
| **Whole-scene geometry, not the view's** | Rays leave probes in every direction. A probe that the camera can't see still has to update while it shades a surface [scaling §6.2] |
| **Bounds of dynamic objects** | Probes "sleep" unless they shade a surface; the AABB of every dynamic object, grown by a grid cell, wakes the probes inside it [scaling §6.3, Listing 5] |
| **Static or dynamic, per object** | Probes near static geometry stay awake ("Vigilant"); probes are not relocated around dynamic geometry [scaling §6, §5]. Godot has a per-instance `gi_mode` of disabled, static or dynamic [godot `GeometryInstance3D.xml` L109–117] |
| **Stable instance identity** | Speedball rewrites only a moved object's instance records and TLAS ancestors; new or removed geometry is a structural rebuild [speedball `ADVANCED.md`, "Explicit scene changes"] |
| **Albedo and emissive at the hit** | Reflected radiance = direct + albedo/π × probe irradiance; albedo is clamped to 0.9 for energy loss. Emissive is added to the hit [rtxgi `ProbeTraceRGS.hlsl`, `CHS.hlsl` L77; wicked `ddgi_raytraceCS.hlsl` L501–502] |
| **Geometric normal and facing** | Back-face hits drive relocation and classification, and store a shortened negative distance [rtxgi `DDGIVolume.md`, `ShaderAPI.md`]. The query wants the geometric normal: "avoid using the shading normal" [rtxgi `Integration.md`] |
| **Lights, including off-screen ones** | Every hit is lit. Wicked and RTXGI shade from the scene's light buffer. Speedball keeps a separate list of 64, chosen by power, spot solid angle and **proximity to the probe volume**, not to the camera [speedball `ADVANCED.md`, "Clustered lighting"] |
| **Shadows at the hit** | RTXGI, Wicked and Speedball trace a shadow ray per light sample [rtxgi `Lighting.hlsl` L38–55; wicked `ddgi_raytraceCS.hlsl` L206–217, L471–480; speedball `gi_probes.js` L2140, L2203]. Flax marks shadowed lights for a distance-field trace [flax `GlobalSurfaceAtlasPass.cpp` L961, L1603]. **No opened implementation reads the camera's shadow cascades at hits**, so PLAN.md's "sun cascades where they cover the hit" is unproven here, and cascades don't cover what is behind the camera **(inference)** |
| **Sky** | A miss stores sky radiance [rtxgi `ProbeTraceRGS.hlsl`] |
| **Last frame's probes** | Sampled at hits for further bounces [rtxgi, ddgi §4] |
| **What changed (lights, materials, objects)** | Optional: RTXGI pauses a converged volume and resumes on "a light or object moves, an explosion occurs, weather changes" [rtxgi `DDGIVolume.md`, "Probe Variability"]. Speedball marks transforms, deforms, material values and lights dirty separately |

**Previous transforms are not a DDGI input** in any source opened. DDGI wants identity, motion flags and bounds.

Probe history is also per volume: camera-following volumes keep interior probes and invalidate only the edge planes on a
scroll [rtxgi `DDGIVolume.md`, "Infinite Scrolling Movement"].

### What the forward pass binds to sample probes

RTXGI's query takes, per volume, **3 sampled texture arrays** (irradiance, distance, probe data), **1 bilinear sampler**
and the **volume constants** [rtxgi `ShaderAPI.md`, `DDGIVolumeResources`]. Each lookup loops over 8 probes and takes
one distance and one irradiance sample per probe [rtxgi `Irradiance.hlsl` L90–180]. Speedball's receivers bind the same
three atlases, with extra atlases only for its reflection tiers [speedball `gi_probes.js` L1855–1907].

- Two cascades double the textures unless they share arrays. Nothing in the sources stops them sharing **(inference)**.
- Probe data is dropped if relocation and classification are (two textures). **(inference)**
- The DDGI paper states that a forward pass can query probes directly, as "indirect lighting buffers" [ddgi §3], and the
  2021 paper samples probes directly during shading [scaling §7.5].
- The view bias makes the query depend on the camera direction [rtxgi `Irradiance.hlsl`, `DDGIGetSurfaceBias`].

**Fragment-stage texture tally (arithmetic over PLAN.md's passes, not a design):** cascades 1 + shadow atlas 1 +
environment (irradiance, prefiltered, BRDF table) 3 + probes 3 + AO 1 = **9** in groups 0 and 1. A glTF PBR material
(base color, normal, ORM, emissive) adds 4, total **13 of 16**. That leaves 3 for a surface shader's own textures.
Unshared cascades (6 probe textures) leave none. Bevy warns at runtime when its view layout passes a safe texture count
for this reason [bevy `mesh_view_bindings.rs` L612–617]. Storage buffers are not tight in the core limits (clusters 3,
plus instances and shadow data, of 8), but compatibility mode allows 0 in the vertex stage and 4 in the fragment stage.

### Passes and resources added

- Per volume: trace, blend irradiance, blend distance, and optionally relocate, classify and a variability reduction, so
  **3 to 5 dispatches** per volume per update (plus reduction). Border texels are written inside the blend shader [rtxgi
  `DDGIVolume.md`; `ProbeBlendingCS.hlsl` L232].
- Resources: ray data, irradiance, distance, probe data (and two variability textures) [rtxgi `DDGIVolume.md`, "Texture
  Layout"].
- The paper used 8×8 irradiance and 16×16 distance texels per probe, and a 32×8×32 grid at 64 rays [ddgi §3, Table 2].
  That fits easily: 524,288 rays at 16 bytes is 8 MB of ray data against the 128 MiB binding limit; a 10-texel-wide
  irradiance tile at 32×32 per layer is 320 texels **(arithmetic)**.
- Tracing is the tight one. Speedball's trace kernel "already uses the portable WebGPU baseline of eight storage
  buffers, so a separate grid binding makes its pipeline invalid", so it packs lights and light cells into one buffer
  [speedball `gi_probes.js` L1784–1787]. It binds BVH nodes, triangle indices, vertex data, triangle materials and
  materials as 5 of them.
- Its answer to the format rules: probe **state lives in storage buffers** (read-write), then a copy kernel writes
  `rgba16float` atlases that receivers sample with hardware filtering [speedball `gi_probes.js` L1855–1907].
- Cost reference: 2.6 ms on an RTX 2080 Ti with **hardware** rays, 32×8×32 probes, 64 rays, of which 0.8 ms is the cast
  and 0.5 ms is the per-pixel sampling [ddgi Table 2]. The GI frame cost of software tracing on web: **(unconfirmed)**,
  no benchmark found.

### Content rules DDGI imposes

- "Avoid representing walls with zero thickness planes"; walls should be single-sided and as thick as the probe spacing
  demands, or light leaks. A probe every 2 to 3 m is the recommended density [rtxgi `DDGIVolume.md`, "Rules of Thumb"].
- The engine's `Plane` primitive is zero thickness [local `raylib_backend.cpp` L996] and `Material` has `double_sided`,
  so winding, mirrored transforms and the double-sided flag must reach the tracer **(inference)**.

## 4. Albedo and emissive for custom or procedural materials

All the engines read here have the material shader itself produce the two values; none derives them from material
constants alone, except as a fallback.

| Source | How |
|---|---|
| Unity | A **Meta Pass** per shader supplies albedo and emission to GI; built-in shaders have one, "if you are using a custom Shader, you can add your own" [unity] |
| Godot SDFGI | Draws the scene from 3 axes in a `MODE_RENDER_SDF` variant of every material shader, which writes `albedo` and `emission` (after the user's code ran) to volume images [godot `scene_forward_clustered.glsl` L2997–3060; `render_forward_clustered.cpp` L3219–3255] |
| Godot lightmaps | `MODE_RENDER_MATERIAL` renders albedo, normal, ORM and emission targets in UV2 space [godot same file L1005–1011] |
| Unreal Lumen | Captures material properties per mesh from several "cards" into a surface cache. Materials with view-dependent logic (camera position, pixel depth) "may appear incorrectly" and need a Ray Tracing Quality Switch [lumen] |
| Flax | Draws objects' G-buffer into tiles of a surface atlas, and reads that at hits [flax `GlobalSurfaceAtlasPass.cpp` L998–1264] |
| RTXGI, Wicked | Fetch the material's textures at the hit by **bindless** index [rtxgi `CHS.hlsl` L48–77; wicked `ddgi_raytraceCS.hlsl`], which WebGPU doesn't have |
| Speedball | Scalar fields, plus each material map **resampled to 256×256** and stacked in texture arrays. Shader-graph materials can't be read: an author hint (`giColor`, `giEmissive`) is used if given, else an untouched white becomes 0.5 gray, because default white at the albedo cap caused a "broad energy wash" [speedball `gi_scene.js` L48, L171–200] |

What follows for a renderer underneath:

- **Evaluating a surface function in compute at hits** rules out `textureSample`, derivatives and `discard` in it
  (section 1), and needs every surface's textures bound at once **(inference)**.
- **Evaluating it in a fragment pass** (bake or capture to a per-surface average) keeps it legal, but the function must
  return albedo and emissive **before lighting**, and must not depend on the camera, `frag_coord` or scene bindings
  **(inference, and Lumen's stated failure case)**.
- A per-surface average loses the position of an emissive texture's bright parts and of vertex colors (our `DynamicMesh`
  updates UVs and colors at runtime [local `platform.hpp` L313–314]) **(inference)**.

## 5. What tracing needs of CPU mesh data

- **Positions, indices and a material per triangle** are the minimum: Speedball's BVH is built from them, and packs
  positions, normals and UVs (8 floats) for the GPU, with UVs read only for maps and alpha [speedball `gi_scene.js`
  L46–47, L40–43]. With per-surface average albedo and no alpha testing, UVs aren't needed **(inference)**. Normals can
  come from the triangle, which is what the DDGI query wants anyway (section 3).
- **They must be kept after upload, or the BVH built at load.** Speedball rebuilds a BLAS only for geometry it hasn't
  cached, and its synchronous MeshBVH rebuild cost about 200 ms, so it caches and uses workers [speedball `gi_refit.js`
  L1–9; `ADVANCED.md`, "BLAS cache and off-thread builds"]. Physics wants the same arrays (PLAN.md, #35).
- **Simplified proxies cover only some geometry.** Lumen's software tracing takes static meshes, instanced and
  hierarchical instanced static meshes and landscape only; **World Position Offset is not supported** [lumen]. That is
  the same limit as PLAN.md's rule that a surface shader with a vertex offset always counts as moving: the CPU can't see
  where it moved.

**Skinned models:**

| Source | Treatment |
|---|---|
| Speedball | Skipped (`isSkinnedMesh` returns false: "bind-pose only; skip for now"). Other deforming meshes refit the BLAS bounds on the CPU from CPU-side vertices, cost O(vertices + nodes) [speedball `gi_scene.js` L102; `gi_refit.js` L1–25] |
| Lumen software | Not in the traced scene; hardware tracing "in particular … supports tracing against skinned meshes" [lumen] |
| Godot SDFGI | Only `GI_MODE_STATIC` geometry is in it; dynamic geometry receives only [godot `GeometryInstance3D.xml` L109–117] |
| Wicked software path | The BVH is built **on the GPU** from the geometry's GPU position buffer [wicked `bvh_primitivesCS.hlsl` L32–38]. Whether that buffer holds skinned positions was not traced **(unconfirmed)** |

With compute skinning the CPU has no posed vertices, so a triangle-level contribution needs the skinned output buffer to
be readable by a compute pass (`STORAGE` usage) and a GPU refit; capsules from the CPU pose avoid both **(inference)**.

## 6. GPU cluster building and shadowed lights

- **The CPU can still pick the shadowed lights on its own.** In every source that clusters on the GPU, the CPU gathers
  the lights and allocates the shadows:
  - Bevy has GPU clustering and, when it is on, skips the CPU cluster assignment [bevy `assign.rs` L190–191]. It still
    sorts the lights on the CPU (type, shadow-enabled first, entity id as tie-break) and caps the shadow maps by count
    [bevy `render/light.rs` L1216–1265, L1290–1300].
  - Godot's cluster builder is GPU; the CPU sorts lights by distance and calls `shadow_atlas_update_light` with a screen
    coverage it computes itself [godot `light_storage.cpp` L892–978; `renderer_scene_cull.cpp` L3537–3658]. Coverage is
    from the light's position and radius, so it needs no cluster data.
  - Three.js clusters in compute from a CPU list sorted by view depth; shadow-casting lights stay outside the clustered
    set [three `ClusteredLightsNode.js` L363, L409].
  - Filament clusters on the CPU, in jobs [filament `Froxelizer.cpp` L647–729].
- **The per-cluster budget keeps its order only if the builder walks the sorted list.** Three.js loops over the sorted
  lights per cluster and stops when full [three L878–900], so the first ones win. Bevy's populate pass appends by atomic
  counters from rasterized fragments [bevy `cluster/gpu.rs` L12–47], which gives no order. Today's "most important kept,
  ties by id, a still view never flickers" [local ADR 0009] needs the first kind, with the list sorted on the CPU
  **(inference)**.
- **Today the order is computed inside `BuildLightClusters`, after the view cull and before the 512-light cut**, and the
  result leaves only as `LightClusters::lights` [local `light_clusters.cpp`]. It isn't a separate function; the order is
  otherwise independent of the clusters.
- What the builder needs of the light list: position, range, and cone for spots, in one storage buffer, plus the view
  and slice parameters; it writes ranges, indices and an index counter. Four storage buffers of 8 **(arithmetic)**.
  Lists sized by the GPU can overflow: Bevy reads counts back and grows its buffers, so a few frames can be wrong [bevy
  `light/cluster/mod.rs` L58–66]. The `dropped` and `cluster_dropped` counts today come from the CPU and would need such
  a readback **(inference)**.
- The cluster lookup uses `gl_FragCoord` inside `PointLighting` [local `lighting.glsl`]. Code that shades a probe hit
  has no screen position, so **light evaluation must be separable from the cluster lookup** **(inference)**.
- `BuildLightClusters` drops lights that miss the camera's view. Probe GI needs them (section 3), so the **unculled**
  list has to reach the renderer **(inference)**.

## 7. Reprojection and TAA

- **Inputs.** Depth, motion vectors, a jittered projection, and color history. Bevy requires `TemporalJitter`,
  `MipBias`, `DepthPrepass` and `MotionVectorPrepass` [bevy `taa/mod.rs` L114].
- **TAA and MSAA exclude each other** in Bevy ("must also disable `Msaa`") and Three.js ("MSAA must be disabled") [bevy
  `taa/mod.rs` L102; three `TRAANode.js` L20]. Filament's motion-vector pass has an MSAA case with its own depth buffer,
  because the color pass's depth can't be reused [filament `PostProcessManager.cpp` L801–805].
- **Motion vectors can be limited to what moved.** Filament draws only the renderables that moved when it can reuse the
  color pass's depth, and skips the pass when nothing did [filament `PostProcessManager.cpp` L801–805; `Renderer.cpp`
  L1077–1079]. This is the same list `RenderScene` would keep.
- **Per object:** the previous matrix [bevy `PreviousGlobalTransform`, `prepass/mod.rs` L221–247; three
  `VelocityNode.js`]. **Per skinned model:** the previous joint matrices, double-buffered and swapped each frame [bevy
  `skin.rs` L67–84]; with compute skinning, the previous posed vertices **(inference)**. **Per vertex-offset surface:**
  its inputs from the previous frame, such as time **(inference)**.
- **Per view:** previous view and projection, **without jitter** [bevy `PreviousViewData.unjittered_clip_from_world`;
  filament `Renderer.cpp` L1072–1075], and a **reset** for camera cuts [bevy `taa/mod.rs` `reset`]. Both key history by
  view, so a view needs an identity that survives frames **(inference)**.
- **Jitter scope.** Filament jitters only the color pass: passes before it (structure, AO) use the plain camera
  [filament `Renderer.cpp` L984–993]. So a view needs two projections.
- **Everything that moves has to write motion**, or it ghosts; alpha-blended surfaces and particles can't, and render
  after TAA [bevy `taa/mod.rs` L104–111].

## 8. Ticket 10's candidate constraints

| Candidate | Verdict | Facts |
|---|---|---|
| Cluster tables are storage buffers; `RenderFrame.lights` stays flat | **Holds, add to it** | The buffers need compute-writable usage. Indices can't be 16-bit in WGSL. The light list must be **unculled and unbudgeted** (GI needs off-screen lights, the camera's view cull drops them) |
| Shadowed lights are picked by a CPU function of their own | **Holds** | Bevy, Godot and Three.js all pick on the CPU with GPU clusters (section 6). The premise "the order would be lost when clustering moves to the GPU" is **only half right**: the order dies if the builder appends by atomics; it survives if the builder walks a CPU-sorted list. Extracting the importance order either way serves both |
| Direct and indirect stay apart | **Holds, add to it** | AO is applied per light sum in shading (section 2). Also keep light *evaluation* apart from the fragment-coordinate cluster lookup, and the geometric normal apart from the shading normal |
| AO reads depth, so MSAA and prepass must not block it | **Holds, stronger than stated** | Forward AO needs depth *before* opaque, so a prepass (or a Filament-style 1x pass, which can't prime 4x) has to exist. WebGPU has no depth resolve, so the 4x depth must be stored and bindable, or a separate 1x depth pass drawn. PLAN.md gives the prepass no phase, and this is the one that fixes it |
| GI needs average albedo and emissive per surface, including custom surface shaders, and CPU mesh data | **Holds, but narrow** | A surface function must return unlit albedo and emissive with no camera, `frag_coord` or scene reads, and be drawable in a capture pass; an average loses emissive textures. Mesh data: positions, indices, per-triangle material; UVs only if alpha or textures are sampled at hits. Speedball shows the cost of not having it: a guessed gray or a hand-written hint |
| Reserved bindings | **Holds** | 16 sampled textures per fragment stage over all groups: about 13 used with AO, shared probe cascades and a PBR material (section 3). Compute tracing is near 8 storage buffers. Four bind groups are fully used by PLAN.md's layout |
| Stable draw ids and previous transforms (`RenderScene`) | **Holds, split** | Ids, moved flags and old + new bounds are for GI and shadow caching. Previous transforms are **only** for TAA and motion vectors, and need per-skin previous poses with them |

**Missing from the list:**

1. **Shadow maps readable in compute.** Cascades (and the atlas) need `TEXTURE_BINDING` and compute-visible binding if
   GI samples them, drawn before the GI update, which comes before opaque. Alternatively trace shadow rays, as every
   implementation read here does (section 3).
2. **A per-draw GI role**: static, dynamic, or none (and cast_shadow, `Hidden`, translucent, double-sided and winding).
   Godot and Speedball (`excludeFromGI`) both have one. Plane primitives are zero thickness, the case RTXGI warns about.
3. **Depth-only variants of every surface**, with the same vertex math as opaque (`@invariant` position) and the same
   discard or dither. AO covers depth-writing surfaces only.
4. **Sample count as a variable**, since TAA implementations exclude MSAA, and an AO that tolerates 4x.
5. **Formats**: no core storage writes to `rg16float`, `rgb10a2unorm`, `r8unorm`, `r16float`. Ask for
   `texture-formats-tier1` when the adapter has it, or write probes and AO as render targets or via
   `rgba16float`/`r32float`.
6. **Persistent view identity**, with a cut flag and unjittered matrices (TAA history, and probe-volume scrolling).
7. **Change tracking beyond transforms**: lights and material values, if a GI volume may pause when converged.
   `RenderScene` as planned compares matrices, poses and mesh versions only.
8. **Compute-skinned output usable as a storage buffer**, and posed vertices not only in a vertex buffer; previous poses
   for TAA.
9. **Surface shaders can't assume derivatives or implicit-LOD sampling** if they are ever to run in compute.

## 9. Not verified

- Anything measured: prepass cost under 4x MSAA, GTAO cost under WebGPU, software-traced DDGI on web. No numbers found.
- Whether camera shadow cascades sampled at probe hits, or GPU-refit BVHs for compute-skinned models, have a published
  precedent: none found in the sources opened, which isn't proof of absence.
- The `web-fetch` sources ([unity], [lumen]) were read through a summarizing fetch; their quotes are as it reported
  them. Lumen's statement that skeletal meshes are absent from software tracing is from the summary, not a quoted line.
- Wicked's `vb_pos_wind` and the rest of its geometry flow were not followed to skinning.
- Safari and Firefox support for the optional formats and limits above: see ticket 01; not checked here.
