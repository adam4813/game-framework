#include "villager_system.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "engine/engine.hpp"
#include "farm_components.hpp"
#include "game_time.hpp"
#include "player_inventory.hpp"
#include "seeded_random.hpp"
#include "shop_helpers.hpp"
#include "town_layout.hpp"
#include "villager_components.hpp"
#include "villager_schedule.hpp"
#include "world_data.hpp"

using namespace engine;
using engine::tilemap::PlayerControlled;
using json = nlohmann::json;

namespace game::farm {

// ── Dialog handler registration ───────────────────────────────────────────────

// Register give_item / take_item / give_coins / take_coins handlers that operate
// on the actor entity's PlayerInventory. Called once per world by RegisterVillagerSystems.
static void RegisterDialogInventoryHandlers(const flecs::world& world) {
	dialog::RegisterDialogNodeHandler(
		world,
		"give_item",
		[](const dialog::DialogRunContext& ctx) -> dialog::NodeResult {
			if (const auto* payload = ctx.node.try_get<dialog::DialogGiveItem>()) {
				if (auto* inv = ctx.actor.try_get_mut<PlayerInventory>()) {
					if (!AddItem(*inv, payload->item, payload->count))
						engine::ecs::Log(ctx.actor.world())
							.warn("[Dialog] give_item: inventory full, '{}' x{} lost", payload->item, payload->count);
				}
			}
			return {.flow = dialog::DialogFlow::Advance};
		}
	);

	dialog::RegisterDialogNodeHandler(
		world,
		"take_item",
		[](const dialog::DialogRunContext& ctx) -> dialog::NodeResult {
			if (const auto* payload = ctx.node.try_get<dialog::DialogGiveItem>()) {
				if (auto* inv = ctx.actor.try_get_mut<PlayerInventory>()) {
					if (HasItem(*inv, payload->item)) RemoveItem(*inv, payload->item, payload->count);
				}
			}
			return {.flow = dialog::DialogFlow::Advance};
		}
	);

	// give_coins: adds coins to the player (e.g., shop pays the player when selling).
	dialog::RegisterDialogNodeHandler(
		world,
		"give_coins",
		[](const dialog::DialogRunContext& ctx) -> dialog::NodeResult {
			if (const auto* payload = ctx.node.try_get<dialog::DialogGiveCoins>()) {
				if (auto* inv = ctx.actor.try_get_mut<PlayerInventory>()) {
					if (!AddItem(*inv, "coin", payload->amount))
						engine::ecs::Log(ctx.actor.world())
							.warn("[Dialog] give_coins: inventory full, {} coins lost", payload->amount);
				}
			}
			return {.flow = dialog::DialogFlow::Advance};
		}
	);

	dialog::RegisterDialogNodeHandler(
		world,
		"take_coins",
		[](const dialog::DialogRunContext& ctx) -> dialog::NodeResult {
			if (const auto* payload = ctx.node.try_get<dialog::DialogGiveCoins>()) {
				if (auto* inv = ctx.actor.try_get_mut<PlayerInventory>()) {
					// Deduct coins; silently skip if the player can't afford it.
					if (CountItem(*inv, "coin") >= payload->amount) RemoveItem(*inv, "coin", payload->amount);
				}
			}
			return {.flow = dialog::DialogFlow::Advance};
		}
	);
}

// ── BT handler registration ───────────────────────────────────────────────────

static void RegisterVillagerBtHandlers(const flecs::world& world) {
	auto& reg = world.get_mut<ai::BTHandlerRegistry>();
	reg.actions["villager_waypoint"] = [](flecs::entity e, ai::Blackboard&, float) -> ai::BTStatus {
		if (!e.has<VillagerSchedule>() || !e.has<tilemap::GridPosition>()) return ai::BTStatus::Failure;

		const auto& schedule = e.get<VillagerSchedule>();
		const int hour = e.world().has<GameTime>() ? e.world().get<GameTime>().hour : 0;
		const glm::ivec2 target = SelectWaypoint(schedule, hour);

		auto& gp = e.get_mut<tilemap::GridPosition>();
		if (gp.x == target.x && gp.z == target.y) return ai::BTStatus::Success;

		// Move one step per BT tick toward target.
		const int dx = (target.x > gp.x) ? 1 : (target.x < gp.x) ? -1 : 0;
		const int dz = (target.y > gp.z) ? 1 : (target.y < gp.z) ? -1 : 0;
		gp.x += dx;
		gp.z += dz;
		return ai::BTStatus::Running;
	};
}

// ── Dialog tree builders ──────────────────────────────────────────────────────

// Build a simple greeting tree: "Hello!" → end.
static flecs::entity BuildGreetingTree(
	const flecs::world& world,
	flecs::entity parent,
	const std::string& speaker,
	const std::string& greeting,
	const std::string& treeName
) {
	dialog::DialogBuilder b;
	b.Node("greet").Line(speaker, greeting).Label("Greeting");
	b.Node("greet").Child("bye").End().Label("End");
	return b.Build(world, treeName, parent);
}

// Build a shopkeeper tree with buy-seeds and sell-crops branches.
// sell_items: items the shop sells (player buys), each entry {"typeId", "price", "count"}
// buy_items:  items the shop buys  (player sells), each entry {"typeId", "price"}
static flecs::entity BuildShopkeeperTree(
	const flecs::world& world,
	flecs::entity parent,
	const std::string& speaker,
	const std::string& greeting,
	const json& sellItems,
	const json& buyItems,
	const std::string& treeName
) {
	dialog::DialogBuilder b;
	b.Node("greet").Line(speaker, greeting).Label("Greeting");

	int choiceOrder = 0;

	// Buy branches (one per sell_item entry)
	for (const auto& item : sellItems) {
		const auto typeId = item.value("typeId", "wheat_seed");
		const auto price = item.value("price", 10);
		const auto count = item.value("count", 1);
		const std::string nodeId = "buy_" + typeId;
		const std::string confirmId = "buy_confirm_" + typeId;
		const std::string doneId = "buy_done_" + typeId;

		// Choice edge on the greeting node
		b.Node("greet").Child(nodeId);
		b.Node(nodeId)
			.Choice("Buy " + typeId + " (" + std::to_string(price) + " coins)", choiceOrder++)
			.Label("Buy " + typeId);

		// Deduct coins and give seeds
		b.Node(nodeId).Child(confirmId);
		b.Node(confirmId).TakeCoins(price).Label("Deduct coins for " + typeId);
		b.Node(confirmId).Child(doneId);
		b.Node(doneId).GiveItem(typeId, count).Label("Give " + typeId);
		b.Node(doneId).Child("end");
	}

	// Sell branches (one per buy_item entry)
	for (const auto& entry : buyItems) {
		const auto typeId = entry.value("typeId", "wheat");
		const auto price = entry.value("price", 8);
		const std::string nodeId = "sell_" + typeId;
		const std::string confirmId = "sell_confirm_" + typeId;
		const std::string doneId = "sell_done_" + typeId;

		b.Node("greet").Child(nodeId);
		b.Node(nodeId)
			.Choice("Sell " + typeId + " (+" + std::to_string(price) + " coins)", choiceOrder++)
			.Label("Sell " + typeId);

		b.Node(nodeId).Child(confirmId);
		b.Node(confirmId).TakeItem(typeId, 1).Label("Take " + typeId + " from player");
		b.Node(confirmId).Child(doneId);
		b.Node(doneId).GiveCoins(price).Label("Give coins for " + typeId);
		b.Node(doneId).Child("end");
	}

	// Goodbye branch
	b.Node("greet").Child("bye");
	b.Node("bye").Choice("Goodbye", choiceOrder).Label("Goodbye");
	b.Node("bye").Child("end");
	b.Node("end").End().Label("End");

	return b.Build(world, treeName, parent);
}

// ── BT tree builder ───────────────────────────────────────────────────────────

static ai::BehaviorTree MakeVillagerBtTree() {
	ai::BehaviorTree bt;
	const auto root = bt.tree.AddNode({.type = ai::BTNodeType::Repeater, .repeat_count = -1});
	bt.tree.AddNode({.type = ai::BTNodeType::Action, .handler_key = "villager_waypoint"}, root);
	return bt;
}

// ── Internal spawn helper ─────────────────────────────────────────────────────

struct VillagerSpec {
	std::string id;
	std::string name;
	std::string archetype;
	VillagerSchedule schedule;
	std::string dialogSpeaker;
	std::string dialogGreeting;
	json shopSellItems; // only for "shopkeeper"
	json shopBuyItems;
};

static flecs::entity SpawnOneVillager(flecs::world& world, flecs::entity sceneRoot, const VillagerSpec& spec) {
	const auto e = world.entity(spec.id.c_str()).child_of(sceneRoot);

	e.add<VillagerTag>();
	e.set<VillagerName>({.name = spec.name});
	e.set<VillagerSchedule>(spec.schedule);
	e.add<Interactable>();
	e.add<FarmInteractable>();

	// Spawn at first waypoint position.
	const glm::ivec2 startWp = spec.schedule.entries.empty() ? glm::ivec2{0, 0} : spec.schedule.entries[0].waypoint;
	e.set<tilemap::GridPosition>({.x = startWp.x, .z = startWp.y});

	// Attach a behavior tree for waypoint movement.
	e.set<ai::Blackboard>({});
	e.set<ai::BehaviorTree>(MakeVillagerBtTree());

	// Build and attach the dialog tree. A villager becomes a shopkeeper-style vendor whenever it
	// carries shop items (data-driven: new vendor archetypes need no C++ change), which the
	// "shopkeeper" archetype always does.
	const std::string treeName = spec.id + "_dialog";
	const bool isVendor = spec.archetype == "shopkeeper" || !spec.shopSellItems.empty() || !spec.shopBuyItems.empty();
	flecs::entity dialogRoot;
	if (isVendor) {
		dialogRoot = BuildShopkeeperTree(
			world,
			sceneRoot,
			spec.dialogSpeaker,
			spec.dialogGreeting,
			spec.shopSellItems,
			spec.shopBuyItems,
			treeName
		);
	}
	else {
		dialogRoot = BuildGreetingTree(world, sceneRoot, spec.dialogSpeaker, spec.dialogGreeting, treeName);
	}
	e.set<DialogTrigger>({.tree = dialogRoot});

	return e;
}

// ── InteractWith → StartDialog observer ──────────────────────────────────────

static void RegisterVillagerInteractObserver(const flecs::world& world, flecs::entity sceneRoot) {
	auto obs = world.observer("VillagerDialogOpener")
				   .event<InteractWith>()
				   .with<Interactable>()
				   .with<VillagerTag>()
				   .run([](flecs::iter& it) {
					   while (it.next()) {
						   for (size_t i = 0; i < it.count(); ++i) {
							   const flecs::entity villager = it.entity(i);
							   if (!villager.has<DialogTrigger>()) continue;
							   const auto& trigger = villager.get<DialogTrigger>();
							   if (!trigger.tree.is_valid()) continue;
							   if (it.world().has<dialog::DialogActive>()) continue; // dialog already open
							   // Find the player entity to use as actor.
							   flecs::entity actor;
							   it.world().query_builder<const PlayerInventory>().with<PlayerControlled>().build().each(
								   [&actor](flecs::entity e, const PlayerInventory&) {
									   if (!actor.is_valid()) actor = e;
								   }
							   );
							   if (!actor.is_valid()) actor = villager; // fallback
							   dialog::StartDialog(it.world(), trigger.tree, actor);
						   }
					   }
				   });
	obs.child_of(sceneRoot);
}

// ── JSON loading ──────────────────────────────────────────────────────────────

static std::vector<VillagerSpec> LoadVillagerSpecs(const json& doc, glm::ivec2 townLocation) {
	std::vector<VillagerSpec> specs;
	try {
		for (const auto& v : doc.at("villagers")) {
			VillagerSpec spec;
			spec.id = v.value("id", "");
			spec.name = v.value("name", spec.id);
			spec.archetype = v.value("archetype", "farmer");

			if (const auto& d = v.value("dialog", json::object()); !d.is_null()) {
				spec.dialogSpeaker = d.value("speaker", spec.name);
				spec.dialogGreeting = d.value("greeting", "...");
			}

			for (const auto& entry : v.at("schedule")) {
				const auto offset = entry.at("offset");
				const int dx = offset[0].get<int>();
				const int dz = offset[1].get<int>();
				spec.schedule.entries.push_back({
					.startHour = entry.value("startHour", 6),
					.waypoint = {townLocation.x + dx, townLocation.y + dz},
				});
			}

			if (spec.archetype == "shopkeeper" && v.contains("shop")) {
				const auto& shop = v.at("shop");
				spec.shopSellItems = shop.value("sell_items", json::array());
				spec.shopBuyItems = shop.value("buy_items", json::array());
			}

			if (!spec.id.empty() && !spec.schedule.entries.empty()) specs.push_back(std::move(spec));
		}
		spdlog::info("[VillagerSystem] Loaded {} villager spec(s) from JSON", specs.size());
	}
	catch (const std::exception& ex) {
		spdlog::warn("[VillagerSystem] Failed to parse villagers.json: {}", ex.what());
	}
	return specs;
}

// ── Public API ────────────────────────────────────────────────────────────────

int TownVillagerCount(const int population, const double perCapita, const int minCount, const int maxCount) {
	if (population <= 0) return 0;
	const int raw = static_cast<int>(std::lround(static_cast<double>(population) * perCapita));
	return std::clamp(raw, minCount, maxCount);
}

void RegisterVillagerLoaders(const flecs::world& /*world*/) {
	// No level JSON loaders needed: villagers are spawned procedurally from villagers.json,
	// not from level files. This hook is reserved for future authoring workflows.
}

void RegisterVillagerSystems(flecs::world& world, flecs::entity sceneRoot) {
	// Ensure dialog module is available (idempotent).
	world.import<dialog::DialogModule>();

	RegisterDialogInventoryHandlers(world);
	RegisterVillagerBtHandlers(world);
	RegisterVillagerInteractObserver(world, sceneRoot);
}

// ── Archetype catalog parsing (ticket 09) ─────────────────────────────────────

static std::vector<VillagerShopItem> ParseShopItems(const json& arr) {
	std::vector<VillagerShopItem> items;
	if (!arr.is_array()) return items;
	for (const auto& it : arr) {
		items.push_back({
			.typeId = it.value("typeId", std::string{}),
			.price = it.value("price", 0),
			.count = it.value("count", 1),
		});
	}
	return items;
}

static json ShopItemsToJson(const std::vector<VillagerShopItem>& items) {
	json arr = json::array();
	for (const auto& it : items) {
		arr.push_back({{"typeId", it.typeId}, {"price", it.price}, {"count", it.count}});
	}
	return arr;
}

VillagerCatalog ParseVillagerCatalog(const json& doc) {
	VillagerCatalog catalog;
	try {
		if (doc.contains("name_pool")) {
			for (const auto& n : doc.at("name_pool")) {
				if (n.is_string()) catalog.namePool.push_back(n.get<std::string>());
			}
		}
		if (const auto tv = doc.value("town_villagers", json::object()); tv.is_object()) {
			catalog.perCapita = tv.value("perCapita", catalog.perCapita);
			catalog.minPerTown = tv.value("min", catalog.minPerTown);
			catalog.maxPerTown = tv.value("max", catalog.maxPerTown);
		}
		if (doc.contains("archetypes")) {
			for (const auto& a : doc.at("archetypes")) {
				VillagerArchetype arche;
				arche.id = a.value("id", std::string{});
				arche.weight = a.value("weight", 1);
				arche.buildingKind = a.value("buildingKind", std::string{});
				if (const auto d = a.value("dialog", json::object()); d.is_object()) {
					arche.dialogGreeting = d.value("greeting", std::string{"..."});
				}
				for (const auto& slot : a.value("schedule", json::array())) {
					arche.schedule.push_back({
						.startHour = slot.value("startHour", 6),
						.place = slot.value("place", std::string{"home"}),
					});
				}
				if (a.contains("shop")) {
					const auto& shop = a.at("shop");
					arche.sellItems = ParseShopItems(shop.value("sell_items", json::array()));
					arche.buyItems = ParseShopItems(shop.value("buy_items", json::array()));
				}
				if (!arche.id.empty()) catalog.archetypes.push_back(std::move(arche));
			}
		}
	}
	catch (const std::exception& ex) {
		spdlog::warn("[VillagerSystem] Failed to parse villager catalog: {}", ex.what());
	}
	return catalog;
}

static const VillagerArchetype* FindArchetype(const VillagerCatalog& catalog, const std::string& id) {
	for (const auto& a : catalog.archetypes) {
		if (a.id == id) return &a;
	}
	return nullptr;
}

// Minimal built-in archetype used when a building's archetype id is missing from the catalog, so a
// town is never left with an unstaffed building or a villager without a routine.
static VillagerArchetype DefaultArchetype(const std::string& id) {
	VillagerArchetype a;
	a.id = id;
	a.dialogGreeting = "...";
	a.schedule = {{.startHour = 7, .place = "work"}, {.startHour = 20, .place = "home"}};
	return a;
}

// ── Procedural per-town planning (ticket 09) ──────────────────────────────────

std::vector<PlannedVillager>
PlanTownVillagers(const int worldSeed, const TownSite& town, const TownLayout* layout, const VillagerCatalog& catalog) {
	std::vector<PlannedVillager> planned;

	// Placement anchors from the town layout: house doors (homes), per-building archetype
	// workplaces, decorative field tiles, and a town-centre fallback.
	std::vector<glm::ivec2> houseDoors;
	std::vector<std::pair<std::string, glm::ivec2>> staffed; // (archetype id, workplace door)
	std::vector<glm::ivec2> fieldTiles;
	glm::ivec2 centre = town.location;
	if (layout != nullptr) {
		centre = layout->landmark;
		fieldTiles = layout->fieldTiles;
		for (const TownBuilding& b : layout->buildings) {
			if (b.ruined) continue;
			if (b.isHome) houseDoors.push_back(b.door);
			if (!b.villagerArchetype.empty()) staffed.emplace_back(b.villagerArchetype, b.door);
		}
	}

	// Deterministic per-town RNG, salted so it does not mirror the town-layout seed stream.
	std::mt19937 rng(DeriveTownSeed(worldSeed, town.id) ^ 0x56494C4Cu);

	std::vector<std::string> names = catalog.namePool;
	Shuffle(names.begin(), names.end(), rng);
	std::size_t nameIdx = 0;
	const auto nextName = [&]() -> std::string {
		if (names.empty()) return "Villager";
		return names[(nameIdx++) % names.size()];
	};

	std::size_t houseIdx = 0;
	const auto nextHome = [&]() -> glm::ivec2 {
		if (houseDoors.empty()) return centre;
		return houseDoors[(houseIdx++) % houseDoors.size()];
	};

	std::size_t fieldIdx = 0;
	const auto nextField = [&]() -> glm::ivec2 {
		if (fieldTiles.empty()) return centre;
		return fieldTiles[(fieldIdx++) % fieldTiles.size()];
	};

	const auto resolve = [](const VillagerArchetype& a, glm::ivec2 home, glm::ivec2 work) {
		VillagerSchedule sched;
		for (const VillagerScheduleSlot& slot : a.schedule) {
			sched.entries.push_back({.startHour = slot.startHour, .waypoint = (slot.place == "home") ? home : work});
		}
		// Guarantee a meaningful (>= 2 entry) routine even for a malformed archetype.
		if (sched.entries.size() < 2) {
			sched.entries = {{.startHour = 7, .waypoint = work}, {.startHour = 20, .waypoint = home}};
		}
		return sched;
	};

	int index = 0;
	const auto add = [&](const VillagerArchetype& a, glm::ivec2 home, glm::ivec2 work) {
		PlannedVillager pv;
		pv.id = "villager_t" + std::to_string(town.id) + "_" + std::to_string(index++);
		pv.archetype = a.id;
		pv.name = nextName();
		pv.schedule = resolve(a, home, work);
		planned.push_back(std::move(pv));
	};

	// 1) Staff every archetype-bound building (shop -> shopkeeper, hall -> elder); workplace = door.
	for (const auto& [archId, door] : staffed) {
		const VillagerArchetype* a = FindArchetype(catalog, archId);
		const VillagerArchetype fallback = DefaultArchetype(archId);
		add(a != nullptr ? *a : fallback, nextHome(), door);
	}

	// 2) Fill with generic villagers up to the population-scaled target, homed at a house door and
	//    working a field (or the town centre if the town has no fields).
	const int target = TownVillagerCount(town.size, catalog.perCapita, catalog.minPerTown, catalog.maxPerTown);
	const VillagerArchetype* farmer = FindArchetype(catalog, "farmer");
	const VillagerArchetype farmerFallback = DefaultArchetype("farmer");
	const VillagerArchetype& fill = farmer != nullptr ? *farmer : farmerFallback;
	while (static_cast<int>(planned.size()) < target) {
		const glm::ivec2 home = nextHome();
		add(fill, home, nextField());
	}

	// Every living town has at least one inhabitant.
	if (planned.empty()) add(fill, centre, centre);

	return planned;
}

// ── Multi-town spawning (ticket 09) ───────────────────────────────────────────

static const TownLayout* FindTownLayout(const WorldData& data, int townId) {
	for (const TownLayout& l : data.townLayouts) {
		if (l.townId == townId) return &l;
	}
	return nullptr;
}

static int SpawnProceduralTownVillagers(
	flecs::world& world,
	flecs::entity townRoot,
	int worldSeed,
	const TownSite& town,
	const TownLayout* layout,
	const VillagerCatalog& catalog
) {
	const std::vector<PlannedVillager> planned = PlanTownVillagers(worldSeed, town, layout, catalog);
	for (const PlannedVillager& pv : planned) {
		const VillagerArchetype* a = FindArchetype(catalog, pv.archetype);
		VillagerSpec spec;
		spec.id = pv.id;
		spec.name = pv.name;
		spec.archetype = pv.archetype;
		spec.schedule = pv.schedule;
		spec.dialogSpeaker = pv.name;
		spec.dialogGreeting = (a != nullptr && !a->dialogGreeting.empty()) ? a->dialogGreeting : "...";
		spec.shopSellItems = (a != nullptr) ? ShopItemsToJson(a->sellItems) : json::array();
		spec.shopBuyItems = (a != nullptr) ? ShopItemsToJson(a->buyItems) : json::array();
		SpawnOneVillager(world, townRoot, spec);
	}
	return static_cast<int>(planned.size());
}

// Spawn villagers in every living town of `data`. The starting village keeps its authored villagers;
// every other living town gets procedural inhabitants bound to its generated buildings. Each town's
// villagers are parented to a per-town TownRoot entity so they are grouped by settlement.
static void SpawnVillagersFromDoc(
	flecs::world& world,
	flecs::entity sceneRoot,
	const WorldData& data,
	int startVillageTownId,
	const json& doc
) {
	const VillagerCatalog catalog = ParseVillagerCatalog(doc);
	int totalVillagers = 0;
	int livingTowns = 0;
	for (const TownSite& town : data.towns) {
		if (town.status != TownStatus::Living) continue;
		++livingTowns;

		const std::string rootName = "TownRoot_" + std::to_string(town.id);
		const flecs::entity townRoot = world.entity(rootName.c_str()).child_of(sceneRoot);
		townRoot.set<TownRoot>({.townId = town.id, .location = town.location});

		if (town.id == startVillageTownId) {
			// Preserve the authored starting-village villagers (unchanged behaviour).
			const auto specs = LoadVillagerSpecs(doc, glm::ivec2{town.location.x, town.location.y});
			for (const auto& spec : specs) {
				SpawnOneVillager(world, townRoot, spec);
				++totalVillagers;
			}
		}
		else {
			totalVillagers +=
				SpawnProceduralTownVillagers(world, townRoot, data.seed, town, FindTownLayout(data, town.id), catalog);
		}
	}
	engine::ecs::Log(world).info(
		"[VillagerSystem] Spawned {} villager(s) across {} living town(s)",
		totalVillagers,
		livingTowns
	);
}

void SpawnVillagers(flecs::world& world, flecs::entity sceneRoot, const WorldData& data, int startVillageTownId) {
	const auto* ref = world.try_get<platform::PlatformRef>();
	const std::string path = (ref && ref->ptr) ? (ref->ptr->AssetDirectory() + "/data/villagers.json") : "";

	if (!path.empty()) {
		if (const auto doc = core::LoadJsonFile(path, "VillagerSystem")) {
			SpawnVillagersFromDoc(world, sceneRoot, data, startVillageTownId, *doc);
			return;
		}
	}

	engine::ecs::Log(world).warn("[VillagerSystem] villagers.json unavailable; using inline headless catalog");
	SpawnVillagersHeadless(world, sceneRoot, data, startVillageTownId);
}

void SpawnVillagersHeadless(
	flecs::world& world,
	flecs::entity sceneRoot,
	const WorldData& data,
	int startVillageTownId
) {
	// Inline JSON matching games/farm/assets/data/villagers.json exactly — single source of truth
	// for headless contexts (tests, CI) that have no file I/O.
	static const std::string kVillagersJson = R"({
		"villagers": [
			{
				"id": "farmer_oswald", "name": "Oswald", "archetype": "farmer",
				"schedule": [
					{ "startHour": 6,  "offset": [2, 3] },
					{ "startHour": 12, "offset": [3, 5] },
					{ "startHour": 20, "offset": [2, 4] }
				],
				"dialog": { "speaker": "Oswald", "greeting": "Good morning! Hard work feeds the village." }
			},
			{
				"id": "shopkeeper_mira", "name": "Mira", "archetype": "shopkeeper",
				"schedule": [
					{ "startHour": 8,  "offset": [4, 2] },
					{ "startHour": 20, "offset": [4, 1] }
				],
				"dialog": { "speaker": "Mira", "greeting": "Welcome to Mira's! What can I do for you?" },
				"shop": {
					"sell_items": [
						{ "typeId": "wheat_seed",  "price": 10, "count": 5 },
						{ "typeId": "tomato_seed", "price": 15, "count": 5 },
						{ "typeId": "carrot_seed", "price": 12, "count": 5 }
					],
					"buy_items": [
						{ "typeId": "wheat",  "price": 8 },
						{ "typeId": "tomato", "price": 12 },
						{ "typeId": "carrot", "price": 10 }
					]
				}
			},
			{
				"id": "elder_aldric", "name": "Aldric", "archetype": "elder",
				"schedule": [
					{ "startHour": 9,  "offset": [-2, 2] },
					{ "startHour": 16, "offset": [-3, 1] },
					{ "startHour": 21, "offset": [-1, -1] }
				],
				"dialog": { "speaker": "Aldric", "greeting": "This village has stood for generations. May it stand for many more." }
			}
		],
		"name_pool": [
			"Rowan", "Mabel", "Cedric", "Odette", "Garrick", "Linnea", "Tobias", "Wrenna",
			"Alden", "Peony", "Harlow", "Isolde", "Bram", "Fenna", "Osric", "Greta",
			"Caddock", "Nessa", "Dorian", "Maren", "Edric", "Sorrel", "Perrin", "Yara",
			"Hollis", "Ingrid", "Merrick", "Dahlia", "Corin", "Elspeth", "Rafe", "Wyn"
		],
		"town_villagers": { "perCapita": 0.04, "min": 1, "max": 8 },
		"archetypes": [
			{
				"id": "farmer", "weight": 5, "buildingKind": "",
				"dialog": { "greeting": "The fields keep us all fed. Fair weather to you." },
				"schedule": [
					{ "startHour": 6,  "place": "work" },
					{ "startHour": 13, "place": "work" },
					{ "startHour": 20, "place": "home" }
				]
			},
			{
				"id": "shopkeeper", "weight": 2, "buildingKind": "shop",
				"dialog": { "greeting": "Looking to trade? You've come to the right counter." },
				"schedule": [
					{ "startHour": 8,  "place": "work" },
					{ "startHour": 20, "place": "home" }
				],
				"shop": {
					"sell_items": [
						{ "typeId": "wheat_seed",  "price": 10, "count": 5 },
						{ "typeId": "carrot_seed", "price": 12, "count": 5 }
					],
					"buy_items": [
						{ "typeId": "wheat",  "price": 8 },
						{ "typeId": "carrot", "price": 10 }
					]
				}
			},
			{
				"id": "elder", "weight": 1, "buildingKind": "hall",
				"dialog": { "greeting": "Every town has a story. Ours is still being written." },
				"schedule": [
					{ "startHour": 9,  "place": "work" },
					{ "startHour": 18, "place": "home" }
				]
			}
		]
	})";

	try {
		SpawnVillagersFromDoc(world, sceneRoot, data, startVillageTownId, json::parse(kVillagersJson));
	}
	catch (const std::exception& ex) {
		engine::ecs::Log(world).error("[VillagerSystem] Failed to parse headless villagers JSON: {}", ex.what());
	}
}

} // namespace game::farm
