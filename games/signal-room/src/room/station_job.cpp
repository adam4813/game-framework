#include "station_job.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "item_components.hpp"
#include "item_placement.hpp"
#include "item_spawn.hpp"
#include "operator_components.hpp"
#include "radio_components.hpp"
#include "radio_systems.hpp"
#include "room_components.hpp"
#include "station_catalogue.hpp"
#include "ticket_simulation.hpp"

namespace signal_room {

namespace {

const StationTransform* FindTransform(const StationKind& kind, const std::string& id) {
	const auto found = std::ranges::find(kind.transforms, id, &StationTransform::id);
	return found == kind.transforms.end() ? nullptr : &*found;
}

const StationKind& KindOf(const flecs::world& world, const flecs::entity station) {
	return *world.get<StationCatalogue>().Get(station.get<Station>().kind);
}

// The Items a Station holds: a Job's placed inputs, or its waiting outputs.
std::vector<flecs::entity> HeldItems(const flecs::entity station) {
	std::vector<flecs::entity> items;
	station.each<Holds>([&](const flecs::entity item) { items.push_back(item); });
	return items;
}

// Whether `have` fits inside `inputs`, ignoring order and counting repeats.
bool FitsInside(std::vector<std::string> have, std::vector<std::string> inputs) {
	for (const auto& id : have) {
		const auto slot = std::ranges::find(inputs, id);
		if (slot == inputs.end()) {
			return false;
		}
		inputs.erase(slot);
	}
	return true;
}

// The frequency of the first Item that has one: what a Transform's outputs inherit.
std::string InheritedFrequency(const std::vector<flecs::entity>& inputs) {
	for (const auto input : inputs) {
		if (const auto& frequency = input.get<Item>().frequency; !frequency.empty()) {
			return frequency;
		}
	}
	return {};
}

void ResetJob(Job& job) { job = {}; }

// Finish the Job: `inputs` are consumed; a transmit Transform sends its transmission, any other leaves
// its outputs on the Station.
void Complete(
	const flecs::world& world,
	const flecs::entity station,
	Job& job,
	const StationTransform& transform,
	const std::vector<flecs::entity>& inputs
) {
	std::string frequency = InheritedFrequency(inputs);
	if (frequency.empty() && station.has<Radio>()) {
		frequency = station.get<Radio>().frequency;
	}
	for (const auto input : inputs) {
		input.destruct();
	}
	if (transform.transmit) {
		RaiseTransmission(world, frequency, station.id());
	}
	for (const auto& outputId : transform.outputs) {
		const auto output = SpawnItem(world, station.parent(), outputId, frequency);
		PlaceOnStation(output, station);
		station.add<Holds>(output);
	}
	if (transform.outputs.empty()) {
		ResetJob(job);
		return;
	}
	job.phase = JobPhase::Done;
	job.progress = 1.0F;
}

// The Job's inputs are all in: start it, or finish it at once for an Instant verb.
void InputsComplete(
	const flecs::world& world,
	const flecs::entity station,
	Job& job,
	const StationTransform& transform,
	const std::vector<flecs::entity>& inputs
) {
	if (transform.verb == Verb::Instant) {
		Complete(world, station, job, transform, inputs);
		return;
	}
	job.phase = JobPhase::Running;
	job.progress = 0.0F;
	job.taps = 0;
}

void PlaceCarried(const flecs::world& world, const flecs::entity op, const flecs::entity station) {
	auto& job = station.get_mut<Job>();
	const auto carried = op.target<Carries>();
	if (job.phase != JobPhase::Idle && job.phase != JobPhase::Loading) {
		Refuse(world, op, station, RefusalReason::Busy);
		return;
	}

	const auto* transform = TransformTaking(world, station, carried);
	if (transform == nullptr) {
		Refuse(world, op, station, RefusalReason::WrongItem);
		return;
	}

	auto held = HeldItems(station);
	op.remove<Carries>(carried);
	station.add<Holds>(carried);
	PlaceOnStation(carried, station);
	held.push_back(carried);
	job.transform = transform->id;
	if (held.size() < transform->inputs.size()) {
		job.phase = JobPhase::Loading;
		return;
	}
	InputsComplete(world, station, job, *transform, held);
}

void PickUpOutput(const flecs::entity op, const flecs::entity station) {
	auto& job = station.get_mut<Job>();
	const auto outputs = HeldItems(station);
	station.remove<Holds>(outputs.front());
	op.add<Carries>(outputs.front());
	if (outputs.size() == 1) {
		ResetJob(job);
	}
}

void StartHold(const flecs::entity op, const flecs::entity station) {
	op.set<Holding>({.station = station.id(), .since = 0.0F});
}

void PressEmptyHanded(
	const flecs::world& world,
	const flecs::entity op,
	const flecs::entity station,
	const flecs::query<const Holding>& holders
) {
	auto& job = station.get_mut<Job>();
	if (job.phase == JobPhase::Done) {
		PickUpOutput(op, station);
		return;
	}

	const auto& kind = KindOf(world, station);
	const StationTransform* transform = FindTransform(kind, job.transform);
	if (job.phase == JobPhase::Idle) {
		// Only a Hold Transform with no inputs (the Radio) starts from nothing.
		const auto found = std::ranges::find_if(kind.transforms, [](const StationTransform& candidate) {
			return candidate.inputs.empty() && candidate.verb == Verb::Hold;
		});
		transform = found == kind.transforms.end() ? nullptr : &*found;
	}
	else if (job.phase != JobPhase::Running) {
		transform = nullptr;
	}

	if (transform != nullptr && transform->verb == Verb::Tap && job.phase == JobPhase::Running) {
		job.progress = static_cast<float>(++job.taps) / static_cast<float>(transform->taps);
		if (job.taps >= transform->taps) {
			Complete(world, station, job, *transform, HeldItems(station));
		}
		return;
	}

	const bool holdable =
		transform != nullptr
		&& (transform->verb == Verb::Hold || (transform->verb == Verb::WalkAway && transform->crankDuration > 0.0F));
	if (!holdable) {
		Refuse(world, op, station, RefusalReason::NothingToDo);
		return;
	}
	if (holders.find([&](const Holding& holding) { return holding.station == station.id(); })) {
		Refuse(world, op, station, RefusalReason::OtherHolder);
		return;
	}
	// A dark Radio refuses; a lit one couples the antenna as the Hold starts.
	const bool radio = station.has<Radio>();
	// `online` is last frame's; the live coupler also stops a second Radio engaging in the same frame.
	const auto coupled = world.get<AntennaCoupler>().coupled;
	if (radio && (!station.get<Radio>().online || (coupled != 0 && coupled != station.id()))) {
		Refuse(world, op, station, RefusalReason::NothingToDo);
		return;
	}
	if (job.phase == JobPhase::Idle) {
		job.phase = JobPhase::Running;
		job.transform = transform->id;
	}
	StartHold(op, station);
	if (radio) {
		EngageCoupler(world, station);
	}
}

} // namespace

const StationTransform* TransformTaking(const flecs::world& world, const flecs::entity station, const flecs::entity carried) {
	const auto& job = station.get<Job>();
	if (job.phase != JobPhase::Idle && job.phase != JobPhase::Loading) {
		return nullptr;
	}
	std::vector<std::string> have{carried.get<Item>().id};
	for (const auto item : HeldItems(station)) {
		have.push_back(item.get<Item>().id);
	}
	const auto& kind = KindOf(world, station);
	const auto takes = [&](const StationTransform& transform) {
		return !transform.inputs.empty() && FitsInside(have, transform.inputs);
	};
	if (job.phase == JobPhase::Loading) {
		const auto* loading = FindTransform(kind, job.transform);
		return loading != nullptr && takes(*loading) ? loading : nullptr;
	}
	const auto found = std::ranges::find_if(kind.transforms, takes);
	return found == kind.transforms.end() ? nullptr : &*found;
}

void Refuse(const flecs::world& world, const flecs::entity op, const flecs::entity target, const RefusalReason reason) {
	world.event<Refusal>().id<Operator>().entity(op).ctx(Refusal{.target = target.id(), .reason = reason}).emit();
}

void PressStation(
	const flecs::world& world,
	const flecs::entity op,
	const flecs::entity station,
	const flecs::query<const Holding>& holders
) {
	if (op.target<Carries>()) {
		PlaceCarried(world, op, station);
		return;
	}
	PressEmptyHanded(world, op, station, holders);
}

void AdvanceJob(const flecs::world& world, const flecs::entity station, const float dt, const bool held) {
	auto& job = station.get_mut<Job>();
	if (job.phase != JobPhase::Running) {
		return;
	}
	const auto* transform = FindTransform(KindOf(world, station), job.transform);
	if (transform == nullptr) {
		return;
	}

	float seconds = 0.0F;
	if (transform->verb == Verb::WalkAway) {
		const bool cranked = held && transform->crankDuration > 0.0F;
		seconds = cranked ? transform->crankDuration : transform->duration;
	}
	else if (transform->verb == Verb::Hold && held) {
		seconds = transform->duration;
	}
	if (seconds <= 0.0F) {
		return;
	}
	job.progress = std::min(1.0F, job.progress + dt / seconds);
	if (job.progress >= 1.0F) {
		Complete(world, station, job, *transform, HeldItems(station));
	}
}

bool AutoGrabOutput(const flecs::world& world, const flecs::entity station, const flecs::query<const Operator>& operators) {
	const auto& job = station.get<Job>();
	const auto* transform = FindTransform(KindOf(world, station), job.transform);
	if (job.phase != JobPhase::Done || transform == nullptr || transform->verb == Verb::WalkAway) {
		return false;
	}
	return operators.find([&](const flecs::entity op, const Operator&) {
		if (op.target<Targets>() != station || op.target<Carries>()) {
			return false;
		}
		PickUpOutput(op, station);
		return true;
	});
}

void AddStationJobs(const flecs::world& world, const flecs::entity parent) {
	std::vector<flecs::entity> stations;
	world.query_builder<const Station>()
		.with(flecs::ChildOf, parent)
		.without<Tarped>()
		.build()
		.each([&](const flecs::entity station, const Station&) { stations.push_back(station); });
	for (const auto station : stations) {
		station.set<Job>({}).set<StationRead>({});
	}
}

} // namespace signal_room
