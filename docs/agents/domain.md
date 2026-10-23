# Domain Docs

How the engineering skills should consume this repo's domain documentation when exploring the codebase.

## Before exploring, read these

- **`GLOSSARY-MAP.md`**: lists each domain context and where its glossary and ADRs live.
- **The glossary for your context**: `docs/GLOSSARY.md` for the engine, `games/<name>/GLOSSARY.md` for a Game
  Target. Game glossaries build on engine vocabulary, so read the engine glossary too when working in a game.
- **ADRs**: `docs/adr/` (system-wide) plus the context's own `docs/adr/` if it has one; read those that touch the area
  you're about to work in.

If any of these files don't exist, **proceed silently**. Don't flag their absence; don't suggest creating them upfront. The `/domain-modeling` skill (reached via `/grill-with-docs` and `/improve-codebase-architecture`) creates them lazily when terms or decisions actually get resolved.

## File structure

Multi-context repo (see `GLOSSARY-MAP.md`):

```
/
├── GLOSSARY-MAP.md
├── docs/
│   ├── GLOSSARY.md            ← engine context
│   └── adr/                   ← system-wide decisions
├── games/
│   └── <name>/
│       ├── GLOSSARY.md        ← game context
│       └── docs/adr/          ← game-specific decisions (created lazily)
└── src/
```

When a Game Target gets its own glossary, add its row to `GLOSSARY-MAP.md`.

## Use the glossary's vocabulary

When your output names a domain concept (in an issue title, a refactor proposal, a hypothesis, a test name), use the term as defined in the glossary for its context. Don't drift to synonyms the glossary explicitly avoids.

If the concept you need isn't in the glossary yet, that's a signal: either you're inventing language the project doesn't use (reconsider) or there's a real gap (note it for `/domain-modeling`).

## Flag ADR conflicts

If your output contradicts an existing ADR, surface it explicitly rather than silently overriding:

> _Contradicts ADR-0007 (event-sourced orders), but worth reopening because…_
