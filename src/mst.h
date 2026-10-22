#ifndef SENTINEL_MST_H
#define SENTINEL_MST_H

#include <algorithm>
#include <cstddef>
#include <functional>
#include <numeric>
#include <queue>
#include <utility>
#include <vector>

namespace sentinel {

struct MSTEdge {
	int u;
	int v;
	long long weight;
};

struct MSTResult {
	long long total_cost = 0;
	std::vector<MSTEdge> edges;
	bool connected = false;
};

class DSU {
private:
	std::vector<int> parent;
	std::vector<int> rank_;

public:
	explicit DSU(int n) : parent(n), rank_(n, 0) {
		std::iota(parent.begin(), parent.end(), 0);
	}

	int find(int x) {
		if (parent[x] != x) {
			parent[x] = find(parent[x]);
		}
		return parent[x];
	}

	bool unite(int a, int b) {
		a = find(a);
		b = find(b);

		if (a == b) {
			return false;
		}

		if (rank_[a] < rank_[b]) {
			std::swap(a, b);
		}

		parent[b] = a;

		if (rank_[a] == rank_[b]) {
			++rank_[a];
		}

		return true;
	}
};

inline MSTResult kruskalMST(int n, std::vector<MSTEdge> edges) {
	MSTResult result;

	if (n < 0) {
		return result;
	}

	if (n == 0) {
		result.connected = true;
		return result;
	}

	std::sort(edges.begin(), edges.end(),
		[](const MSTEdge& a, const MSTEdge& b) {
			if (a.weight != b.weight) {
				return a.weight < b.weight;
			}
			if (a.u != b.u) {
				return a.u < b.u;
			}
			return a.v < b.v;
		});

	DSU dsu(n);

	// Se detiene al tener n - 1 aristas: el arbol ya esta completo.
	std::size_t next = 0;
	while (next < edges.size() && static_cast<int>(result.edges.size()) < n - 1) {
		const MSTEdge& edge = edges[next];
		bool valid = edge.u >= 0 && edge.u < n &&
			edge.v >= 0 && edge.v < n && edge.u != edge.v;

		if (valid && dsu.unite(edge.u, edge.v)) {
			result.edges.push_back(edge);
			result.total_cost += edge.weight;
		}

		++next;
	}

	result.connected =
		(n == 1 || static_cast<int>(result.edges.size()) == n - 1);

	return result;
}

// Tecnica de comparacion: Prim con heap binario, O(E log V).
// Si el grafo no es conexo, arranca de nuevo en cada componente y regresa el
// bosque de expansion minima, igual que Kruskal.
inline MSTResult primMST(int n, const std::vector<MSTEdge>& edges) {
	MSTResult result;

	if (n < 0) {
		return result;
	}

	std::vector<std::vector<std::pair<int, std::size_t>>> adjacency(n);

	for (std::size_t i = 0; i < edges.size(); ++i) {
		const MSTEdge& edge = edges[i];
		bool valid = edge.u >= 0 && edge.u < n &&
			edge.v >= 0 && edge.v < n && edge.u != edge.v;

		if (valid) {
			adjacency[edge.u].push_back({edge.v, i});
			adjacency[edge.v].push_back({edge.u, i});
		}
	}

	using Candidate = std::pair<long long, std::size_t>;
	std::vector<bool> inTree(n, false);

	for (int root = 0; root < n; ++root) {
		if (!inTree[root]) {
			std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate>> pending;
			inTree[root] = true;

			for (const auto& neighbor : adjacency[root]) {
				pending.push({edges[neighbor.second].weight, neighbor.second});
			}

			while (!pending.empty()) {
				std::size_t index = pending.top().second;
				pending.pop();
				int outside = inTree[edges[index].u] ? edges[index].v : edges[index].u;

				if (!inTree[outside]) {
					inTree[outside] = true;
					result.edges.push_back(edges[index]);
					result.total_cost += edges[index].weight;

					for (const auto& [next, nextIndex] : adjacency[outside]) {
						if (!inTree[next]) {
							pending.push({edges[nextIndex].weight, nextIndex});
						}
					}
				}
			}
		}
	}

	result.connected =
		(n <= 1 || static_cast<int>(result.edges.size()) == n - 1);

	return result;
}

} // namespace sentinel

#endif
