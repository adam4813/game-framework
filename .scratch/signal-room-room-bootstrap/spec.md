# Signal Room spec 1: Room & bootstrap

Status: ready-for-agent
Map: [Signal Room: Vertical Slice](../signal-room/map.md) (Spec 1 packet on
[Spec chunking & hand-off order](../signal-room/issues/13-spec-chunking-order.md))
Sources: [Room layout, movement & set-down](../signal-room/issues/07-room-layout-movement.md),
[Operators & Swap](../signal-room/issues/02-operator-seats-coop-input.md) (camera only),
[Greybox readability & juice](../signal-room/issues/15-greybox-readability.md)
Base branch: `feat/signal-room` (branch off it, PR back into it; it never merges into `main`)

## Problem Statement

Signal Room has a resolved design for its vertical slice, but there is no game to put it in. There is no
`games/signal-room/` Game Target, no room to stand in, no Operator to move and nothing to pick up. Every later spec
(core loop, Operators & AI, Faults, meta, audio) needs a running room to build on. Until one exists, nobody can judge
the things the design only guessed: whether the 13×9 Layout fits the fixed camera, whether 4.5 m/s and the dash feel
right, whether targeting picks the machine the player meant, and whether the greybox machines read from above.

## Solution

A playable greybox room. Launching the Signal Room executable (desktop or web) drops the player straight into Shift 1's
Layout, seen from a fixed high-angle camera that frames the whole floor. The machines stand on their tables: the
in-service ones (Radio 1, Decoder, Telex) show their silhouettes and idle lamps, and the rest stand Tarped under khaki
cloth. Dressing-prop tarps, walls, the rug and the storm window are there too. The player drives one Operator on the
keyboard. They walk with inertia, dash, slide along furniture and walls, and see whatever they would act on brighten in
their colour. A placeholder Item waits on a Counter. The player can pick it up, carry it (at its carry speed), and set
it down on any free Counter with a dust puff. Everything about the room, the Shift, the movement feel and the greybox
look is data. A developer can retune it from an ImGui panel and reload the Shift without rebuilding.

## User Stories

### Launching and the Game Target

1. As a developer, I want Signal Room to build as its own executable from `games/signal-room/`, so that it can be built,
   run and tested apart from the sandbox and other games.
2. As a developer, I want the desktop and WASM presets to build the Signal Room target along with the sandbox, so that
   I don't need a separate build recipe.
3. As a developer, I want the game's code compiled into a library that both the executable and the tests link, so that
   tests exercise the same code that ships.
4. As a developer, I want Signal Room's tests registered with CTest through the existing test presets, so that
   `ctest --preset desktop-test-release` runs them with the engine tests.
5. As a player, I want the game to open straight into Shift 1's room, so that I can start moving with no menus (menus
   arrive with the meta spec).
6. As a player on the web build, I want the room to load and play in the browser, so that the slice can be shared as a
   link.
7. As a developer, I want the room to run as a registered scene, so that the Flecs Remote API can switch to it and
   query it.
8. As a developer, I want the game's assets (Shift, Layout, Item, tuning, greybox and palette files) copied or preloaded
   with the build like the sandbox's, so that the executable finds them on desktop and web.

### Shift and Layout data

9. As a designer, I want a Shift file that names its Layout, so that several Shifts can share one floorplan.
10. As a designer, I want the Shift file to list which of the Layout's Stations are in service, so that every Station
    it doesn't list stands Tarped with no C++ change.
11. As a designer, I want the Layout authored as an engine Level JSON on a 1 m floor grid, so that it uses the engine's
    existing loader, names and refs.
12. As a designer, I want each Station placement to give its kind, cell, footprint and worked sides, so that I can move
    or resize a machine in data.
13. As a designer, I want Counters authored as 1×1 solid furniture worked from every open side, so that they double as
    short dividers.
14. As a designer, I want wall cells to carry a height class (full, low or none), so that the north wall stands tall,
    the side walls stay low and nothing hides the floor from the camera.
15. As a designer, I want the open south edge to block movement without a wall mesh, so that Operators can't walk off
    the floor and the camera's view stays clear.
16. As a designer, I want dressing props (future-machine tarps, the storm window, the foreground desk edge, the rug)
    placed in the Layout with no Station data, so that the room looks furnished without adding game rules.
17. As a designer, I want named spawn markers (`spawn_1`, `spawn_2`, …) in the Layout, so that a Shift's Roster can
    say where each Operator starts.
18. As a designer, I want the Shift's Roster to list one Operator with its spawn marker and identity colour, so that
    spec 3 can grow it to two without redefining it.
