# Shadows Are Stable Cascades and One Cached Atlas

Dynamic shadows replace the planar projection. The one shadowed `DirectionalLight` (the first by entity id) gets
cascaded shadow maps: `RenderSettings.shadow_cascades` layers (default 4) of a `depth32float` 2D array, reversed-Z like
every depth target, out to `DirectionalLight.shadow_distance` (default 80 m), split by a log/linear blend. Each cascade
is fitted to its slice's bounding sphere and its light-space origin snapped to whole texels, so it doesn't shimmer when
the camera turns or moves. Its near plane is pulled back on the CPU until it takes in every caster's bounding sphere
along the light, so casters in front of the cascade still shadow it. Spot and point lights share one `depth32float`
atlas. A point light uses 6 square tiles, one 90° face each. `PickShadowedLights`, a pure CPU function, gives tiles to
at most `RenderSettings.max_shadowed_lights` lights, ranked as the clusters rank them, and sizes them by rank band.
A tile is redrawn only when its light changed, a caster's old or new bounds touch the tile's frustum (from the
`Renderer`'s `DrawTracker`), the tile was reassigned or resized, or everything was invalidated. Cascades follow the
camera and are redrawn every Frame.

Every lookup takes a world position and can answer "not covered", so phase 6 can sample the same cascades and atlas at
probe hits, in compute, with one more coarse cascade layer added to the array.

Rejected:

- **`depth-clip-control` (`unclippedDepth`) for casters in front of a cascade.** It's missing on some GPUs (ARM Bifrost),
  so a fallback is needed anyway, and two paths would both need testing. Clamping depth in the vertex shader
  ("pancaking") skews large triangles that cross the near plane.
- **Cascades fitted tightly to the view frustum.** Tighter, but the shadow edges crawl whenever the camera turns.
- **Cube maps for point lights.** They'd be a second kind of shadow binding in every pass and in phase 6's compute. In an
  atlas, a face is a tile like a spot light's and is cached on its own.
- **Redrawing every tile every Frame, or a whole light when one face is touched.** Still lights with still casters,
  most of a scene, would cost a full redraw each Frame. Testing per face redraws one or two faces for a moving prop.

Consequences: sphere fitting wastes some cascade resolution. A light changing rank band gets a new tile size and
redraws, which can pop. Lights past the budget, and lights without `casts_shadows` (off by default on point and spot
lights), are unshadowed. `DrawTracker` and tile caching are built last in phase 4. Until then every tile redraws.
