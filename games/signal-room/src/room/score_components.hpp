#pragma once

#include <vector>

namespace signal_room {

// World singleton: the attempt's score. Reset at each attempt start and on Reload. `stars` is 0 until the
// Shift is Ended.
struct Score {
	int score{0};
	int chainStep{0}; // index into the Handler Chain
	int par{0};       // sum of the base of every Ticket that went On Air
	int cleared{0};
	int lost{0};
	int bestChainStep{0};
	int stars{0};
};

// World singleton: how the Shift turns its score into stars. Absolute thresholds (when given) win over fractions of par.
struct StarRules {
	std::vector<float> fractions;
	std::vector<float> absolute; // [1 star, 2 stars, 3 stars], empty for none
};

enum class ScoreReason { Clear, LostIntercept };

// Events raised on the Score singleton. ScoreChanged carries the delta actually applied.
struct ScoreChanged {
	int delta{0};
	ScoreReason reason{ScoreReason::Clear};
};
struct ChainChanged {
	int step{0};
};

} // namespace signal_room