19. As a designer, I want the Shift to say which Items start on which Counters, so that this spec's placeholder Item
    (and later the Oil Can in Shift 2) is placed by data.
20. As a designer, I want the Layout to feed the same occupancy grid to the engine's `GridNavGraph`, so that the AI spec
    can path over the floor I authored.
21. As a designer, I want a clear load error naming the file and the bad entry (unknown Station name in the in-service
    list, a footprint off the grid, overlapping solid cells, a missing spawn marker), so that I can fix data quickly.
22. As a designer, I want the Shift 1–2 Layout to match the agreed 13×9 floorplan, with Radios at (1–2,0), (4–5,0) and
    (8–9,0), so that the first playable room is the one the design was tuned for.

### The Operator and movement

23. As a player, I want to walk with WASD or the arrow keys at 4.5 m/s, so that crossing the room takes about 3 s.
24. As a player, I want a short ramp up (about 0.08 s) and a short stop (about 0.15 s), so that movement has weight
    but still feels responsive.
25. As a player, I want diagonal movement no faster than straight movement, so that no direction is a shortcut.
26. As a player, I want my Operator to turn toward my input quickly (about 0.06 s), so that facing follows my intent.
27. As a player, I want to dash on Shift (2.2× for 0.2 s, 0.5 s cooldown), so that I can make up lost time.
28. As a player, I want to dash while carrying, so that carrying never locks me out of a dash.
29. As a player, I want carrying an Item to scale my walk and dash by its `carry_speed`, so that heavy or fragile Items
    can feel different later (every slice Item is 1.0×).
30. As a player, I want to slide along walls and furniture when I walk into them at an angle, so that I never stick on
    corners.
31. As a player, I want Tarped Stations and dressing-prop tarps with floor footprints to block me like any furniture,
    so that the room's obstacles are honest.
32. As a player, I want the storm-window cells and the gap between Radio 2 and Radio 3 to stay walkable, so that the
    room plays as drawn.
33. As a player, I want my Operator drawn in its identity colour (P1 brick red) with a collar, so that I can find it at
    a glance.

### Targeting, pick-up and set-down

34. As a player, I want the Station or Counter I'm about to act on to brighten with a soft pulse in my Operator's
    colour, so that I know what pressing the action key will do before I press it.
35. As a player, I want the Target to be the cell in front of me (about 0.9 m ahead) when it's a worked side of an
    in-service Station or a Counter, so that facing a thing selects it.
36. As a player, I want a fallback to the nearest workable thing within 1.2 m in a ±50° cone, so that I don't have to
    line up perfectly.
37. As a player, I want a Station to be targetable only from its worked sides, so that I can't work the Telex from the
    side it faces away from.
38. As a player, I want Tarped Stations and dressing props never to become my Target, so that out-of-service machines
    stay out of my way.
39. As a player, I want to press the action key at a Counter holding an Item while empty-handed to pick it up, so that
    I can carry it.
40. As a player, I want to press the action key at an empty Counter while carrying to set the Item down, so that I can
    hand things off.
41. As a player, I want pressing the action key with nothing sensible to do (occupied Counter while carrying, empty
    Counter while empty-handed, an in-service Station) to do nothing, so that a wrong press is harmless.
42. As a player, I want my Operator to turn to face the Target when it acts but never to jump to a new position, so
    that interactions feel like Overcooked.
43. As a player, I want the carried Item shown in front of my Operator, with cards tilted toward the camera, so that I
    can read what I hold.
44. As a player, I want a dust puff when I set an Item down, so that the set-down lands.
45. As a player, I want an Item never to end up on the floor, so that nothing gets lost.

### Camera and greybox reads

46. As a player, I want one fixed high-angle camera (about 55° pitch, looking north) that frames the whole floor, the
    north wall, the window and the beacon spot and never moves, so that I always see the whole room.
47. As a player, I want each Station to be a small machine on a table or bench that fills its footprint, so that I can
    see over and past it.
48. As a player, I want each Station kind to have its own silhouette (Radio set with antenna whip and traffic lamp,
    Decoder cabinet with two reels and a side crank, Telex with a platen roll, Keypunch with a slanted top, Counter as a
    wood block with a lighter top slab), so that I can name machines by shape.
49. As a player, I want each in-service Station's lamp and riser progress bar placed on the top deck or a rear riser,
    so that an Operator standing at the front never hides its state.
50. As a player, I want Tarped Stations shown as a khaki cloth with a lumpy top and dark rope bands and no lamp, so that
    out-of-service machines read at once.
