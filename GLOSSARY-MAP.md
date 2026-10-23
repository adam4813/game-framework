# Glossary Map

This repo has multiple domain contexts. Read the glossary for the context you're working in; game contexts build on the
engine's vocabulary (Scene, Module, Level, …) without redefining it.

| Context | Glossary           | ADRs        | Covers                                    |
|---------|--------------------|-------------|-------------------------------------------|
| Engine  | `docs/GLOSSARY.md` | `docs/adr/` | `src/engine/`, Sandbox, system-wide rules |

Each Game Target adds a row: `games/<name>/GLOSSARY.md` and `games/<name>/docs/adr/`.

System-wide decisions (anything that binds more than one context) stay in `docs/adr/`.
