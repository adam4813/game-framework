# AI Operator behaviour

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: open
Blocked by: 01, 02, 06

## Question

How does an AI Operator decide what to do and how is it 'correct but less efficient'? Settle: how it picks the next job (utility scoring over open tickets/route steps/repairs? GOAP? BT? weigh the engine's AI module), how it avoids clashing with the human (claims/reservations on stations and items), the inefficiency knobs (reaction delay, suboptimal picks, slower cranking, no Overdrive?), how it continues the human's in-progress job after a Swap, whether the player can steer it (ping/command) or not, and how it handles repairs and heat. Its tuning must live in data.

Context: [Routine Cable route & station verbs](01-routine-cable-route.md) is resolved. Work is item-driven Transforms with one Job per Station and station-owned progress (an AI can resume any paused Job); items match tickets by frequency at the Telex. The AI must pick Radio Presets by ticket frequency, avoid decoys and over-producing items no ticket wants, and hand off only via Counters.

Context from [Operators & Swap](02-operator-seats-coop-input.md): an AI Operator is any Operator no Seat holds. After Swap it first finishes the hand-off (Hold or carry, waiting at a busy Station), then picks its own job. Each AI Operator shows an intent icon for its target Station. Arriving cancels its action, with ~0.25 s coyote time on Holds.

Context from [Tickets, window & scoring for Shifts 1–2](05-tickets-window-scoring.md): Tickets go Stand-by (gauge
overfilled, frequency silent) → On Air; the Radio only copies a frequency with an On Air Ticket (decoys always on air).
Value falls with the gauge band (100/75/50%) and a Lost Intercept breaks the Handler Chain, so the AI's job pick should
weigh urgency and band, and may pre-position during Stand-by.
