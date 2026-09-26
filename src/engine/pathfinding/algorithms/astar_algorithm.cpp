#include "astar_algorithm.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "engine/profiling/profiling.hpp"

namespace engine::pathfinding {

namespace {
// Reusable A* scratch state. Kept thread_local (PathRequestSystem is multi-threaded) so every search
// on a worker reuses the same buffers instead of allocating four containers per call. Node bookkeeping
// uses flat arrays indexed by the 1-based NodeId with a per-search "version" stamp, so state is reset
// in O(1) (bump the version) rather than by clearing the maps each search.
struct AStarWorkspace {
	std::vector<float> g_score;           // g_score[id] valid when seen[id] == version
	std::vector<graph::NodeId> came_from; // predecessor on the best-known path to id
	std::vector<std::uint32_t> seen;      // seen[id] == version → id has a g_score this search
	std::vector<std::uint32_t> closed;    // closed[id] == version → id finalised this search
	std::vector<NavEdge> neighbors;       // reused neighbour buffer
	// Heap stored as a flat vector so clear() is O(1) (just resets size, preserves capacity).
	// std::priority_queue wraps a vector but doesn't expose clear(); popping all entries is O(N log N).
	std::vector<std::pair<float, graph::NodeId>> open_heap;
	std::uint32_t version = 0;

	void EnsureCapacity(const size_t node_count) {
		if (g_score.size() >= node_count + 1) return;
		g_score.assign(node_count + 1, 0.0f);
		came_from.assign(node_count + 1, graph::NullNode);
		seen.assign(node_count + 1, 0);
		closed.assign(node_count + 1, 0);
		version = 0;
	}

	// Advances to a fresh search version. On the (very rare) 32-bit wrap, clears the stamps so a
	// stale value can't masquerade as "seen this search".
	std::uint32_t NextVersion() {
		if (++version == 0) {
			std::ranges::fill(seen, 0U);
			std::ranges::fill(closed, 0U);
			version = 1;
		}
		return version;
	}

	void ResetOpen() {
		open_heap.clear(); // O(1) — preserves heap vector capacity across searches
	}

	void PushOpen(const float f, const graph::NodeId n) {
		open_heap.emplace_back(f, n);
		std::push_heap(open_heap.begin(), open_heap.end(), std::greater<>{});
	}

	[[nodiscard]] std::pair<float, graph::NodeId> PopOpen() {
		std::pop_heap(open_heap.begin(), open_heap.end(), std::greater<>{});
		const auto top = open_heap.back();
		open_heap.pop_back();
		return top;
	}

	[[nodiscard]] bool OpenEmpty() const { return open_heap.empty(); }
};
} // namespace

NavPath AStarAlgorithm::FindPath(const INavGraph& g, const graph::NodeId start, const graph::NodeId goal) const {
	profiling::Zone _zone{"A*::FindPath"};
	if (!g.Valid(start) || !g.Valid(goal)) return {};
	if (start == goal) return NavPath{.waypoints = {g.Position(start)}, .found = true};

	thread_local AStarWorkspace ws;
	ws.EnsureCapacity(g.NodeCount());
	const std::uint32_t version = ws.NextVersion();
	ws.ResetOpen();

	ws.g_score[start] = 0.0f;
	ws.seen[start] = version;
	ws.PushOpen(g.Heuristic(start, goal), start);

	int nodes_expanded = 0;
	while (!ws.OpenEmpty()) {
		const auto [f, current] = ws.PopOpen();

		if (ws.closed[current] == version) continue; // stale duplicate from lazy deletion
		ws.closed[current] = version;
		++nodes_expanded;

		if (current == goal) {
			profiling::Plot("PathfindingModule", "astar.nodes_expanded", static_cast<int64_t>(nodes_expanded));
			std::vector<glm::vec3> waypoints;
			for (graph::NodeId node = goal; node != start; node = ws.came_from[node]) {
				waypoints.push_back(g.Position(node));
			}
			waypoints.push_back(g.Position(start));
			std::ranges::reverse(waypoints);
			return NavPath{.waypoints = std::move(waypoints), .found = true};
		}

		g.NeighborsInto(current, ws.neighbors);
		const float current_g = ws.g_score[current];
		for (const auto& [to, cost] : ws.neighbors) {
			if (ws.closed[to] == version) continue;
			const float tentative_g = current_g + cost;
			if (ws.seen[to] != version || tentative_g < ws.g_score[to]) {
				ws.came_from[to] = current;
				ws.g_score[to] = tentative_g;
				ws.seen[to] = version;
				ws.PushOpen(tentative_g + g.Heuristic(to, goal), to);
			}
		}
	}

	profiling::Plot("PathfindingModule", "astar.nodes_expanded", static_cast<int64_t>(nodes_expanded));
	return {}; // no path found
}

} // namespace engine::pathfinding
