#include "upgrades.hpp"

#include <algorithm>
#include <numeric>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "controls.hpp"
#include "crabnado_constants.hpp"
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
	upgrade.first_molt = entry.value("first_molt", false);
	if (const auto req = entry.find("requires"); req != entry.end()) {
		const std::string stat = req->value("stat", "");
		const size_t dot = stat.find('.');
		const bool hasBound = req->contains("min") || req->contains("max");
		if (dot == std::string::npos || dot == 0 || dot + 1 == stat.size() || !hasBound) {
			spdlog::error(
				"[{}] '{}' entry '{}': 'requires' needs \"stat\": \"Component.member\" and a \"min\" or \"max\", got {}",
				kLogTag, path, id, req->dump()
			);
			return false;
		}
		UpgradeRequirement requirement{.component = stat.substr(0, dot), .member = stat.substr(dot + 1)};
		if (req->contains("min")) {
			requirement.min = req->at("min").get<float>();
		}
		if (req->contains("max")) {
			requirement.max = req->at("max").get<float>();
		}
		upgrade.requires_ = std::move(requirement);
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

// The player's component named `name`, or an invalid entity.
flecs::entity FindPlayerComponent(const flecs::entity player, const std::string_view name) {
	flecs::entity component;
	player.each([&](const flecs::id id) {
		if (!id.is_pair() && std::string_view{id.entity().name().c_str()} == name) {
			component = id.entity();
		}
	});
	return component;
}

// Applies one effect to the player's component by reflection. Logs and skips an effect whose component
// or member the player doesn't have.
void ApplyEffect(const flecs::world& world, const flecs::entity player, const UpgradeEffect& effect) {
	const flecs::entity component = FindPlayerComponent(player, effect.component);
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

// Whether the player meets `upgrade`'s requirement. An unreadable stat counts as not eligible.
bool IsEligible(const flecs::world& world, const flecs::entity player, const Upgrade& upgrade) {
	if (!upgrade.requires_) {
		return true;
	}
	const UpgradeRequirement& req = *upgrade.requires_;
	const flecs::entity component = FindPlayerComponent(player, req.component);
	void* data = component ? ecs_get_mut_id(world, player, component) : nullptr;
	if (data == nullptr) {
		return false;
	}
	flecs::cursor cursor{world.c_ptr(), component, data};
	if (cursor.push() != 0 || cursor.member(req.member.c_str()) != 0) {
		return false;
	}
	const double value = cursor.get_float();
	return (!req.min || value >= *req.min) && (!req.max || value <= *req.max);
}

// Rolls up to kMoltChoices distinct eligible upgrades from RngState. On the first molt one eligible
// "first_molt" upgrade takes slot 1.
MoltOffer RollOffer(const flecs::world& world, const int moltLevel) {
	const auto& upgrades = world.get<UpgradeRegistry>().upgrades;
	auto& rng = world.get_mut<ecs::RngState>();
	const flecs::entity player = FindPlayer(world);
	std::vector<int> pool;
	for (int i = 0; i < static_cast<int>(upgrades.size()); ++i) {
		if (player && IsEligible(world, player, upgrades[static_cast<size_t>(i)])) {
			pool.push_back(i);
		}
	}
	MoltOffer offer;
	int filled = 0;
	if (moltLevel == 2) {
		std::vector<size_t> flagged;
		for (size_t i = 0; i < pool.size(); ++i) {
			if (upgrades[static_cast<size_t>(pool[i])].first_molt) {
				flagged.push_back(i);
			}
		}
		if (!flagged.empty()) {
			const size_t at = flagged[static_cast<size_t>(rng.NextRange(0, static_cast<int>(flagged.size())))];
			std::swap(pool[0], pool[at]);
			offer.ids[0] = upgrades[static_cast<size_t>(pool[0])].id;
			filled = 1;
		}
	}
	const int count = std::min(kMoltChoices, static_cast<int>(pool.size()));
	for (int i = filled; i < count; ++i) {
		std::swap(pool[static_cast<size_t>(i)], pool[static_cast<size_t>(rng.NextRange(i, static_cast<int>(pool.size())))]);
		offer.ids[static_cast<size_t>(i)] = upgrades[static_cast<size_t>(pool[static_cast<size_t>(i)])].id;
	}
	return offer;
}

// Spends the shells for one molt and offers its upgrades, pausing the run. With no upgrades to offer
// the molt level still rises and the run carries on.
void BeginMolt(const flecs::world& world, RunState& run) {
	run.shells -= run.shells_needed;
	++run.molt_level;
	run.shells_needed = ShellsNeeded(run.molt_level);
	const MoltOffer offer = RollOffer(world, run.molt_level);
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
			if (IsRunning(run.phase) && run.shells >= run.shells_needed) {
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
	run.phase = run.clock >= constants::kBossAt ? RunPhase::Boss : RunPhase::Playing;
	world.remove<scene::Paused>();
	spdlog::info("[Run] Molt {}: chose {}", run.molt_level, upgrade->id);
}

} // namespace crabnado
