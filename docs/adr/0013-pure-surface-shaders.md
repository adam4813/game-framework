# Custom Materials Are Pure Surface Shaders, Declared in a Manifest

A game's own material look is a **surface shader**: one WGSL file with `fn surface(in: SurfaceIn) -> SurfaceOut` and an
optional `fn vertex_offset(in: VertexIn) -> vec3`. `surface()` returns albedo, alpha, emissive, a world shading normal,
metallic, roughness and occlusion. It's pure: its inputs are world position, normals, UVs, vertex color, time and its own
textures and parameters, and it can't read the camera, the fragment coordinate, lights, shadows or the depth buffer. The
engine wraps it in every pass (depth, shadow, opaque, translucent) and both vertex layouts (rigid, skinned) and does all
the lighting, keeping direct and indirect light apart. A material without a surface shader uses `standard_surface()`, the
glTF-style PBR material reading the four map roles (`Albedo`, `NormalMap`, `OrmMap`, `EmissiveMap`), and custom shaders
usually call it and change its result. `ShaderMap{stages}` becomes `SurfaceShader{path}`.

A surface shader's interface lives in `shaders.json` beside its flags: the textures it adds (up to 3 beside the PBR maps,
or 7 without them) and its parameters, typed and with defaults. The engine generates the group-2 declarations from that
entry, so a file never names a `@group` or `@binding`. Each added texture is an Asset Role made at runtime
(`SurfaceRole::<name>`, exclusive, ADR 0012). The validation test, pipeline warm-up and the level loader all read the same
manifest, and the test compiles every valid combination of engine flags (skinned, alpha mask, unlit, occluder fade) and
the shader's own flags on Dawn's Null device.

The purity is what phase 6's GI needs: the engine can run any surface away from the camera and capture its average albedo
and emissive, with nobody declaring them by hand. It also keeps the engine free to change lighting, shadows and pass
structure without touching game shaders. The occluder fade, which reads the screen, becomes an engine step after
`surface()`, flagged per material (`Material.occluder_fade`). Vertex alpha is plain opacity.

`unlit` draws write the inverse of the tone map, divided by exposure, so they come out of HDR tone mapping as authored.
This requires a tone-map curve that is invertible on [0, 1).

Rejected:

- **Whole fragment shaders, as `ShaderMap` gives today.** Every custom shader would reimplement lighting, shadows,
  clusters and the depth variant, and break each time the engine changes them. GI couldn't capture its color.
- **Bindings written in the WGSL file, or parsed from annotations in it.** Binding numbers would then be the game's, and
  the engine couldn't add to the material group without editing every shader.
- **Separate static and instanced vertex layouts.** Every draw already reads its per-draw data by `instance_index`, so a
  static draw is one instance. A third layout would add a third of the variants and buy nothing.
- **A tone-map curve that is linear below a knee, or letting tone mapping shift unlit colors.** The first pins the curve's
  shape now and still shifts colors near white. The second loses the stylized look the games are built on.

Consequences: view-dependent looks (rim light, screen-space effects) have no hook yet. A second hook, which GI ignores,
comes when a game first wants one. Vertex offsets are invisible to the GI tracer, and a draw with one always counts as
moving for shadow caching. Up to 32 variants per combination of a shader's own flags must compile in the test. A slow
test is fixed by cutting combinations, never by sampling them.
