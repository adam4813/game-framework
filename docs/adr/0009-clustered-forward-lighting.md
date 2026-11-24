# Many Point Lights Use Clustered Forward Shading, Built on the CPU, and the Web Build Is WebGL 2 Only

Point lights are drawn with clustered forward shading. Each Frame the view frustum is split into screen tiles and depth
slices, the CPU lists the lights that reach each cluster, and each fragment evaluates only its own cluster's lights. A
scene can author any number of lights; the ones in view are kept within a budget
(`src/engine/platform/light_clusters.hpp`).

It stays forward because the engine's draws already are: translucent and instanced draws, unlit materials and custom
`ShaderMap` shaders keep working as they are, and a custom shader opts in to the lights with
`#include "lighting.glsl"`.

The lists are built on the CPU and reach the GPU as RGBA32F textures read with `texelFetch`, because desktop raylib 5.5
is built for OpenGL 3.3, macOS stops at 4.1 and WebGL 2 has no compute: none of them has both compute shaders and
storage buffers. The assignment is plain C++ in the platform layer, so it is unit tested without a GPU and a future
backend reuses it.

The web build therefore needs WebGL 2 (`texelFetch`, float textures). There is no WebGL 1 fallback: the page shows a
message instead. raylib is built for OpenGL ES 3 through the overlay triplet `triplets/wasm32-emscripten.cmake`, and
shaders are one GLSL source for both targets, with the version line prepended at load
(`src/engine/platform/shader_source.hpp`).

Rejected:

- **Deferred shading.** It shades only visible fragments, but needs a forward path beside it for translucent draws, and
  every `ShaderMap` shader would have to write a G-buffer or stay forward.
- **Tiled Forward+** (screen tiles with no depth slices). It needs a depth prepass to cull well where depth varies, and
  its lists suit translucent draws less.
- **Raising the uniform arrays.** Every fragment would pay for every light.
- **A WebGL 1 path with 4 lights.** It would keep a second set of shaders in step by hand.

Consequences: a light needs a bound to be clustered by, so `PointLight` has a `range` where it falls off smoothly to
zero, derived from its attenuation when unset. Over a budget the most important lights are kept, by intensity over
squared distance with ties broken by entity id, so a still view never flickers; a cluster over its own budget can show a
seam against its neighbors. Every backend clips at `kClipNear` and `kClipFar`, which the cluster grid spans. Each light
leaves one texel spare, for a spot light's direction and cone.
