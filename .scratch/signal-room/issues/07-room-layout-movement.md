# Room layout, movement & set-down

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: open
Blocked by: 01, 02

## Question

What do the Shift 1 and Shift 2 rooms look like and how do Operators move through them? Settle the floorplans (which stations, walls vs islands, Mainframe on the rug, tarped machines as obstacles), room scale relative to Operator speed (cross-room travel time is a tuning target), movement feel (inertia, dash multiplier and cooldown), Operator–Operator collision/blocking, the collision approach (Jolt character controller vs simple 2D shapes on the floor plane), the interaction range/facing rule for choosing a station, and whether rooms are authored as engine Level JSON.

Context: [Routine Cable route & station verbs](01-routine-cable-route.md) is resolved. Set-down is Counters + station slots only, never the floor; hand-offs go through Counters. Settle how many Counters each room has and where. The Keypunch stands in the room in both `keypunch` Modifier states (tarped/set dressing when unused).
