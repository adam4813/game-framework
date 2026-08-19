#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include <flecs.h>
#include <nlohmann/json_fwd.hpp>

// Data-driven level loading. A level file is JSON describing world singletons and a tree of named
// entities, each with a set of components. Rather than a giant `if (name == "cube") ...` switch,
// component names map to loader callbacks in a registry (Strategy/Registry pattern), so new
// component types become loadable by registering a loader — no changes to the core loader.
namespace engine::level {

// Parses a JSON value describing one component and applies it to `entity`.
using ComponentLoader = std::function<void(flecs::entity entity, const nlohmann::json& value)>;

// Applies a JSON value to a world-level singleton (e.g. ambient light).
using SingletonLoader = std::function<void(const flecs::world& world, const nlohmann::json& value)>;

// Registry singleton mapping component/singleton names to their loaders. Owned by LevelModule and
// extended by the game layer via RegisterComponentLoader / RegisterSingletonLoader.
struct LevelRegistry {
	std::unordered_map<std::string, ComponentLoader> loaders;
	std::unordered_map<std::string, SingletonLoader> singletonLoaders;
};

} // namespace engine::level
