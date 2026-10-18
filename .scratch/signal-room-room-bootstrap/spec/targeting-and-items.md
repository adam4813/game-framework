# Spec 1 · Targeting, pick-up & set-down

Part of [Signal Room spec 1: Room & bootstrap](../spec.md). The Target rule, the interact press, carrying and Counter contents, and the Item catalogue.

## User stories

34. As a player, I want the Station or Counter I'm about to act on to brighten with a soft pulse in my Operator's
    colour, so that I know what pressing the action key will do before I press it.
35. As a player, I want the Target to be the cell in front of me (about 0.9 m ahead) when it's a worked side of an
    in-service Station or a Counter, so that facing a thing selects it.
36. As a player, I want a fallback to the nearest workable thing within 1.2 m in a ±50° cone, so that I don't have to
    line up perfectly.
37. As a player, I want a Station to be targetable only from its worked sides, so that I can't work the Telex from the
    side it faces away from.
38. As a player, I want Tarped Stations and dressing props never to become my Target, so that out-of-service machines
    stay out of my way.
39. As a player, I want to press the action key at a Counter holding an Item while empty-handed to pick it up, so that
    I can carry it.
40. As a player, I want to press the action key at an empty Counter while carrying to set the Item down, so that I can
    hand things off.
41. As a player, I want pressing the action key with nothing sensible to do (occupied Counter while carrying, empty
    Counter while empty-handed, an in-service Station) to do nothing, so that a wrong press is harmless.
42. As a player, I want my Operator to turn to face the Target when it acts but never to jump to a new position, so
    that interactions feel like Overcooked.
43. As a player, I want the carried Item shown in front of my Operator, with cards tilted toward the camera, so that I
    can read what I hold.
44. As a player, I want a dust puff when I set an Item down, so that the set-down lands.
45. As a player, I want an Item never to end up on the floor, so that nothing gets lost.

## Implementation decisions

- **Item catalogue:** per Item `id`, greybox `shape` key, `carry_speed` (default 1.0), and `tilt_to_camera` (cards).
  This spec ships one placeholder Item. Spec 2 adds the route Items and their frequency stripe.
- **Targeting** is a system per Seated Operator (there is one in this spec), run each frame. Ray 0.9 m along facing →
  the cell. If that cell belongs to an in-service Station and the Operator is on one of that Station's worked sides, or
  the cell is a Counter, that is the Target. Otherwise take the nearest workable thing within 1.2 m and ±50°. Tarped
  Stations and props are never candidates. The Target is stored as a relationship from the Operator to the Station or
  Counter entity, which later specs read (verbs, Hints, chevrons).
- **Interaction** happens on the `interact` press event, not by polling a held state: empty-handed at a Counter with
  an Item → pick up; carrying at an empty Counter → set down. Anything else does nothing in this spec (the Bubble ✗
  refusal arrives with spec 2's verbs). The Operator turns to face the Target and never snaps position.
- **Carry and Counter contents** use relationships: the Operator *carries* the Item and the Counter *holds* the Item.
  An Item always has exactly one holder (Operator or Counter), so there is no floor state.