51. As a player, I want the placeholder Item to have a clear shape, so that I can tell it from the furniture.
52. As a designer, I want greybox shapes built from engine primitives listed per kind in data, with named parts (lamp,
    riser bar, reels, crank, platen, paper), so that later specs can animate those parts without remodelling.
53. As a designer, I want every greybox colour to come from a palette data file (Operator identity colours, lamp
    colours, frequency colours 7.0 cyan / 7.5 magenta / 8.0 violet, tarp, wood, walls), so that colours are tuned in
    one place and frequency colours stay clear of Operator and lamp colours.

### Debugging and tuning

54. As a developer, I want an ImGui Room panel listing the Shift files with a Reload button, so that I can edit Layout
    or Shift JSON and see it without restarting.
55. As a developer, I want movement tuning sliders in the panel (walk speed, ramp up/down, turn time, dash multiplier,
    duration and cooldown) that apply live, so that I can tune feel in play.
56. As a developer, I want debug overlays for the floor grid, solid cells, worked sides, Operator circle and the
    targeting ray and cone, so that I can see why a Target was or wasn't picked.
57. As a developer, I want the Operator's position, velocity, facing, carried Item and Target visible as reflected
    components in the Flecs Explorer, so that I can inspect them at runtime.
58. As a developer, I want a headless test harness that loads a Shift, presses keys, steps frames and queries the
    world, so that room behaviour is covered without a window.

## Implementation Decisions

### Game Target and bootstrap

- **New Game Target per ADR-0006** under `games/signal-room/`: its own CMake file added from the root with
  `add_subdirectory`, its own `src/`, `tests/` and assets. The game code builds as a static library (game code only,
  linking `engine_lib`). A thin executable adds the entry point and a test target links the same library. A
  `add_signal_room_test` helper mirrors `add_engine_test`. Asset copy (desktop) and preload (WASM) mirror the sandbox's,
  including the WASM pre-js scripts. The entry point follows the sandbox `main` (platform, engine context, Flecs remote
  on desktop, emscripten main loop on web).
- **Engine modules imported:** level, render, input, particles, pathfinding, debug UI. **Not physics** (Jolt is not used
  in the slice, per issue 07). Every game component is pre-registered in the module constructor, as `AGENTS.md` asks.
  The crash that rule guards against still has an unverified cause (see
  [lazy component registration crash](../backlog/issues/05-lazy-component-registration-crash.md)).
- **One game module** that registers the game's Level loaders, components (with reflection, so the Explorer shows
  them) and systems. **One Room scene**, tagged for pipeline filtering. Its `Load` order: load the palette, greybox,
  Item and tuning registries → register systems → load the Shift → build the Layout → spawn the Operator and starting
  Items. On boot the game activates the Room scene with Shift 1. Title, Slot and Shift pickers are spec 5.
- **Game vocabulary:** use `games/signal-room/GLOSSARY.md` terms in identifiers: Shift, Layout, Station, Tarped,
  Counter, Item, Operator, Roster, Target. Never "level" for a Shift, never "Mainframe".

### Data files (this spec settles the Layout and Shift→Layout schema)

All JSON, loaded at Room load and re-read on Reload. Later specs extend these documents and never redefine them.

- **Shift file** (one per Shift): `id`, display `name`, `layout` (a path to a Layout file), `in_service` (Station
  placement names), `roster` (an ordered list; in this spec, one entry with `spawn` marker name and `colour` palette
  token), and `counter_items` (Counter placement name → Item id). Spec 2 adds Radios' frequencies, tickets, the
  Collection Schedule and Modifiers. Spec 3 grows the Roster. Spec 4 uses `counter_items` for the Oil Can.
