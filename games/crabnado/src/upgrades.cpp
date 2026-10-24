#include "upgrades.hpp"

#include <algorithm>
#include <numeric>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "controls.hpp"
#include "engine/engine.hpp"
#include "player.hpp"
#include "run.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr std::string_view kLogTag = "Upgrades";

// Parses one registry entry. Returns false (with an error naming the file and entry) when malformed.
bool ParseUpgrade(const std::string_view path, const std::string& id, const nlohmann::json& entry, Upgrade& upgrade) {
	upgrade = {.id = id, .name = entry.value("name", id), .description = entry.value("description", "")};
	const auto effects = entry.find("effects");
	if (effects == entry.end() || !effects->is_array() || effects->empty()) {
		spdlog::error("[{}] '{}' entry '{}': 'effects' must be a non-empty array", kLogTag, path, id);
		return false;
	}
	for (const auto& effect : *effects) {
		const std::string stat = effect.value("stat", "");
		const std::string op = effect.value("op", "");
		const size_t dot = stat.find('.');
		if (dot == std::string::npos || dot == 0 || dot + 1 == stat.size() || (op != "add" && op != "multiply")
			|| !effect.contains("value") || !effect.at("value").is_number()) {
			spdlog::error(
				"[{}] '{}' entry '{}': each effect needs \"stat\": \"Component.member\", \"op\": \"add\" or "
				"\"multiply\" and a numeric \"value\", got {}",
				kLogTag, path, id, effect.dump()
			);
			return false;
		}
		upgrade.effects.push_back(
			{.component = stat.substr(0, dot),
			 .member = stat.substr(dot + 1),
			 .op = op == "add" ? UpgradeOp::Add : UpgradeOp::Multiply,
			 .value = effect.at("value").get<float>()}
		);
	}
	return true;
}

UpgradeRegistry LoadUpgrades(const std::string_view path) {
	UpgradeRegistry registry;
	const auto doc = core::LoadJsonFile(path, kLogTag);
	if (!doc) {
		return registry;
	}
	if (!doc->is_object()) {
		spdlog::error("[{}] '{}' must be an object of upgrades keyed by id", kLogTag, path);
		return registry;
	}
	for (const auto& [id, entry] : doc->items()) {
		Upgrade upgrade;
		if (ParseUpgrade(path, id, entry, upgrade)) {
			registry.upgrades.push_back(std::move(upgrade));
		}
	}
	spdlog::info("[{}] Loaded {} upgrades from '{}'", kLogTag, registry.upgrades.size(), path);
	return registry;
}

// Applies one effect to the player's component by reflection. Logs and skips an effect whose component
// or member the player doesn't have.
void ApplyEffect(const flecs::world& world, const flecs::entity player, const UpgradeEffect& effect) {
	flecs::entity component;
	player.each([&](const flecs::id id) {
		if (!id.is_pair() && std::string_view{id.entity().name().c_str()} == effect.component) {
			component = id.entity();
		}
	});
	void* data = component ? ecs_get_mut_id(world, player, component) : nullptr;
	if (data == nullptr) {
		spdlog::error("[{}] The player has no component '{}'", kLogTag, effect.component);
		return;
	}
	flecs::cursor cursor{world.c_ptr(), component, data};
	if (cursor.push() != 0 || cursor.member(effect.member.c_str()) != 0) {
		spdlog::error("[{}] '{}' has no reflected member '{}'", kLogTag, effect.component, effect.member);
		return;
	}
	const double current = cursor.get_float();
	cursor.set_float(effect.op == UpgradeOp::Add ? current + effect.value : current * effect.value);
	ecs_modified_id(world, player, component);
}

// Rolls up to kMoltChoices distinct upgrades from RngState.
MoltOffer RollOffer(const flecs::world& world) {
	const auto& upgrades = world.get<UpgradeRegistry>().upgrades;
	auto& rng = world.get_mut<ecs::RngState>();
	std::vector<int> pool(upgrades.size());
	std::iota(pool.begin(), pool.end(), 0);
	MoltOffer offer;
	const int count = std::min(kMoltChoices, static_cast<int>(pool.size()));
	for (int i = 0; i < count; ++i) {
		std::swap(pool[i], pool[rng.NextRange(i, static_cast<int>(pool.size()))]);
		offer.ids[i] = upgrades[pool[i]].id;
	}
	return offer;
}

// Spends the shells for one molt and offers its upgrades, pausing the run. With no upgrades to offer
// the molt level still rises and the run carries on.
void BeginMolt(const flecs::world& world, RunState& run) {
	run.shells -= run.shells_needed;
	++run.molt_level;
	run.shells_needed = ShellsNeeded(run.molt_level);
	const MoltOffer offer = RollOffer(world);
	if (offer.ids[0].empty()) {
		return;
	}
	world.set<MoltOffer>(offer);
	run.phase = RunPhase::Molting;
	world.add<scene::Paused>();
	spdlog::info("[Run] Molt {}: offering {}, {}, {}", run.molt_level, offer.ids[0], offer.ids[1], offer.ids[2]);
}

} // namespace

void RegisterUpgrades(const flecs::world& world, const std::string_view path) {
	world.component<UpgradeOp>().constant("Add", UpgradeOp::Add).constant("Multiply", UpgradeOp::Multiply);
	world.component<MoltOffer>().member<std::string>("ids", kMoltChoices);
	world.set<MoltOffer>({});
	world.component<UpgradeRegistry>();
	world.set<UpgradeRegistry>(LoadUpgrades(path));

	world.system<RunState>("MoltCheck")
		.term_at(0)
		.singleton()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, RunState& run) {
			if (run.phase == RunPhase::Playing && run.shells >= run.shells_needed) {
				BeginMolt(it.world(), run);
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	world.system<const PlayerIntent>("MoltPick")
		.term_at(0)
		.singleton()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, const PlayerIntent& intent) {
			if (intent.pick > 0) {
				ChooseUpgrade(it.world(), intent.pick - 1);
			}
		})
		.add<scene::GameScene>();
}

const Upgrade* FindUpgrade(const flecs::world& world, const std::string_view id) {
	const auto& upgrades = world.get<UpgradeRegistry>().upgrades;
	const auto found = std::ranges::find(upgrades, id, &Upgrade::id);
	return found == upgrades.end() ? nullptr : &*found;
}

void ChooseUpgrade(const flecs::world& world, const int choice) {
	auto& run = world.get_mut<RunState>();
	const flecs::entity player = FindPlayer(world);
	if (run.phase != RunPhase::Molting || !player || choice < 0 || choice >= kMoltChoices) {
		return;
	}
	const Upgrade* upgrade = FindUpgrade(world, world.get<MoltOffer>().ids[choice]);
	if (upgrade == nullptr) {
		return;
	}
	for (const auto& effect : upgrade->effects) {
		ApplyEffect(world, player, effect);
	}
	world.set<MoltOffer>({});
	run.phase = RunPhase::Playing;
	world.remove<scene::Paused>();
	spdlog::info("[Run] Molt {}: chose {}", run.molt_level, upgrade->id);
}

} // namespace crabnado
