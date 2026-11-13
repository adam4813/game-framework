# Spec 2 · Machine reads & guidance

Part of [Signal Room spec 2: Core loop](../spec.md). Lamp and riser-bar states, part animation, stamp confetti,
where-to-go chevrons and the Bubble ✗.

## User stories

82. As a player, I want each Station's lamp to tell its state (off = idle, amber blinking faster as progress rises =
    running, green steady = output waiting), so that I can read the room without walking over.
83. As a player, I want a riser progress bar on each Station in the lamp colour, so that I can see how far a Job has
    got.
84. As a player, I want a Radio's lamp to slow-blink in its frequency colour while On Air, glow steady while coupled,
    quick-blink grey during a Coupler reset and stay off when dark, so that the coupler reads at a glance.
85. As a player, I want the Decoder's reels to spin while it runs, faster while cranked, and its crank to turn while
    cranked, so that the machine looks busy.
86. As a player, I want the Telex's paper to stand behind it while loaded and slide up on transmit, with its lamp
    blinking amber while loaded and flashing green on transmit, so that the stamp lands.
87. As a player, I want paper confetti to burst from the Telex on a stamp, so that a transmission feels good.
88. As a player, I want a down chevron in my colour over every Station that would take the Item I carry right now,
    anywhere in the room, so that I know where to go next.
89. As a player, I want no chevrons while empty-handed and never over Counters, so that chevrons only mean "this Item
    goes there".
90. As a player, I want a red ✗ in a Bubble above my Operator when a press is refused, so that I know the press did
    nothing on purpose.
91. As a designer, I want blink rates, bar sizes, chevron and Bubble offsets, confetti intensity and glyphs in data, so
    that reads are tuned without rebuilding.

## Implementation decisions

- **Station read state** is derived from the Job phase and (for Radios) the coupler and online state, then drives the
  greybox's named parts from spec 1 (`lamp`, `riser_bar`, `reel_l`, `reel_r`, `crank`, `platen`, `paper`, `body`).
  Colours are palette tokens; blink rates, spin speeds and the bar's size are tuning data.
  | Station | Idle | Running | Done | Other |
  |---|---|---|---|---|
  | any | lamp off, bar empty | amber blink, rate rising with progress; bar fills amber | green steady; bar full green | |
  | Radio | dark: off, body dim | coupled + copying: steady frequency colour, bar in frequency colour | green steady | On Air: frequency-colour slow blink; Coupler reset: quick grey blink |
  | Decoder | reels still | reels spin; faster and crank turns while cranked | reels still | |
  | Telex | paper hidden | loaded: amber blink, paper standing | | transmit: green flash, paper slides up, confetti |
- **Confetti** is a particles-module burst on the Telex on each transmission, like spec 1's dust puff (its tuning in
  the tuning file).
- **Where-to-go chevrons:** for each Operator carrying an Item, a down chevron in its identity colour floats above
  every in-service Station that would accept that Item now (Idle or Loading, with a Transform taking it into a free
  slot). Use the same acceptance check as the interact rule, so the two can't disagree. Several Operators' chevrons
  over one Station sit side by side (only one Operator in spec 2, but don't assume it). A greybox catalogue entry
  `chevron` gives its shape; its height, bob and spacing are tuning.
- **Bubble:** one per Operator, above the head and offset to the side. On a refusal event it shows a red ✗ for a short
  time (data), restarting on a repeat. Glyphs are a data table (`refuse` now; spec 3 adds AI intent glyphs). It is a
  world-space greybox model or a projected UI element, whichever the implementer finds simpler; the glyph table
  stays data either way.
- **Brighten** from spec 1 stays as is; a brightened Station still shows its lamp and bar states.
