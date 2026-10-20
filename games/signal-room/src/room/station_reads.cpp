#include "station_reads.hpp"

#include <algorithm>
#include <cmath>

#include "engine/engine.hpp"
#include "operator_components.hpp"
#include "palette.hpp"
#include "radio_components.hpp"
#include "radio_systems.hpp"
#include "room_components.hpp"
#include "room_constants.hpp"
#include "room_tuning.hpp"
#include "station_components.hpp"
#include "ticket_components.hpp"

using namespace engine;

namespace signal_room {

namespace {

// Part names the greybox catalogue gives the animated parts, and the palette tokens of the states.
constexpr const char* kLampPart = "lamp";
constexpr const char* kBarPart = "riser_bar";
constexpr const char* kBodyPart = "body";
constexpr const char* kLeftReelPart = "reel_l";
constexpr const char* kRightReelPart = "reel_r";
constexpr const char* kCrankPart = "crank";
constexpr const char* kPaperPart = "paper";
constexpr const char* kLampOff = "lamp_off";
constexpr const char* kLampAmber = "lamp_amber";
constexpr const char* kLampGreen = "lamp_green";
constexpr const char* kLampGrey = "lamp_grey";
constexpr const char* kBarTrack = "bar_track";
constexpr const char* kConfettiColourToken = "confetti";

// The empty bar keeps this sliver of fill so its transform never collapses.
constexpr float kMinFill = 0.001F;

constexpr core::Rgba kFallback{.r = 255, .g = 0, .b = 255, .a = 255};

core::Rgba Token(const Palette& palette, const char* token) {
	const auto* colour = palette.Get(token);
	return colour != nullptr ? *colour : kFallback;
}

core::Rgba Dim(const core::Rgba colour, const float keep) {
	const auto channel = [keep](const uint8_t c) { return static_cast<uint8_t>(std::lround(static_cast<float>(c) * keep)); };
	return {.r = channel(colour.r), .g = channel(colour.g), .b = channel(colour.b), .a = colour.a};
}

ReadState DeriveState(
	const flecs::world& world,
	const flecs::entity station,
	const Job& job,
	const flecs::query<const Ticket>& tickets
) {
	if (!station.has<Radio>()) {
		switch (job.phase) {
		case JobPhase::Running: return ReadState::Running;
		case JobPhase::Done: return ReadState::Done;
		default: return ReadState::Idle;
		}
	}
	if (job.phase == JobPhase::Done) {
		return ReadState::Done;
	}
	// A half-done copy stays Running after the Operator lets go; only the coupled Radio shows as coupled.
	const auto& coupler = world.get<AntennaCoupler>();
	if (coupler.coupled == station.id()) {
		return ReadState::Coupled;
	}
	const auto& radio = station.get<Radio>();
	if (radio.online) {
		return ReadState::OnAir;
	}
	const bool resetting = coupler.coupled == 0 && coupler.resetLeft > 0.0F;
	return resetting && FrequencyOnAir(tickets, radio.frequency) ? ReadState::Resetting : ReadState::Dark;
}

// Blinks per second the lamp runs at in `read`'s state (0 for a steady or unlit lamp).
float BlinkRate(const ReadState state, const float fill, const ReadTuning& tuning) {
	switch (state) {
	case ReadState::Running: return tuning.blinkMin + (tuning.blinkMax - tuning.blinkMin) * fill;
	case ReadState::OnAir: return tuning.blinkSlow;
	case ReadState::Resetting: return tuning.blinkQuick;
	default: return 0.0F;
	}
}

// Set a part's base colour; its Material follows unless the Target brighten owns it this frame.
void Paint(const flecs::entity station, const flecs::entity part, const core::Rgba colour) {
	part.get_mut<PartColour>().colour = colour;
	if (!station.has<Brightened>()) {
		part.get_mut<render::Material>().color = colour;
	}
}

void DriveLamp(const flecs::entity station, const StationRead& read, const core::Rgba bright, const Palette& palette) {
	const auto lamp = station.lookup(kLampPart);
	if (!lamp || !lamp.has<PartColour>()) {
		return;
	}
	const bool lit = read.blinkPhase < 0.5F;
	core::Rgba colour = Token(palette, kLampOff);
	if (read.flashLeft > 0.0F) {
		colour = Token(palette, kLampGreen);
	}
	else {
		switch (read.state) {
		case ReadState::Running: colour = lit ? Token(palette, kLampAmber) : colour; break;
		case ReadState::Done: colour = Token(palette, kLampGreen); break;
		case ReadState::OnAir: colour = lit ? bright : colour; break;
		case ReadState::Coupled: colour = bright; break;
		case ReadState::Resetting: colour = lit ? Token(palette, kLampGrey) : colour; break;
		default: break;
		}
	}
	Paint(station, lamp, colour);
}

// The bar fills from its left end in the lamp colour over its dark track; empty, the fill all but vanishes.
void DriveBar(const flecs::entity station, const StationRead& read, const core::Rgba bright, const Palette& palette) {
	const auto bar = station.lookup(kBarPart);
	if (!bar || !bar.has<PartHome>() || !bar.has<render::CubePrimitive>()) {
		return;
	}
	core::Rgba colour = Token(palette, kBarTrack);
	if (read.fill > 0.0F) {
		switch (read.state) {
		case ReadState::Done: colour = Token(palette, kLampGreen); break;
		case ReadState::Coupled: colour = bright; break;
		default: colour = Token(palette, kLampAmber); break;
		}
	}
	const float fill = std::clamp(read.fill, kMinFill, 1.0F);
	const auto home = bar.get<PartHome>().position;
	const float width = bar.get<render::CubePrimitive>().size.x;
	auto& transform = bar.get_mut<spatial::Transform>();
	transform.scale = {fill, 1.0F, 1.0F};
	transform.position = home + glm::vec3{-(1.0F - fill) * width * 0.5F, 0.0F, 0.0F};
	Paint(station, bar, colour);
}

void SpinPart(const flecs::entity station, const char* name, const float angle) {
	const auto part = station.lookup(name);
	if (part) {
		part.get_mut<spatial::Transform>().rotation.y = angle;
	}
}

void DriveMoving(
	const flecs::entity station,
	const StationRead& read,
	const ReadTuning& tuning
) {
	SpinPart(station, kLeftReelPart, read.reelAngle);
	SpinPart(station, kRightReelPart, -read.reelAngle);
	SpinPart(station, kCrankPart, read.crankAngle);

	// The paper stands while the Telex is loaded, and slides up (still shown) through the flash.
	const auto paper = station.lookup(kPaperPart);
	if (!paper || !paper.has<PartHome>()) {
		return;
	}
	const bool sliding = read.flashLeft > 0.0F;
	const bool shown = sliding || read.state == ReadState::Running;
	if (shown == paper.has(flecs::Disabled)) {
		shown ? paper.enable() : paper.disable();
	}
	const float slid = sliding ? 1.0F - read.flashLeft / tuning.telexFlash : 0.0F;
	paper.get_mut<spatial::Transform>().position = paper.get<PartHome>().position + glm::vec3{0.0F, tuning.telexSlide * slid, 0.0F};
}

void RegisterReadDrive(const flecs::world& world, const flecs::entity roomRoot) {
	world.system<const Job, StationRead>("StationReadDrive")
		.kind(flecs::OnUpdate)
		.each([tickets = world.query<const Ticket>(),
			   holders = world.query<const Holding>()](flecs::iter& it, const size_t i, const Job& job, StationRead& read) {
			const auto world = it.world();
			const auto station = it.entity(i);
			const auto& tuning = world.get<ReadTuning>();
			const auto& palette = world.get<Palette>();
			const float dt = it.delta_time();

			read.state = DeriveState(world, station, job, tickets);
			read.cranked = holders.find([&](const Holding& holding) { return holding.station == station.id(); });
			read.fill = read.state == ReadState::Idle || read.state == ReadState::Dark ? 0.0F : job.progress;
			read.flashLeft = std::max(0.0F, read.flashLeft - dt);
			const float rate = BlinkRate(read.state, read.fill, tuning);
			read.blinkPhase = rate > 0.0F ? std::fmod(read.blinkPhase + rate * dt, 1.0F) : 0.0F;

			const bool running = job.phase == JobPhase::Running;
			if (running) {
				read.reelAngle += (read.cranked ? tuning.reelCrankedSpeed : tuning.reelSpeed) * dt;
			}
			if (running && read.cranked) {
				read.crankAngle += tuning.crankSpeed * dt;
			}

			const auto* frequency = station.has<Radio>() ? FrequencyColour(world, station.get<Radio>().frequency) : nullptr;
			const core::Rgba bright = frequency != nullptr ? *frequency : Token(palette, kLampAmber);
			DriveLamp(station, read, bright, palette);
			DriveBar(station, read, bright, palette);
			DriveMoving(station, read, tuning);

			// A dark Radio's set dims; it returns to its frequency's body colour otherwise.
			const auto body = station.lookup(kBodyPart);
			if (station.has<Radio>() && body && body.has<PartColour>()) {
				const auto* base = FrequencyBodyColour(world, station.get<Radio>().frequency);
				if (base != nullptr) {
					Paint(station, body, read.state == ReadState::Dark ? Dim(*base, tuning.darkDim) : *base);
				}
			}
		})
		.child_of(roomRoot);
}

// Burst the Station's `confetti` emitter, like the Counter's dust puff.
void BurstConfetti(const flecs::world& world, const flecs::entity station) {
	const auto confetti = station.lookup(kConfettiEmitterName);
	if (!confetti || !confetti.has<particles::ParticleEmitter>()) {
		return;
	}
	const auto& tuning = world.get<ConfettiTuning>();
	const core::Rgba start = Token(world.get<Palette>(), kConfettiColourToken);
	confetti.get_mut<particles::ParticleEmitter>() = {
		.rate = 0.0F,
		.accumulator = std::round(tuning.count),
		.particleLifetime = tuning.lifetime,
		.speed = tuning.speed,
		.startSize = tuning.size,
		.spread = tuning.spread,
		.direction = {0.0F, 1.0F, 0.0F},
		.colorStart = start,
		.colorEnd = {.r = start.r, .g = start.g, .b = start.b, .a = 0},
	};
}

void RegisterTransmissionFlash(const flecs::world& world, const flecs::entity roomRoot) {
	world.observer<ShiftWindow>("StationTransmissionFlash")
		.event<Transmission>()
		.each([](flecs::iter& it, size_t, ShiftWindow&) {
			const auto world = it.world();
			const auto station = world.entity(it.param<Transmission>()->station);
			if (!station.is_alive() || !station.has<StationRead>()) {
				return;
			}
			station.get_mut<StationRead>().flashLeft = world.get<ReadTuning>().telexFlash;
			BurstConfetti(world, station);
		})
		.child_of(roomRoot);
}

} // namespace

void RegisterStationReads(const flecs::world& world, const flecs::entity roomRoot) {
	RegisterReadDrive(world, roomRoot);
	RegisterTransmissionFlash(world, roomRoot);
}

} // namespace signal_room
