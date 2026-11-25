# The Renderer Takes a Whole Frame as Data, Not One Draw Call at a Time

Render systems stop drawing. They extract the Frame into plain data, a `RenderFrame` (views, draws, lights, environment,
2D list, settings), and hand it over in one call, `Renderer::Render(frame)`. Drawing leaves `platform::Platform`, which
keeps windowing, input, audio, saves and debug UI. This amends ADR 0001: the rest of the code still calls no graphics
library directly, but the seam that hides it moves up from "draw this object now" to "here is the Frame".

`Platform` is asked to draw one object, now, into the one current target. Shadow maps need the same objects drawn again
from other views, with different culling. Global illumination needs the whole scene at once. Neither fits a per-object
virtual, every new pass would add more of them, and each separate draw path (batched, skinned, dynamic, translucent)
bakes in the one pass it was written for.

The seam doesn't depend on which graphics API is behind it. It lands first, with raylib as the first `Renderer`, and
stays if the choice of API (ADR 0011) is reversed.

There is no scene graph. Flecs already holds the hierarchy (`ChildOf`), inherited materials and transforms; a tree would
be a second copy to keep in step. What moved, appeared or vanished since the last Frame is worked out by the renderer
from the Frames it is given, because Flecs change detection can't tell: it is per table, transform propagation writes
every entity it matches, and physics writes through `const_cast`.

Rejected:

- **Widening `Platform` with a call per pass.** Each pass (cascade, spot, cube face, probe) would need every draw path
  again.
- **A common GPU API under the current systems.** It moves the API, not the problem: the order of draws stays spread over
  `OnStore` systems. A second graphics API, if one is ever wanted, is a second `Renderer`, taking the same `RenderFrame`.
- **A scene graph owned by the renderer.**

Consequences: extraction lists everything that could be drawn and doesn't cull, since a caster off screen can still
throw a shadow into view; its cost is measured before the seam is built. What is drawn, and in which order, can be
tested headless. The fields and lifetime of `RenderFrame`, who owns meshes and textures, and how `Platform` is split are
decided separately, and the glossary's terms change with them.
