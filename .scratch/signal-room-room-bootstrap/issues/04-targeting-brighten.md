# 04: Targeting & the Target brighten

Spec: [Signal Room spec 1: Room & bootstrap](../spec.md)
Base branch: `feat/signal-room` (this game's main: branch off it, PR back into it, never into `main`)

**What to build:** As the player moves, the Station or Counter their Operator would act on brightens with a soft pulse
in the Operator's identity colour, with no outline, lift or floor tile (ticket 15). Facing a thing selects it. A cone
fallback forgives imprecise aim. A Station counts only from its worked sides, and Tarped Stations and props are never
picked.

**Blocked by:** 03 (Operator movement & collision).

**Status:** ready-for-agent

- [ ] A facing ray of ~0.9 m from the Operator's centre picks a cell. If that cell is a Counter, or an in-service
      Station the Operator stands on a worked side of, that is the Target.
- [ ] Otherwise the Target is the nearest workable thing within ~1.2 m inside ±50° of facing, or nothing.
- [ ] Tarped Stations and props are never candidates.
- [ ] The ray length and cone values are in the tuning file and on the Room panel's sliders.
- [ ] The Target is stored as a relationship from the Operator to the Station or Counter entity, and is visible in the
      Explorer.
- [ ] The Target brightens and pulses in the Operator's palette colour, with rate and strength in data. Nothing else
      brightens.
- [ ] The Room panel gets a ray and cone overlay.
- [ ] Harness tests:
  - facing a Counter point-blank targets it;
  - a Tarped Keypunch faced point-blank is never the Target;
  - facing the Telex from its unworked south side gives no Target;
  - a slightly-off aim within the cone targets the nearest thing;
  - with nothing in reach there is no Target.
- [ ] Runtime check: the brighten follows the Operator through the room in the running game.
