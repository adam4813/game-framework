# Wind and waves

Back to the [spec](../spec.md).

## User stories

1. As a player, I want to see which way the wind is blowing and how strong it is, so that I can predict the waves.
2. As a player, I want a warning a few seconds before the wind shifts, so that I can reposition.
3. As a player, I want the wind to push me, the horde and my bubbles slightly, so that the breeze is always felt.
4. As a player, I want to see a crest coming, with an upwind foam line about 2 seconds ahead, so that I can time my
   response.
5. As a player, I want crests to tumble enemies (stun them and carry them downwind), so that waves are a tool I can
   use against the horde.
6. As a player, I want to hold a key to dig in so a crest can't move me, so that I can hold my position.
7. As a player, I want digging in to root me in place while my weapons keep firing, so that it's a stand-and-fight
   choice.
8. As a player, I want digging in to drain a stamina meter, and stamina to refill when I'm not dug in, so that I can't
   dig in forever.
9. As a player, I want to see my stamina, so that I know whether I can survive the next crest.
10. As a player, I want a crest to carry me when I'm not dug in, giving a speed burst I can still steer, so that I can
    surf out of trouble.
11. As a player, I want every third crest in a set to be big, so that there's a rhythm to learn.
12. As a player, I want big crests to also tumble lobsters, so that big waves feel different.
13. As a player, I want the waves to start predictable and get more chaotic, so that the game is easy to learn and hard
    to master.
14. As a player, I want tide pools to be calm where crests break up, so that I have safe pockets to stand in.
15. As a player, I want tumbled enemies to recover after a moment and come back at me, so that waves buy time instead
    of clearing the screen.

## Implementation decisions

- **Wind** is a singleton with direction, strength and gust. The direction shifts every 20–30 s by roughly 45–90°,
  turning over a few seconds, with a warning flag set ahead of the turn for the HUD. Gusts briefly raise strength.
  Randomness comes from the engine's deterministic `RngState`.
- **The wave field** is one deep module: a pure function of position, time and the wind's recent history that returns
  the water height and the push at that point. Every system that applies wave force samples it (player, enemies,
  bubbles, shells, the boss), and so do the visuals. Nothing else knows how waves are made.
- **Crests** travel in the wind's direction as bands perpendicular to it. A **set** is a group of crests about 6 s
  apart, with every third crest big.
- **Ramp to chaos:**
  - 0:00–1:30: one swell, steady sets.
  - 1:30–3:00: after each wind shift the previous swell keeps running alongside the new one, so crests cross and
    their heights add.
  - 3:00+: rogue waves (single large crests from a random direction near the wind's) arrive with a short warning.
- **Tumble:** a crest above the tumble threshold stuns an enemy for a short time and adds a knockback velocity along
  the push, decaying over about a second. Lobsters only tumble on big crests.
- **Dig in** is a held action. While held and stamina > 0, the player is rooted: wave push and wind push are ignored,
  movement input is ignored, and weapons fire as normal. Stamina drains while dug in and refills while not. Exact rates
  are tuning data.
- **Surf:** a player who isn't dug in takes the crest's push as extra velocity on top of steering.
- **Pools** reduce wave push to near zero inside their area. That's a property the wave field reads from the arena
  layout, so every consumer gets it for free.
- **Wind push** stays small (the prototype used about 1.2–1.5 × strength), so waves, not wind, are the main force.
