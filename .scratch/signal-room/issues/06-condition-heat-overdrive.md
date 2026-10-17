# Condition, heat & Overdrive in Shift 2

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: open
Blocked by: 01

## Question

Define Shift 2's maintenance layer with only the Shift 1–2 stations available (no Supply Rack, so where do oil cans / spares come from?). Which machines have a Condition meter, what wears them, the Healthy → Rattling → Fault stages, which faults and repairs exist in the slice (gear jam, loose tube, paper jam, stuck valve; blown fuse needs spares, so is it in or out?), the core-temperature model and its thresholds, the Cooling station's controls (fan speeds, steam purge, coolant), Overdrive's risk/reward, and Shift 2's 'Mainframe runs hot' flaky-machine twist. Also: does Shift 1 have any wear beyond the scripted jam?

Context: [Routine Cable route & station verbs](01-routine-cable-route.md) is resolved. The Decoder is Walk-away 6 s with Hold-to-crank (~2.5 s); Overdrive builds on the crank. Shift 1's scripted jam is a Decoder card jam. The Keypunch only exists under the `keypunch` Modifier, so its paper jam is not in the slice. Wear/heat tuning should be expressible as Modifier overrides.
