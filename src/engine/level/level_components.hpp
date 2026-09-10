#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include <flecs.h>
#include <nlohmann/json_fwd.hpp>

// Data-driven level loading. A level file is JSON describing world singletons and a tree of named
// entities, each with a set of components. Rather than a giant `if (name == "cube") ...` switch,
// component/link/singleton names map to loader callbacks in a registry (Strategy/Registry pattern),
// so new component or relationship types become loadable by registering a factory — no changes to
// the core builder. Each engine module registers the factories for the components it owns.
namespace engine::level {

// Parses a JSON value describing one component and applies it to `entity`.
using ComponentLoader = std::function<void(flecs::entity entity, const nlohmann::json& value)>;

// Applies a JSON value to a world-level singleton (e.g. ambient light).
using SingletonLoader = std::function<void(const flecs::world& world, const nlohmann::json& value)>;

// Wires a relationship from `parent` to a freshly built child `child` (beyond the implicit ChildOf),
// e.g. a "render_with" link adds render::RenderWith(parent -> child) so a nested material entity is
// used by its renderable parent. Registered by the module that owns the relationship.
using LinkLoader = std::function<void(flecs::entity parent, flecs::entity child)>;

// Wires a relationship from an entity to another entity referenced *by name* elsewhere in the same
// level (resolved in a second pass after the whole entity tree is built), e.g. a "look_at" ref adds
// render::LookAt(self -> target) so a camera aims at the target entity's WorldTransform. Same shape
// as a LinkLoader; the difference is that the target is an arbitrary named entity, not the parent.
using RefLoader = std::function<void(flecs::entity self, flecs::entity target)>;

// Registry singleton mapping component/singleton/link/ref names to their factories. Owned by
// LevelModule and populated by each engine module (and the game layer) via RegisterComponentLoader /
// RegisterSingletonLoader / RegisterLink / RegisterRef.
struct LevelRegistry {
	std::unordered_map<std::string, ComponentLoader> loaders;
	std::unordered_map<std::string, SingletonLoader> singletonLoaders;
	std::unordered_map<std::string, LinkLoader> linkLoaders;
	std::unordered_map<std::string, RefLoader> refLoaders;
};

} // namespace engine::level
