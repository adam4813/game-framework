#include "dijkstra_algorithm.hpp"

#include <algorithm>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace engine::pathfinding {

NavPath DijkstraAlgorithm::FindPath(const INavGraph& g, const graph::NodeId start, const graph::NodeId goal) const {
	if (!g.Valid(start) || !g.Valid(goal)) return {};
	if (start == goal) return NavPath{.waypoints = {g.Position(start)}, .found = true};

	using Entry = std::pair<float, graph::NodeId>; // (accumulated cost, node)
	std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
	std::unordered_map<graph::NodeId, graph::NodeId> came_from;
	std::unordered_map<graph::NodeId, float> dist;
	std::unordered_set<graph::NodeId> visited;

	dist[start] = 0.0f;
	open.emplace(0.0f, start);

	while (!open.empty()) {
		const auto [d, current] = open.top();
		open.pop();

		if (visited.contains(current)) continue;
		visited.insert(current);

		if (current == goal) {
			std::vector<glm::vec3> waypoints;
			graph::NodeId node = goal;
			while (came_from.contains(node)) {
				waypoints.push_back(g.Position(node));
				node = came_from[node];
			}
			waypoints.push_back(g.Position(start));
			std::ranges::reverse(waypoints);
			return NavPath{.waypoints = std::move(waypoints), .found = true};
		}

		for (const auto& [to, cost] : g.Neighbors(current)) {
			if (visited.contains(to)) continue;
			const float new_dist = dist[current] + cost;
			if (!dist.contains(to) || new_dist < dist[to]) {
				came_from[to] = current;
				dist[to] = new_dist;
				open.emplace(new_dist, to);
			}
		}
	}

	return {};
}

} // namespace engine::pathfinding
