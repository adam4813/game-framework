# Tidy-ups from the ticket 03 review

Status: ready-for-agent
Source: review of [03: Operator movement & collision](../../signal-room-room-bootstrap/issues/03-operator-movement-collision.md), 2026-10-18
Base branch: `feat/signal-room` (branch off it; land per `games/signal-room/AGENTS.md`)

Behaviour-preserving changes in `games/signal-room/`, except item 3, which adds a test. Each item is mechanical. Keep
every existing test passing and change no data files.

- [ ] **Split the action layer out of the movement file** (CODING_STANDARDS.md, "One concern per file"). Move the
      `OperatorInput` system and `WishDirection` from `src/room/operator_movement.cpp` into a new
      `src/room/operator_input.{hpp,cpp}` with `RegisterOperatorInput(world, roomRoot)`. `RoomScene::Load` calls it
      before `RegisterOperatorMovement`, so OperatorInput still runs first in `OnUpdate`. Update both headers' comments.
- [ ] **Give the greybox model builder its own file.** Move `BuildGreyboxModel` from `src/room/layout_greybox.{hpp,cpp}`
      into `src/room/greybox_model.{hpp,cpp}`; `layout_greybox.cpp` and `operator_spawn.cpp` include the new header.
- [ ] **Test the turn time.** `turn_time` (0.06 s) is the time for a 180° turn, so a 90° turn takes 0.03 s, about 1.8
      frames at 60 Hz. Add a test to `tests/operator_movement_test.cpp`: from spawn (facing north, yaw 0), hold `D`.
      After 1 frame `OperatorMotion::facing` is strictly between −π/2 and 0. After 2 frames it equals −π/2 (east)
      within 1e-4. Read `facing` from the Operator entity.
- [ ] **Remove the duplicated roster error.** In `src/room/operator_spawn.cpp`, the two
      `std::format("Shift file {}: roster entry {} ...")` blocks share their prefix; build the prefix once.
- [ ] **Repoint a stale reference.** `.scratch/backlog/issues/05-lazy-component-registration-crash.md` cites the rule
      in `AGENTS.md` and the code-review skill; the rule now lives in `CODING_STANDARDS.md` (Registration and
      reflection). Update the citation.

Done when: `cmake --build --preset desktop-test-release` and `ctest --preset desktop-test-release` pass with one more
test than before (63), `cmake --build --preset desktop-release` and `cmake --build --preset wasm-release` build.
