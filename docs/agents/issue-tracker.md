# Issue tracker: Local Markdown

Issues and specs for this repo live as markdown files in `.scratch/`.

## Conventions

- One feature per directory: `.scratch/<feature-slug>/`
- The spec is `.scratch/<feature-slug>/spec.md`
- Implementation issues are one file per ticket at `.scratch/<feature-slug>/issues/<NN>-<slug>.md`, numbered from `01`, never a single combined tickets file
- Triage state is recorded as a `Status:` line near the top of each issue file (see `triage-labels.md` for the role strings)
- Comments and conversation history append to the bottom of the file under a `## Comments` heading

## When a skill says "publish to the issue tracker"

Create a new file under `.scratch/<feature-slug>/` (creating the directory if needed).

## When a skill says "fetch the relevant ticket"

Read the file at the referenced path. The user will normally pass the path or the issue number directly.

## Writing procgen acceptance criteria

Procedural generation is stochastic — global invariants often fight the emergent behavior you want. Phrase connectivity
and coverage criteria as **minimum guarantees**, not global invariants:

- ✅ "The starting village has at least one road connection"
- ❌ "All living towns are road-connected"

A global invariant ("all X have Y") forces a post-hoc reconciliation pass that overrides the simulation. A minimum
guarantee ("the player's X has Y") lets the simulation breathe. When you write `- [ ] The road graph connects all
living towns`, ask: *do I actually want to forbid isolated clusters, or do I just need the player's spawn to work?*

### Scale invariants belong in acceptance criteria

Procgen tickets must state **concrete dimensions** in their ACs. A ticket that passes at 16×16 and 1000×1000
is unspecified — the implementer will pick a convenient default and that default will drive every downstream
decision (town radius, noise frequency, minimap density, road width, map pixel scale). Late rework is the cost.

Every procgen ticket that owns a grid, a generation budget, or a placement radius must include:

- **World dimensions** — explicit `width × height` tile count, even if just `WorldGenParams` defaults.
- **Scale reasoning** — one sentence: why those dimensions (e.g., "towns at radius 10 occupy 3 % of
  a 700-tile edge; at 160 tiles they occupy 12 %").
- **Proportion assertions** — e.g. "a living town's footprint covers ≤ 5 % of the world area."

The spec's **Implementation Decisions** section must also state the chosen scale and its rationale so
downstream tickets (minimap, map modal, noise tuning) can be written correctly without re-deriving it.

## Wayfinding operations

Used by `/wayfinder`. The **map** is a file with one **child** file per ticket.

- **Map**: `.scratch/<effort>/map.md` (the Notes / Decisions-so-far / Fog body).
- **Child ticket**: `.scratch/<effort>/issues/NN-<slug>.md`, numbered from `01`, with the question in the body. A `Type:` line records the ticket type (`research`/`prototype`/`grilling`/`task`); a `Status:` line records `claimed`/`resolved`.
- **Blocking**: a `Blocked by: NN, NN` line near the top. A ticket is unblocked when every file it lists is `resolved`.
- **Frontier**: scan `.scratch/<effort>/issues/` for files that are open, unblocked, and unclaimed; first by number wins.
- **Claim**: set `Status: claimed` and save before any work.
- **Resolve**: append the answer under an `## Answer` heading, set `Status: resolved`, then append a context pointer (gist + link) to the map's Decisions-so-far in `map.md`.
