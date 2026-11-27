# Assets Are Entities Holding CPU Data, and the Renderer Pulls Them

Every asset (image, mesh, shader, animation set, font, sound, music) becomes an unnamed entity under one `assets` root,
outside every scene, holding its CPU data, an `AssetPath` when it came from a file, and a `version`. Components point at
assets through one exclusive relationship per role: `(Albedo, texture)`, `(NormalMap, texture)`, `(UseMesh, mesh)`,
`(UseSound, sound)` and so on, each tagged `AssetRole`. Each Frame, `RenderFrame` lists every live asset with its version.
The `Renderer` diffs that list against what it holds on the GPU: it uploads what is new or newer and frees what is gone.
This takes up #73 and settles what ADR 0010 left open: who owns meshes and textures.

The GPU copy is a cache the `Renderer` can rebuild. So `Renderer::Render(frame)` stays the only call across the seam, a
second `Renderer` or a lost device starts by uploading everything again, and headless code gets real texture sizes, mesh
bounds and text widths. Until now headless got zeros and -1 handles. Uploading or changing a texture or dynamic mesh is a
write to its data that bumps `version`. Filter and wrap are fields on the image.

A path asset is refcounted by the pairs that target it and destroyed when the last one goes, as `AssetRegistry` does
today. A runtime asset (an uploaded texture, a baked tilemap mesh) has no path and belongs to whoever made it. Level JSON
still names files by path: the loader turns each path into its asset entity and adds the pair. Deleting an asset removes
the pairs that point at it (`OnDeleteTarget: Remove`), so nothing dangles.

Rejected:

- **Upload calls on the `Renderer`, or a separate `GpuResources` interface.** A second call across the seam, with no
  depth behind it: each would pass straight through to the backend. A renderer swap or a lost device would also have to
  ask the engine for everything again.
- **One generic `UseAsset` relationship.** A material with albedo, normal and ORM maps has three textures, and a
  generic pair can't say which is which.
- **Typed `int` handles from a registry, keeping assets out of the ECS.** It would leave two asset models side by side,
  and lose the inheritance through `RenderWith` and prefabs that pairs on material entities get for free.
- **Change lists filled by observers instead of the complete list.** They're cheaper, but a swap or a lost device then
  needs a "send me everything" request back from the `Renderer`. A few hundred assets cost microseconds to list.

Consequences: CPU copies stay in memory alongside the GPU ones. Freeing the pixels of large static textures after upload
can come later if memory matters. Text measuring moves to a CPU font module. Audio loads by the asset's path, because it
isn't behind the render seam.
