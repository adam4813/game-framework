#pragma once

#include <string_view>

#include <flecs.h>

#include "level_components.hpp"

namespace engine::level {

// Level module: owns the LevelRegistry. It is intentionally generic — it knows nothing about render,
// physics, audio, etc. Each engine module registers the factories for the components, singletons and
// relationships it owns (via RegisterComponentLoader / RegisterSingletonLoader / RegisterLink) in its
// own constructor, so LevelModule must be imported before those modules. Call LoadLevel to
// instantiate a JSON level into the world.
class LevelModule {
public:
	explicit LevelModule(const flecs::world& world);
};

// Register (or overwrite) a loader for a component name. `name` must match the JSON key used under
// an entity's "components" object.
void RegisterComponentLoader(const flecs::world& world, std::string_view name, ComponentLoader loader);

// Register (or overwrite) a loader for a world singleton, keyed by its JSON name under "singletons".
void RegisterSingletonLoader(const flecs::world& world, std::string_view name, SingletonLoader loader);

// Register (or overwrite) a link factory, keyed by the name used in a child entity's "link" field.
// The factory wires a relationship from the parent entity to the built child (e.g. "render_with").
void RegisterLink(const flecs::world& world, std::string_view name, LinkLoader loader);

// Register (or overwrite) a ref factory, keyed by the name used in an entity's "refs" object. The
// factory wires a relationship from the entity to another entity referenced by name elsewhere in the
// same level (resolved after the whole tree is built), e.g. "look_at" -> render::LookAt.
void RegisterRef(const flecs::world& world, std::string_view name, RefLoader loader);

// Register a loader that adds the zero-size tag `Tag` to an entity (the JSON value is ignored). A
// convenience over RegisterComponentLoader for pure tags such as game PlayerControlled markers:
//   level::RegisterTag<game::PlayerControlled>(world, "player_controlled");
template<typename Tag>
void RegisterTag(const flecs::world& world, const std::string_view name) {
	RegisterComponentLoader(world, name, [](const flecs::entity e, const nlohmann::json&) { e.add<Tag>(); });
}

// Parse a level JSON file and instantiate its singletons + entity tree into the world. Missing
// files or a malformed document are logged and abort the load; unknown component names are logged
// and skipped. When `parent` is set, every top-level entity in the level is created as its child
// (so a scene can own the whole tree under one root and tear it down with a single destruct).
// Returns the number of top-level entities created (-1 on load failure).
int LoadLevel(const flecs::world& world, std::string_view path, flecs::entity parent = {});

} // namespace engine::level
