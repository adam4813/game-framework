# Operator feel decisions: dash cooldown and collar contrast

Status: needs-info
Source: review of [03: Operator movement & collision](../../room-bootstrap/issues/03-operator-movement-collision.md), 2026-10-18

Two readings of spec 1 that ticket 03 settled one way and the designer should confirm. Each answer is a small change.

## 1. When does the dash cooldown start?

Spec: "dash ×2.2 for 0.2 s with a 0.5 s cooldown", with the test example "a second dash inside 0.5 s does nothing".

- **Now:** the cooldown starts when the dash *ends*, so dashes start 0.7 s apart (`UpdateDash` in
  `games/signal-room/src/room/operator_movement.cpp`; the tuning file's comment says so).
- **Alternative:** the cooldown starts with the dash, so dashes start 0.5 s apart.

`SecondDashInsideTheCooldownDoesNothing` passes under both readings. Whichever is chosen, add a test that only that
reading passes: a dash attempt between 0.5 s and 0.7 s after the first one starts.

## 2. Should the collar contrast with the body?

Ticket: "spawns … in P1 brick red with an identity-colour collar". The catalogue's `operator` model draws both body and
collar in the identity colour, so the collar stands out only by shape (a wider disc at the neck).

- **Now:** body and collar both `identity`.
- **Alternative:** body in a neutral uniform token (a new palette entry such as `operator_uniform`), collar in
  `identity`. This is a data-only change in `assets/greybox/catalogue.json` and `palette.json`.

## Answer

_(designer: pick an option for each, then set Status: ready-for-agent)_
