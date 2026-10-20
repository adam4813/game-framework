#pragma once

#include <string>

#include <flecs.h>

namespace signal_room {

// A where-to-go chevron: floats over `station`, in `operator`'s identity colour, because that Operator
// carries an Item the Station would take now. `age` (seconds) drives the bob.
struct Chevron {
	flecs::entity_t operator_{0};
	flecs::entity_t station{0};
	float age{0.0F};
};

// An Operator's Bubble, a top-level Room entity that follows it. While `timeLeft` > 0 it shows the
// glyph named `glyph` (a GreyboxCatalogue glyph); otherwise it is hidden.
struct Bubble {
	flecs::entity_t operator_{0};
	std::string glyph;
	float timeLeft{0.0F};
};

// Spawn `op`'s hidden Bubble under `parent`: the catalogue's `bubble` backing plus one child per glyph.
void SpawnBubble(const flecs::world& world, flecs::entity parent, flecs::entity op);

// Register, as children of the Room's root entity:
// - GuidanceChevrons: for each Operator carrying an Item, a Chevron over every in-service Station that
//   would take it now (TransformTaking, the interact rule's own check); Chevrons that no longer apply are
//   destroyed, and several Operators' Chevrons over one Station sit side by side. Never over Counters.
// - GuidanceBubbles: positions each Bubble beside and above its Operator and counts its glyph down.
// - BubbleRefusal: on a Refusal, shows the `refuse` glyph for the tuned duration, restarting on a repeat.
void RegisterGuidance(const flecs::world& world, flecs::entity roomRoot);

} // namespace signal_room
