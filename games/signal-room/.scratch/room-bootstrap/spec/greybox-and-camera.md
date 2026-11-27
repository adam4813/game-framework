# Spec 1 · Camera & greybox reads

Part of [Signal Room spec 1: Room & bootstrap](../spec.md). The fixed camera, the greybox catalogue and palette, and how Stations, Tarps and the Target render.

## User stories

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

## Implementation decisions

- **Greybox catalogue:** per kind, a list of primitive parts (cube, sphere, capsule) with offset, size and a palette
  token, plus optional part names (`lamp`, `riser_bar`, `reel_l`, `reel_r`, `crank`, `platen`, `paper`, `antenna`,
  `body`). Named parts become child entities that later specs can find by name and animate. The `tarp` entry is used
  both for Tarped Stations (over the Station's footprint) and for dressing tarps.
- **Palette:** named tokens → RGBA. It holds Operator identities (P1 brick red, P2 mustard), lamps (amber, green, red,
  grey), frequencies (7.0 cyan, 7.5 magenta, 8.0 violet), tarp khaki, rope, wood, counter top, walls, floor and rug.
  Radio bodies use a neutral token until spec 2 gives each Radio a frequency.
- **Rendering:** Stations, Counters, props and walls build their greybox children from the catalogue when the Layout
  loads. The Target brighten is a material tint and pulse driven by the Target relationship. The set-down dust puff is
  a particles-module burst, with its intensity in data. In-service Station lamps show their idle (off) state, and
  their riser bars are empty.
