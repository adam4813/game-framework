#include "radio_systems.hpp"

#include <algorithm>
#include <format>
#include <string_view>

#include "engine/engine.hpp"
#include "palette.hpp"
#include "placements.hpp"
#include "radio_components.hpp"
#include "room_components.hpp"
#include "room_constants.hpp"
#include "room_tuning.hpp"
#include "ticket_components.hpp"
#include "ticket_rules.hpp"
#include "station_components.hpp"

using namespace engine;

namespace signal_room {

namespace {

// A dark Radio's body and display keep this fraction of their frequency colour.
constexpr float kLabelFontSize = 13.0F;
// The part names the greybox catalogue gives a Radio's display and traffic lamp.
constexpr const char* kDisplayPart = "display";
constexpr const char* kLampPart = "lamp";
constexpr const char* kLampOffToken = "lamp_off";

bool OnAir(const flecs::query<const Ticket>& tickets, const std::string& frequency) {
	return tickets.find([&](const Ticket& ticket) {
		return ticket.state == TicketState::OnAir && ticket.frequency == frequency;
	});
}

void Raise(const flecs::world& world, const auto& event, const flecs::entity radio) {
	world.event<std::remove_cvref_t<decltype(event)>>().template id<Radio>().entity(radio).emit();
}

void RegisterCouplerReset(const flecs::world& world, const flecs::entity roomRoot) {
	world.system<AntennaCoupler>("CouplerResetTick")
		.term_at(0)
		.singleton()
		.kind(flecs::OnUpdate)
		.each([](flecs::iter& it, size_t, AntennaCoupler& coupler) {
			coupler.resetLeft = std::max(0.0F, coupler.resetLeft - it.delta_time());
		})
		.child_of(roomRoot);
}

void RegisterOnlineState(const flecs::world& world, const flecs::entity roomRoot) {
	world.system<Radio>("RadioOnlineState")
		.kind(flecs::OnUpdate)
		.each([tickets = world.query<const Ticket>()](flecs::iter& it, const size_t i, Radio& radio) {
			const auto& coupler = it.world().get<AntennaCoupler>();
			const auto self = it.entity(i);
			const bool online =
				OnAir(tickets, radio.frequency)
				&& (coupler.coupled == 0 || coupler.coupled == self.id())
				&& coupler.resetLeft <= 0.0F;
			if (online && !radio.online) {
				Raise(it.world(), RadioOnline{}, self);
			}
			radio.online = online;
		})
		.child_of(roomRoot);
}

// The Hold ending (release, move, dash, or the copy completing) lets the coupler go and starts the reset.
void RegisterDisconnect(const flecs::world& world, const flecs::entity roomRoot) {
	world.observer<const Holding>("CouplerDisconnect")
		.event(flecs::OnRemove)
		.each([](flecs::iter& it, size_t, const Holding& holding) {
			const auto world = it.world();
			if (!world.has<AntennaCoupler>() || holding.station == 0) {
				return;
			}
			auto& coupler = world.get_mut<AntennaCoupler>();
			if (coupler.coupled != holding.station) {
				return;
			}
			coupler.coupled = 0;
			coupler.resetLeft = world.get<CouplerTuning>().couplerReset;
			Raise(world, CouplerReleased{}, world.entity(holding.station));
		})
		.child_of(roomRoot);
}

// The traffic lamp lights in the frequency colour while the Radio is online; the Radio itself never changes.
void RegisterDimming(const flecs::world& world, const flecs::entity roomRoot) {
	world.system<const Radio>("RadioLamp")
		.kind(flecs::PreStore)
		.each([](const flecs::entity radio, const Radio& state) {
			const auto* bright = FrequencyColour(radio.world(), state.frequency);
			const auto* off = radio.world().get<Palette>().Get(kLampOffToken);
			const auto part = radio.lookup(kLampPart);
			if (bright == nullptr || off == nullptr || !part || !part.has<PartColour>()) {
				return;
			}
			const auto colour = state.online ? *bright : *off;
			part.get_mut<PartColour>().colour = colour;
			if (!radio.has<Brightened>()) {
				part.get_mut<render::Material>().color = colour;
			}
		})
		.child_of(roomRoot);
}

// The frequency label, drawn over the Radio's display part in its colour (dimmed while dark).
void RegisterLabels(const flecs::world& world, const flecs::entity roomRoot) {
	world.system<const Radio>("RadioLabel")
		.kind<ecs::OnUI>()
		.each([cameras = world.query<const spatial::WorldTransform, const render::Camera>()](
				  flecs::iter& it, const size_t i, const Radio& state) {
			const auto world = it.world();
			auto* platform = world.get<EngineContextRef>().ptr->GetPlatform();
			const auto* base = FrequencyColour(world, state.frequency);
			const auto display = it.entity(i).lookup(kDisplayPart);
			const auto camera = cameras.first();
			if (platform == nullptr || base == nullptr || !display || !camera || !display.has<spatial::WorldTransform>()) {
				return;
			}
			const auto screen = render::ScreenProjector::FromCamera(
											camera,
											static_cast<float>(platform->Width()),
											static_cast<float>(platform->Height())
			).Project(display.get<spatial::WorldTransform>().position);
			const float width = platform->MeasureText(state.frequency, kLabelFontSize);
			// Always the bright frequency colour, drawn four times a pixel apart for a bold neon read.
			for (const auto [dx, dy] : {std::pair{0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 1.0F}, {1.0F, 1.0F}}) {
				platform->DrawText(state.frequency, screen.x - width * 0.5F + dx, screen.y - kLabelFontSize * 0.5F + dy, kLabelFontSize, *base);
			}
		})
		.child_of(roomRoot);
}

} // namespace

const core::Rgba* FrequencyColour(const flecs::world& world, const std::string& label) {
	const auto* frequency = world.get<FrequencyTable>().Find(label);
	return frequency == nullptr ? nullptr : world.get<Palette>().Get(frequency->colour);
}

const core::Rgba* FrequencyBodyColour(const flecs::world& world, const std::string& label) {
	const auto* frequency = world.get<FrequencyTable>().Find(label);
	if (frequency == nullptr) {
		return nullptr;
	}
	const auto* body = world.get<Palette>().Get(frequency->colour + "_body");
	return body != nullptr ? body : world.get<Palette>().Get(frequency->colour);
}

std::expected<void, std::string>
AssignRadios(const flecs::world& world, const flecs::entity parent, const Shift& shift, const std::string& shiftPath) {
	world.set<AntennaCoupler>({});
	const auto stations = PlacementsOf<Station>(world, parent);
	const auto find = [&](const std::string& name) {
		const auto found = std::ranges::find_if(stations, [&](const auto& placement) {
			return std::string_view{placement.first.name().c_str()} == name;
		});
		return found == stations.end() ? flecs::entity{} : found->first;
	};

	for (const auto& [name, frequency] : shift.radios) {
		if (!find(name)) {
			return std::unexpected(
				std::format("Shift file {}: radios.{} names no Station in the Layout", shiftPath, name)
			);
		}
	}
	for (const auto& [e, station] : stations) {
		if (station.kind != kRadioKind || e.has<Tarped>()) {
			continue;
		}
		const auto name = std::string{e.name().c_str()};
		const auto tuned = std::ranges::find(shift.radios, name, &std::pair<std::string, std::string>::first);
		if (tuned == shift.radios.end()) {
			return std::unexpected(std::format("Shift file {}: in-service Radio '{}' has no frequency in radios", shiftPath, name));
		}
		e.set<Radio>({.frequency = tuned->second});
	}
	return {};
}

void EngageCoupler(const flecs::world& world, const flecs::entity radio) {
	world.get_mut<AntennaCoupler>().coupled = radio.id();
	Raise(world, CouplerEngaged{}, radio);
}

void RegisterRadioSystems(const flecs::world& world, const flecs::entity roomRoot) {
	RegisterCouplerReset(world, roomRoot);
	RegisterOnlineState(world, roomRoot);
	RegisterDisconnect(world, roomRoot);
	RegisterDimming(world, roomRoot);
	RegisterLabels(world, roomRoot);
}

} // namespace signal_room
