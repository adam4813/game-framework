# Spec 2 · In-play HUD

Part of [Signal Room spec 2: Core loop](../spec.md). The Ticket rack, the Window clock, score and Handler Chain on
screen.

## User stories

74. As a player, I want the Ticket rack across the top of the screen, oldest on the left, so that I can read every open
    Ticket at a glance.
75. As a player, I want each Ticket card to show its callsign, its frequency label on a swatch in the frequency colour,
    and its tier, so that I know which Radio to go to and how urgent it is.
76. As a player, I want each Ticket's gauge to show the Stand-by overfill as a separate dim segment above the On Air
    line, so that I can see when it will go On Air.
77. As a player, I want the On Air gauge coloured green, amber or red by band, flashing in red, so that danger is
    obvious.
78. As a player, I want a Cleared Ticket to flash green and a Lost Intercept to flash red as it leaves the rack, so
    that I notice what happened.
79. As a player, I want the Window clock (m:ss) on screen, showing it's held during Orientation and marking Last Call,
    so that I always know the time left.
80. As a player, I want my score and the current Handler Chain multiplier on screen, with a pop when either changes,
    so that I feel the chain building.
81. As a designer, I want the HUD's layout numbers in a data file and its colours from the palette, so that the HUD is
    tuned without rebuilding.

## Implementation decisions

- **Built with the engine UI module** (`Panel`, `Label`, `ProgressBar`, `Stack`), scoped to the Room scene, drawn in
  `OnUI`. No ImGui in the player HUD.
- **Event-driven:** Ticket UI entities are created on `TicketArrived` and removed after their exit flash on
  `TicketCleared` / `LostIntercept`. Gauge fill, band colour and the clock update per frame from the Ticket and Shift
  clock components (cheap: at most 5 Tickets).
- **Ticket card:** callsign, tier label (from the tier row's `label`, e.g. "HIGH PRIORITY"), frequency swatch +
  label, and a gauge. The gauge shows 0–100% On Air in the band colour, plus the Stand-by part as a dim segment drawn
  past 100% up to the tier's maximum overfill. Red-band flash rate is data.
- **Clock:** `m:ss` of `window_left`. In Orientation it shows the full Window with a "HELD" tag; in Last Call it turns
  red with a "LAST CALL" tag. Labels are HUD data.
- **Score and chain:** score as an integer, chain as `×1.25`. A short scale pop on `ScoreChanged` / `ChainChanged`
  (duration and scale in data).
- **HUD layout file** (new data file): anchor, margins, card size and spacing, font sizes, flash and pop timings,
  label strings. Colours are palette tokens (bands, frequencies, text, panel).
- The HUD reads as a 1962 teleprinter rack in spirit, but greybox styling is enough: flat panels, the palette, the
  default font.