- **Layout file** = an engine Level JSON with game-registered loaders:
  - a `floor_grid` singleton (`width` 13, `height` 9, `cell_size` 1.0 m, origin), which owns the occupancy grid;
  - `station` on a placement entity: `kind` (string key into the greybox catalogue: `radio`, `decoder`, `telex`,
    `keypunch`), `cell`, `footprint` (w×h in cells), `worked_sides` (subset of N/E/S/W). The entity name is the
    placement name the Shift refers to (for example `Radio1`, `Radio2`, `Radio3`, `Decoder`, `Telex`, `Keypunch`);
  - `counter` on a placement entity: `cell` (always 1×1, worked from every side that isn't solid);
  - `wall` runs: cells plus a `height` class (`full`, `low`), and an `edge_block` for the open south edge (it
    collides but has no mesh);
  - `prop`: `kind` (greybox catalogue key such as `tarp`, `storm_window`, `desk_edge`, `rug`), `cell`, `footprint`,
    and a `solid` flag. Wall-mounted props like the Power Panel tarp have no floor footprint;
  - `spawn_marker` with a `cell` (or a position) and a name.
  - The camera is an ordinary Layout entity using the engine `camera` loader and a `look_at` ref to a framing target,
    so retuning the shot is a data edit.
- **Kind keys are strings into registries, not enums.** Station kinds, prop kinds and Item ids are open-ended (Shifts
  3–10 add machines), so they are string-keyed registry entries, per `AGENTS.md`.
- **Item catalogue:** per Item `id`, greybox `shape` key, `carry_speed` (default 1.0), and `tilt_to_camera` (cards).
  This spec ships one placeholder Item. Spec 2 adds the route Items and their frequency stripe.
- **Greybox catalogue:** per kind, a list of primitive parts (cube, sphere, capsule) with offset, size and a palette
  token, plus optional part names (`lamp`, `riser_bar`, `reel_l`, `reel_r`, `crank`, `platen`, `paper`, `antenna`,
  `body`). Named parts become child entities that later specs can find by name and animate. The `tarp` entry is used
  both for Tarped Stations (over the Station's footprint) and for dressing tarps.
- **Palette:** named tokens → RGBA. It holds Operator identities (P1 brick red, P2 mustard), lamps (amber, green, red,
  grey), frequencies (7.0 cyan, 7.5 magenta, 8.0 violet), tarp khaki, rope, wood, counter top, walls, floor and rug.
  Radio bodies use a neutral token until spec 2 gives each Radio a frequency.
- **Tuning file:** walk 4.5 m/s; accel time 0.08 s; decel time 0.15 s; turn time 0.06 s; dash ×2.2 for 0.2 s with a
  0.5 s cooldown; Operator radius (start ~0.3 m); facing-ray length 0.9 m; cone fallback 1.2 m / ±50°; target pulse
  rate and brightness. The ImGui panel edits it live. Saving tuning back to disk is not required.
- **Bindings file:** actions `move_up`, `move_down`, `move_left`, `move_right`, `dash`, `interact` → key lists
  (defaults: WASD + arrows, Left/Right Shift, Space + E). This spec has a single keyboard table. Spec 3 turns it into
  the per-Operator binding layer, so game logic reads actions and never raw key codes.

### Scale

- 13×9 cells at 1 m, so the floor is 13 m × 9 m. At 4.5 m/s a full east-west run takes ≈ 2.9 s, which meets the ~3 s
  cross-room target. Machines fill their footprints on ~0.62 m tables. The camera at ~55° pitch fits the whole floor
  plus the north wall, as checked in the greybox prototype. Every number stays in the files above.

### Room simulation

- **Collision** is hand-rolled 2D on the floor plane: the Operator is a circle against the AABBs of solid cells (Station
  footprints, Tarped or not, Counters, walls, the south edge block and solid props), resolved with slide-along. No Jolt.
  Operator–Operator soft separation is spec 3 (this spec has one Operator), but the collision step should take a list
  of circles so it can be added without restructuring.
- **Movement** reads actions, builds a normalised wish direction (camera looks north, so up = −z/north), accelerates
  and decelerates toward `walk × carry_speed` (× dash while dashing), and turns facing toward input. Dash takes the
  current wish direction, or facing when there is no input. It works while carrying and respects the cooldown.
- **Targeting** is a system per Seated Operator (there is one in this spec), run each frame. Ray 0.9 m along facing →
  the cell. If that cell belongs to an in-service Station and the Operator is on one of that Station's worked sides, or
  the cell is a Counter, that is the Target. Otherwise take the nearest workable thing within 1.2 m and ±50°. Tarped
  Stations and props are never candidates. The Target is stored as a relationship from the Operator to the Station or
  Counter entity, which later specs read (verbs, Hints, chevrons).
- **Interaction** happens on the `interact` press event, not by polling a held state: empty-handed at a Counter with
  an Item → pick up; carrying at an empty Counter → set down. Anything else does nothing in this spec (the Bubble ✗
  refusal arrives with spec 2's verbs). The Operator turns to face the Target and never snaps position.
- **Carry and Counter contents** use relationships: the Operator *carries* the Item and the Counter *holds* the Item.
  An Item always has exactly one holder (Operator or Counter), so there is no floor state.
- **Rendering:** Stations, Counters, props and walls build their greybox children from the catalogue when the Layout
  loads. The Target brighten is a material tint and pulse driven by the Target relationship. The set-down dust puff is
  a particles-module burst, with its intensity in data. In-service Station lamps show their idle (off) state, and
  their riser bars are empty.
- **Debug UI:** a Room panel (Shift list + Reload, tuning sliders, overlay toggles) built on the platform `DebugUi`
  interface, scoped to the Room scene and running on the main thread. Debug draw goes through `Platform`.
- **Platform:** no direct Raylib calls from game code. Input comes through the engine `InputState`, drawing goes through
  render components and `Platform`.

## Testing Decisions

- **One seam: a headless Room harness**, confirmed with the user. It follows the scene-equivalence `TestWorld` (a real
  `EngineContext` on a `NullPlatform`, no window or GPU) and imports the Signal Room module. A test loads a Shift file
  (fixture files under the game's tests, plus the real Shift 1), presses actions by writing the bound keys into the
  `InputState` singleton, steps the world for N frames at a fixed delta, and asserts only on observable world state:
  Operator position and facing, the Target relationship, what the Operator carries, what each Counter holds, which
  Stations are Tarped, and load errors.
- **Good tests check behaviour a player or designer would notice, never internals.** Examples: "walking east into the
  Decoder for 2 s leaves the Operator outside its footprint"; "holding up-right does not outpace holding up"; "after one
  1 s walk the Operator is about 4.5 m away (within a tolerance for the accel ramp)"; "dash covers more ground, and a
  second dash inside 0.5 s does nothing"; "a Tarped Keypunch is never the Target even when faced point-blank"; "facing
  the Telex from its unworked south side gives no Target"; "pick up → walk → set down moves the Item to the second
  Counter, and an occupied Counter refuses"; "carry_speed 0.5 halves the walk distance"; "a Shift listing an unknown
  Station name fails to load with that name in the error"; "Shift 1 puts Radio1, Decoder and Telex in service and Tarps
  Radio2, Radio3 and Keypunch".
- Collision maths, targeting and schema parsing are covered **only through the harness**. No separate pure-function
  seam.
- **Prior art:** `tests/scene_equivalence` (`TestWorld` + `NullPlatform`) for the harness; `tests/unit` GoogleTest
  style (behaviour-named `TEST`s, `add_engine_test`) for structure and registration.
- **Runtime verification** (required by `AGENTS.md` before calling it done): launch the desktop build, use the Flecs
  Remote API to switch to the Room scene, query the Operator and Counter components, drive input overrides to walk,
  dash, pick up and set down, and confirm no runtime asserts. Also load the WASM build in a browser once.

## Out of Scope

- Station verbs, Transforms and Jobs; real route Items and frequency stripes; Radios' frequencies and the Antenna
  Coupler; Tickets, the Window, scoring, HUD, Briefing, Hints and Orientation (spec 2).
- Where-to-go chevrons and the Bubble (✗ refusal, AI intent): these need Transforms or AI (specs 2 and 3).
- The Seat model, the Seat ring and chevron, Swap, a second Operator, Operator–Operator separation, the AI Operator, the
  per-Operator binding layer (spec 3). Bindings stay keyboard-only here, but game logic reads actions so spec 3 can
  swap the source.
- Faults, the Oil Can, emergency lighting, Fault particles (spec 4); Requisitions, save, Title and Shift picker
  (spec 5); all audio (spec 6), so the room is silent.
- Station state animation (blinking lamps, spinning reels, crank, paper). The parts exist and are named, but they stay
  idle.
- Jolt physics, position snap on interaction, camera shake (dropped), gamepad, human co-op.
- Shift 2's own file and content. The shared Layout already contains the Shift 2 machines (Tarped in Shift 1), and
  spec 4 adds Shift 2.

## Further Notes

- Prerequisites are met: the Dear ImGui debug UI and the synth stream have both landed on `feat/signal-room`, and
  [Greybox readability & juice](../signal-room/issues/15-greybox-readability.md) is resolved. Its three.js prototype
  (branch `prototype/greybox-readability`, variant D) is the visual reference for silhouettes, part placement and the
  Target brighten.
- The engine's Level concept may be renamed later (out of scope on the map). Keep Layout loading behind the game's own
  "load Layout" call so a rename touches one place.
- If a useful term comes up while implementing (for example **Worked side** or **Footprint**), add it to
  `games/signal-room/GLOSSARY.md` rather than inventing synonyms.
- Exit criterion (from the packet): walk, dash and collide through the Shift 1 Layout among in-service and Tarped
  machines, and carry and set down an Item with the Target brighten showing.
