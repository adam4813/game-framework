#include <format>
#include <string>

#include <nlohmann/json.hpp>

#include "engine/engine.hpp"
#include "layout.hpp"
#include "room_components.hpp"

using namespace engine;
using nlohmann::json;

namespace signal_room {

namespace {

// Loaders can't fail the Level load, so a bad field is recorded for LoadLayout to report.
void RecordError(const flecs::entity e, const std::string& error) {
	e.world().get_mut<LayoutErrors>().errors.push_back(std::format("'{}' {}", e.name().c_str(), error));
}

// A [x, y] pair of integers, or `def` when the key is absent.
glm::ivec2 ReadCell(const flecs::entity e, const json& j, const char* key, const glm::ivec2 def) {
	if (!j.contains(key)) {
		return def;
	}
	const auto& a = j.at(key);
	if (!a.is_array() || a.size() != 2 || !a[0].is_number_integer() || !a[1].is_number_integer()) {
		RecordError(e, std::format("has a bad {} (expected [x, y] in whole cells)", key));
		return def;
	}
	return {a[0].get<int>(), a[1].get<int>()};
}

glm::ivec2 ReadRequiredCell(const flecs::entity e, const json& j, const char* key) {
	if (!j.contains(key)) {
		RecordError(e, std::format("has no {}", key));
	}
	return ReadCell(e, j, key, glm::ivec2{0});
}

std::string ReadRequiredString(const flecs::entity e, const json& j, const char* key) {
	if (!j.contains(key) || !j.at(key).is_string()) {
		RecordError(e, std::format("has no {}", key));
		return {};
	}
	return j.at(key).get<std::string>();
}

// "worked_sides": ["N", "E", "S", "W"] (any subset) → Sides bits.
uint32_t ReadSides(const flecs::entity e, const json& j) {
	uint32_t sides = 0;
	for (const auto& side : j.value("worked_sides", json::array())) {
		const auto letter = side.is_string() ? side.get<std::string>() : std::string{};
		if (letter == "N") {
			sides |= Sides::North;
		}
		else if (letter == "E") {
			sides |= Sides::East;
		}
		else if (letter == "S") {
			sides |= Sides::South;
		}
		else if (letter == "W") {
			sides |= Sides::West;
		}
		else {
			RecordError(e, std::format("has unknown worked side {} (N, E, S or W)", side.dump()));
		}
	}
	return sides;
}

// Register a component loader that records a wrongly typed field (nlohmann throws on those) as a
// Layout error instead of letting it escape the Level load.
void RegisterGuardedLoader(const flecs::world& world, const std::string& name, level::ComponentLoader loader) {
	level::RegisterComponentLoader(
		world,
		name,
		[name, loader = std::move(loader)](const flecs::entity e, const json& j) {
			try {
				loader(e, j);
			}
			catch (const json::exception& ex) {
				RecordError(e, std::format("has a malformed {}: {}", name, ex.what()));
			}
		}
	);
}

} // namespace

void RegisterLayoutLoaders(const flecs::world& world) {
	level::RegisterSingletonLoader(world, "floor_grid", [](const flecs::world& w, const json& j) {
		FloorGrid grid{};
		grid.width = j.value("width", grid.width);
		grid.height = j.value("height", grid.height);
		grid.cellSize = j.value("cell_size", grid.cellSize);
		grid.origin = core::JVec3(j, "origin", grid.origin);
		w.set<FloorGrid>(grid);
	});

	RegisterGuardedLoader(world, "station", [](const flecs::entity e, const json& j) {
		e.set<Station>({
			.kind = ReadRequiredString(e, j, "kind"),
			.cell = ReadRequiredCell(e, j, "cell"),
			.footprint = ReadCell(e, j, "footprint", glm::ivec2{1}),
			.workedSides = ReadSides(e, j),
		});
	});

	RegisterGuardedLoader(world, "counter", [](const flecs::entity e, const json& j) {
		e.set<Counter>({.cell = ReadRequiredCell(e, j, "cell")});
	});

	RegisterGuardedLoader(world, "wall", [](const flecs::entity e, const json& j) {
		e.set<Wall>({
			.cell = ReadRequiredCell(e, j, "cell"),
			.footprint = ReadCell(e, j, "footprint", glm::ivec2{1}),
			.heightClass = ReadRequiredString(e, j, "height"),
		});
	});

	RegisterGuardedLoader(world, "edge_block", [](const flecs::entity e, const json& j) {
		e.set<EdgeBlock>({
			.cell = ReadRequiredCell(e, j, "cell"),
			.footprint = ReadCell(e, j, "footprint", glm::ivec2{1}),
		});
	});

	RegisterGuardedLoader(world, "prop", [](const flecs::entity e, const json& j) {
		e.set<Prop>({
			.kind = ReadRequiredString(e, j, "kind"),
			.cell = ReadRequiredCell(e, j, "cell"),
			.footprint = ReadCell(e, j, "footprint", glm::ivec2{0}),
			.solid = j.value("solid", false),
			.height = j.value("height", 0.0F),
		});
	});

	RegisterGuardedLoader(world, "spawn_marker", [](const flecs::entity e, const json& j) {
		e.set<SpawnMarker>({.cell = ReadRequiredCell(e, j, "cell")});
	});
}

} // namespace signal_room
