#include "signal_room_module.hpp"

#include <memory>
#include <string>


#include "engine/engine.hpp"
#include "room/bindings.hpp"
#include "room/greybox_catalogue.hpp"
#include "room/guidance.hpp"
#include "room/hud.hpp"
#include "room/hud_layout.hpp"
#include "room/item_catalogue.hpp"
#include "room/item_components.hpp"
#include "room/layout.hpp"
#include "room/modifiers.hpp"
#include "room/operator_components.hpp"
#include "room/palette.hpp"
#include "room/radio_components.hpp"
#include "room/room_components.hpp"
#include "room/room_scene.hpp"
#include "room/room_tuning.hpp"
#include "room/score_components.hpp"
#include "room/shift_flow.hpp"
#include "room/shift.hpp"
#include "room/station_catalogue.hpp"
#include "room/station_components.hpp"
#include "room/ticket_components.hpp"
#include "room/ticket_rules.hpp"

using namespace engine;

namespace signal_room {

namespace {

// Every game component is registered here, before any system or load can reach it lazily (a lazy
// registration inside a readonly multi-stage world crashes; see CODING_STANDARDS.md).
void RegisterComponents(const flecs::world& world) {
	world.component<glm::ivec2>("ivec2").member<int>("x").member<int>("y");
	world.component<Sides>()
		.bit("north", Sides::North)
		.bit("east", Sides::East)
		.bit("south", Sides::South)
		.bit("west", Sides::West);

	world.component<RoomSceneTag>();
	world.component<FloorTile>();
	world.component<Tarped>();
	world.component<FloorGrid>()
		.member<int>("width")
		.member<int>("height")
		.member<float>("cell_size")
		.member<glm::vec3>("origin")
		.add(EcsSingleton);
	world.component<RoomState>().member<std::string>("shift_path").member<std::string>("load_error").add(EcsSingleton);
	world.component<RoomOverlays>()
		.member<bool>("floor_grid")
		.member<bool>("solid_cells")
		.member<bool>("worked_sides")
		.member<bool>("operator_circle")
		.member<bool>("targeting_reach")
		.add(EcsSingleton);
	world.component<RoomOptions>().member<bool>("auto_grab").add(EcsSingleton);
	world.component<ModifierToggle>().add(EcsSingleton);
	world.component<AppliedModifiers>().member<std::string>("ids").add(EcsSingleton);
	world.component<ModifierCatalogue>().add(EcsSingleton);
	world.component<LayoutErrors>().add(EcsSingleton);
	world.component<Palette>().add(EcsSingleton);
	world.component<HudLayout>().add(EcsSingleton);
	world.component<HudPulse>().member<float>("score").member<float>("chain").add(EcsSingleton);
	world.component<HudCard>();
	world.component<HudGauge>().member<float>("on_air").member<float>("standby").member<std::string>("band");
	world.component<HudClock>();
	world.component<HudClockTag>();
	world.component<HudScore>();
	world.component<HudChain>();
	world.component<GreyboxCatalogue>().add(EcsSingleton);

	world.component<Station>()
		.member<std::string>("kind")
		.member<glm::ivec2>("cell")
		.member<glm::ivec2>("footprint")
		.member<Sides>("worked_sides");
	world.component<Counter>().member<glm::ivec2>("cell");
	world.component<Wall>()
		.member<glm::ivec2>("cell")
		.member<glm::ivec2>("footprint")
		.member<std::string>("height_class");
	world.component<EdgeBlock>().member<glm::ivec2>("cell").member<glm::ivec2>("footprint");
	world.component<Prop>()
		.member<std::string>("kind")
		.member<glm::ivec2>("cell")
		.member<glm::ivec2>("footprint")
		.member<bool>("solid")
		.member<float>("height");
	world.component<SpawnMarker>().member<glm::ivec2>("cell");
	world.component<PartColour>().member<core::Rgba>("colour");
	world.component<Brightened>();

	world.component<MovementTuning>()
		.member<float>("walk_speed")
		.member<float>("accel_time")
		.member<float>("decel_time")
		.member<float>("turn_time")
		.member<float>("dash_multiplier")
		.member<float>("dash_duration")
		.member<float>("dash_cooldown")
		.member<float>("radius")
		.add(EcsSingleton);
	world.component<TargetingTuning>()
		.member<float>("ray_length")
		.member<float>("cone_range")
		.member<float>("cone_half_angle")
		.member<float>("brighten_rate")
		.member<float>("brighten_strength")
		.member<float>("brighten_lift")
		.add(EcsSingleton);
	world.component<DustPuffTuning>()
		.member<float>("count")
		.member<float>("speed")
		.member<float>("lifetime")
		.member<float>("size")
		.member<float>("spread")
		.add(EcsSingleton);
	world.component<CouplerTuning>().member<float>("coupler_reset").add(EcsSingleton);
	world.component<ConfettiTuning>()
		.member<float>("count")
		.member<float>("speed")
		.member<float>("lifetime")
		.member<float>("size")
		.member<float>("spread")
		.add(EcsSingleton);
	world.component<ReadTuning>()
		.member<float>("blink_slow")
		.member<float>("blink_quick")
		.member<float>("blink_min")
		.member<float>("blink_max")
		.member<float>("reel_speed")
		.member<float>("reel_cranked_speed")
		.member<float>("crank_speed")
		.member<float>("telex_flash")
		.member<float>("telex_slide")
		.member<float>("dark_dim")
		.add(EcsSingleton);
	world.component<GuidanceTuning>()
		.member<float>("chevron_height")
		.member<float>("chevron_bob")
		.member<float>("chevron_bob_rate")
		.member<float>("chevron_spacing")
		.member<float>("bubble_duration")
		.member<float>("bubble_side")
		.member<float>("bubble_height")
		.add(EcsSingleton);
	world.component<Chevron>().member<flecs::entity_t>("operator").member<flecs::entity_t>("station").member<float>("age");
	world.component<Bubble>().member<flecs::entity_t>("operator").member<std::string>("glyph").member<float>("time_left");
	world.component<ReadState>()
		.constant("Idle", ReadState::Idle)
		.constant("Running", ReadState::Running)
		.constant("Done", ReadState::Done)
		.constant("OnAir", ReadState::OnAir)
		.constant("Coupled", ReadState::Coupled)
		.constant("Resetting", ReadState::Resetting)
		.constant("Dark", ReadState::Dark);
	world.component<StationRead>()
		.member<ReadState>("state")
		.member<float>("fill")
		.member<bool>("cranked")
		.member<float>("blink_phase")
		.member<float>("reel_angle")
		.member<float>("crank_angle")
		.member<float>("flash_left");
	world.component<PartHome>().member<glm::vec3>("position");
	world.component<Bindings>().add(EcsSingleton);
	world.component<ItemCatalogue>().add(EcsSingleton);
	world.component<Operator>().member<std::string>("colour");
	world.component<OperatorActions>()
		.member<glm::vec2>("move")
		.member<bool>("dash")
		.member<bool>("interact")
		.member<bool>("interact_held");
	world.component<OperatorMotion>().member<glm::vec2>("velocity").member<float>("facing");
	world.component<Dash>().member<glm::vec2>("direction").member<float>("time_left").member<float>("cooldown_left");
	world.component<Targets>().add(flecs::Exclusive);
	world.component<Item>()
		.member<std::string>("id")
		.member<std::string>("frequency")
		.member<float>("carry_speed")
		.member<bool>("tilt_to_camera");
	world.component<Carries>().add(flecs::Exclusive);
	world.component<Holds>();
	world.component<StationCatalogue>().add(EcsSingleton);
	world.component<JobPhase>()
		.constant("Idle", JobPhase::Idle)
		.constant("Loading", JobPhase::Loading)
		.constant("Running", JobPhase::Running)
		.constant("Done", JobPhase::Done);
	world.component<Job>()
		.member<JobPhase>("phase")
		.member<std::string>("transform")
		.member<float>("progress")
		.member<int>("taps");
	world.component<Holding>().member<flecs::entity_t>("station").member<float>("since");
	world.component<RefusalReason>()
		.constant("WrongItem", RefusalReason::WrongItem)
		.constant("Busy", RefusalReason::Busy)
		.constant("NothingToDo", RefusalReason::NothingToDo)
		.constant("OtherHolder", RefusalReason::OtherHolder)
		.constant("CounterFull", RefusalReason::CounterFull);
	world.component<Refusal>();
	world.component<Radio>().member<std::string>("frequency").member<bool>("online");
	world.component<AntennaCoupler>().member<flecs::entity_t>("coupled").member<float>("reset_left").add(EcsSingleton);
	world.component<CouplerEngaged>();
	world.component<CouplerReleased>();
	world.component<RadioOnline>();

	world.component<TicketState>().constant("StandBy", TicketState::StandBy).constant("OnAir", TicketState::OnAir);
	world.component<ShiftPhase>()
		.constant("Briefing", ShiftPhase::Briefing)
		.constant("Running", ShiftPhase::Running)
		.constant("Ended", ShiftPhase::Ended);
	world.component<Ticket>()
		.member<std::string>("callsign")
		.member<std::string>("tier")
		.member<std::string>("frequency")
		.member<int>("order")
		.member<TicketState>("state")
		.member<float>("standby_left")
		.member<float>("on_air_total")
		.member<float>("on_air_left")
		.member<bool>("unlosable");
	world.component<ShiftWindow>()
		.member<ShiftPhase>("phase")
		.member<float>("window")
		.member<float>("window_left")
		.member<uint64_t>("seed")
		.member<float>("next_arrival")
		.member<int>("next_order")
		.member<int>("next_pinned")
		.member<bool>("last_call")
		.member<bool>("frozen")
		.add(EcsSingleton);
	world.component<Score>()
		.member<int>("score")
		.member<int>("chain_step")
		.member<int>("par")
		.member<int>("cleared")
		.member<int>("lost")
		.member<int>("best_chain_step")
		.member<int>("stars")
		.add(EcsSingleton);
	world.component<StarRules>().add(EcsSingleton);
	world.component<ScoreReason>().constant("Clear", ScoreReason::Clear).constant("LostIntercept", ScoreReason::LostIntercept);
	world.component<ScoreChanged>();
	world.component<ChainChanged>();
	world.component<SeedPin>().member<bool>("pinned").member<uint64_t>("value").add(EcsSingleton);
	world.component<ScheduleRng>().add(EcsSingleton);
	world.component<ScheduleState>().add(EcsSingleton);
	world.component<Schedule>().add(EcsSingleton);
	world.component<TicketRules>().add(EcsSingleton);
	world.component<FrequencyTable>().add(EcsSingleton);
	world.component<TicketArrived>();
	world.component<TicketOnAir>();
	world.component<TicketCleared>();
	world.component<LostIntercept>();
	world.component<LastCall>();
	world.component<ShiftEnded>();
	world.component<ShiftPhaseChanged>();
	world.component<ShiftBriefing>().member<std::string>("name").add(EcsSingleton);
	world.component<FlowUi>().add(EcsSingleton);
	world.component<Transmission>();
}

} // namespace

SignalRoomModule::SignalRoomModule(flecs::world& world) {
	// Level, render, input and the debug UI are always imported by the EngineContext.
	world.get<EngineContextRef>().ptr->GetModuleRegistry().Import(world, {"particles", "pathfinding"});

	RegisterComponents(world);
	world.set<RoomState>({});
	world.set<RoomOverlays>({});
	world.set<RoomOptions>({});
	world.set<ModifierToggle>({});
	world.set<AppliedModifiers>({});
	world.set<SeedPin>({});
	// Defaults until the Room loads the tuning and bindings files.
	world.set<MovementTuning>({});
	world.set<TargetingTuning>({});
	world.set<DustPuffTuning>({});
	world.set<CouplerTuning>({});
	world.set<ConfettiTuning>({});
	world.set<ReadTuning>({});
	world.set<GuidanceTuning>({});
	world.set<AntennaCoupler>({});
	world.set<Bindings>({});
	RegisterLayoutLoaders(world);

	scene::RegisterScene<RoomSceneTag>(world, std::make_shared<RoomScene>());

	engine::ecs::Log(world).info("[SignalRoom] Module imported");
}

} // namespace signal_room
