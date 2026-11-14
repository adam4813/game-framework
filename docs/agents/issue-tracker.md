# Issue tracker

The tracker depends on where the work belongs:

- **Framework work** (engine, tools, CI, docs, anything shared between games): GitHub issues on
  [`adam4813/game-framework`](https://github.com/adam4813/game-framework/issues), through the `gh` CLI.
- **Game Target work** (inside `games/<name>/`): local markdown files, gitignored, because games here are usually
  prototypes.

Root `.scratch/` is gitignored scratch space for local planning notes. Framework specs and tickets live on GitHub.

## Framework: GitHub issues

- **Spec**: a parent issue titled `Spec: <feature>`. It holds the problem, the solution, out of scope and notes, with
  one section per topic, each topic's user stories next to its implementation decisions. Testing decisions get their
  own section. When a topic is too long for the body, put it in a comment headed `## <Topic>` and link that comment
  from the body.
- **Ticket**: one issue per ticket, created as a sub-issue of its spec with
  `gh issue create --parent <spec#>`. Its `Spec:` line names the spec issue and the sections it implements
  (`Spec: #60. Read: Movement, Testing.`), so an implementer reads only those.
- **Triage state**: a label (see `triage-labels.md`). Each issue carries exactly one category and one state label.
- **Comments and history**: issue comments.
- **Blocking**: a `Blocked by: #NN` line near the top of the body.
- **Done**: the pull request that finishes the ticket says `Closes #NN`. It closes when the PR merges.

### When a skill says "publish to the issue tracker"

Run `gh issue create -R adam4813/game-framework`, with `--parent <spec#>` for a ticket and `--label` for its category
and state.

### When a skill says "fetch the relevant ticket"

Run `gh issue view <NN> --comments`, then read the spec sections its `Spec:` line names, opening the rest of the spec
issue only for context those sections lack.

## Game Targets: local markdown

Work for a single Game Target lives in that game's folder: the spec at `games/<name>/spec.md` (or a spec index plus
`games/<name>/spec/<topic>.md`), tickets at `games/<name>/issues/<NN>-<slug>.md`, and game-specific ADRs at
`games/<name>/docs/adr/`. A game with several features or its own backlog uses `games/<name>/.scratch/` instead
(`<feature-slug>/spec.md`, `<feature-slug>/issues/`, `backlog/issues/`).

`.gitignore` ignores `games/*/issues/` and `games/*/.scratch/`. To keep a ticket for later, commit it explicitly with
`git add -f <path>`. Once committed, it stays tracked.

- Past about 100 lines, a spec is an **index**: the problem, the solution, a Sections table (one row per topic: link
  + what it covers), out of scope and notes. Each topic lives in `spec/<topic>.md` with its user stories next to its
  implementation decisions; testing decisions get their own section.
- A ticket's `Spec:` line links the index and names the sections it implements
  (`Spec: [spec](../spec.md). Read: [Movement](../spec/operator-movement.md), [Testing](../spec/testing.md).`).
- One file per ticket, numbered from `01`, never a single combined tickets file.
- Triage state is a `Status:` line near the top of each file (role strings in `triage-labels.md`).
- Comments and history append to the bottom of the file under a `## Comments` heading.
- "Publish to the issue tracker" creates a file under the game's folder; "fetch the relevant ticket" reads the file
  at the given path or number.

## Tracking follow-ups and known limitations

When an implementer flags a known limitation, deferred work, or a crash risk (e.g. "chunking is a follow-up for
play-scale forests"), open a ticket for it immediately, in the tracker its work belongs to: `ready-for-agent` or
`needs-triage` as appropriate, with a `Blocked by:` line if it cannot start until another ticket completes. A
follow-up must be filed when it's found; an unfiled one is invisible to the coordinator and surfaces later as a
surprise blocker.

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

Used by `/wayfinder`. Wayfinding is exploratory, so it runs locally in both trackers: in `.scratch/<effort>/` for
framework work, in the game's folder for a Game Target. File the tickets that come out of it in the tracker the work
belongs to. The **map** is a file with one **child** file per ticket.

- **Map**: `<effort>/map.md` (the Notes / Decisions-so-far / Fog body).
- **Child ticket**: `<effort>/issues/NN-<slug>.md`, numbered from `01`, with the question in the body. A `Type:` line records the ticket type (`research`/`prototype`/`grilling`/`task`); a `Status:` line records `claimed`/`resolved`.
- **Blocking**: a `Blocked by: NN, NN` line near the top. A ticket is unblocked when every file it lists is `resolved`.
- **Frontier**: scan `<effort>/issues/` for files that are open, unblocked, and unclaimed; first by number wins.
- **Claim**: set `Status: claimed` and save before any work.
- **Resolve**: append the answer under an `## Answer` heading, set `Status: resolved`, then append a context pointer (gist + link) to the map's Decisions-so-far in `map.md`.
