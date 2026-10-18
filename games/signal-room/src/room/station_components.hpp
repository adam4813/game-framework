#pragma once

#include <string>

#include <flecs.h>

namespace signal_room {

// Where a Station's Job is: Idle (nothing placed) → Loading (some inputs placed, only for Transforms
// with more than one) → Running (inputs complete, or a Hold underway) → Done (outputs waiting on the
// Station) → Idle once every output is picked up.
enum class JobPhase { Idle, Loading, Running, Done };

// On each in-service Station: the one Job it runs, with no queue. The progress belongs to the
// Station, so any Operator can walk away and come back. Placed inputs and waiting outputs are the
// Station's (Holds, item) pairs. `transform` is the active StationTransform's id ("" while Idle);
// `progress` runs 0-1; `taps` counts the presses of a Tap verb.
struct Job {
	JobPhase phase{JobPhase::Idle};
	std::string transform;
	float progress{0.0F};
	int taps{0};
};

// On an Operator while it Holds the action key at a Station. Its Target is frozen to `station`;
// releasing the key, any move input or a dash removes it. `since` is how long it has Held, in seconds.
struct Holding {
	flecs::entity_t station{0};
	float since{0.0F};
};

// Why a press did nothing. The Bubble shows it as a ✗ and the audio spec will hook it.
enum class RefusalReason {
	WrongItem,   // carrying something the Station (or Counter) can't take
	Busy,        // the Station is mid-Job or has outputs waiting
	NothingToDo, // empty-handed with nothing to pick up or start
	OtherHolder, // another Operator is Holding the Station
	CounterFull, // carrying at an occupied Counter, or empty-handed at an empty one
};

// Event raised on the Operator when its press can't act. It never changes the Operator, the Item or
// the Station.
struct Refusal {
	flecs::entity_t target{0};
	RefusalReason reason{RefusalReason::NothingToDo};
};

} // namespace signal_room
