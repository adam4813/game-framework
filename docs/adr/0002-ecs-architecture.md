# Flecs ECS as the Architectural Backbone for Entity Composition

Flecs 4.x ECS is the primary mechanism for composing entities, wiring data (components) to behaviour (systems), and filtering execution across scenes. It was chosen over OOP inheritance hierarchies (a `GameObject` base class with virtual dispatch) and EnTT. Flecs' declarative composition — entities are defined by which components they carry, not by their class — eliminates the tight coupling that comes from deep inheritance chains and makes entity "types" open to extension at runtime by adding/removing components. It also provides first-class tooling (the Flecs Explorer REST API) for runtime introspection.

## Considered options

**OOP inheritance / virtual dispatch**: New entity "kinds" require new subclasses; cross-cutting concerns (e.g. pausing, scene filtering) require either multiple inheritance or flags threaded through a base class. Rejected because it couples behaviour to type hierarchy in ways that grow brittle as the game expands.

**EnTT**: A solid, widely used ECS. Rejected in favour of Flecs' richer built-in feature set (relationships, observers, hierarchical queries, REST Explorer, pipeline phases) which maps well to a data-driven game engine without requiring those features to be built separately.

---

**Note**: ECS is the mechanism for *cross-cutting entity composition*. OOP and Gang-of-Four patterns (Strategy, Registry, Factory, etc.) remain the right tool *within* a module's own implementation when they genuinely reduce complexity or improve maintainability.
