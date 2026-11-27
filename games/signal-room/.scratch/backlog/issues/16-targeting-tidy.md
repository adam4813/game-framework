# Targeting tidy-ups

Status: needs-triage
Source: code review of [04: Targeting & the Target brighten](../../room-bootstrap/issues/04-targeting-brighten.md), 2026-10-18

Judgement calls from the review that weren't fixed in ticket 04.

- [ ] **One per-side table.** `OnWorkedSide` (`operator_targeting.cpp`) cascades over `Sides::North/South/West/East`.
      `AddSideBars` (`room_overlays.cpp`) has its own per-side table, and so does `OpenSides`. A shared side →
      (normal, edge) table near `Sides` would serve all three.
- [ ] **Split `TargetingTuning`?** It holds both picking (ray, cone) and the brighten's look (rate, strength, lift). Split
      it once a second brighten-like effect (the where-to-go chevrons) needs its own look values.
- [ ] **Tuning validation is untested.** The harness always loads `assets/tuning.json`, so the loader's range errors
      (`brighten_lift` above 1, a negative `ray_length`) are never exercised. Let the harness point the Room at a fixture
      tuning file, then add a test for the error naming the field.
- [ ] **Overlay toggles over REST.** Setting `RoomOverlays` through the Remote API does nothing until the next Room
      load, because only the panel calls `BuildRoomOverlays`. An `OnSet` observer on `RoomOverlays` would make REST
      toggles (and agents' runtime checks) work live.
