#ifndef MAX_FLOW_H
#define MAX_FLOW_H

#include <algorithm>
#include <cstddef>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace sentinela {

struct FlowEdge {
	int from;
	int to;
	long long capacity;
};

struct MaxFlowResult {
	long long flow = 0;
	std::vector<FlowEdge> edges;
	// Flujo asignado a cada arista de entrada, en el mismo orden que edges.
	std::vector<long long> edgeFlow;
	// 1 si el nodo queda del lado de source en el corte minimo, 0 si no.
	std::vector<int> minCutSide;
};

namespace detail {

constexpr long long FLOW_INF = std::numeric_limits<long long>::max();

struct ResidualEdge {
	int to;
	int reverseIndex;
	long long capacity;
};

// Grafo residual compartido por Dinic y Edmonds-Karp.
class FlowNetwork {
public:
	explicit FlowNetwork(int n)
		: graph_(n), level_(n), nextEdge_(n) {}

	// Agrega la arista dirigida from -> to y regresa su posicion en graph_[from].
	std::size_t addEdge(int from, int to, long long capacity) {
		ResidualEdge forward{to, static_cast<int>(graph_[to].size()), capacity};
		ResidualEdge reverse{from, static_cast<int>(graph_[from].size()), 0};
		graph_[from].push_back(forward);
		graph_[to].push_back(reverse);
		return graph_[from].size() - 1;
	}

	// Capacidad residual restante de una arista agregada con addEdge.
	long long residual(int from, std::size_t index) const {
		return graph_[from][index].capacity;
	}

	// Dinic: grafo de niveles por BFS y flujo bloqueante por DFS. O(V^2 E).
	long long dinic(int source, int sink) {
		long long totalFlow = 0;
		while (buildLevels(source, sink)) {
			std::fill(nextEdge_.begin(), nextEdge_.end(), 0);
			long long pushed = sendFlow(source, sink, FLOW_INF);
			while (pushed > 0) {
				totalFlow = addChecked(totalFlow, pushed);
				pushed = sendFlow(source, sink, FLOW_INF);
			}
		}
		return totalFlow;
	}

	// Edmonds-Karp: camino aumentante mas corto por BFS en cada iteracion. O(V E^2).
	long long edmondsKarp(int source, int sink) {
		long long totalFlow = 0;
		long long pushed = augmentShortestPath(source, sink);
		while (pushed > 0) {
			totalFlow = addChecked(totalFlow, pushed);
			pushed = augmentShortestPath(source, sink);
		}
		return totalFlow;
	}

	// Marca los nodos alcanzables desde source en el grafo residual (corte minimo).
	std::vector<int> sourceSide(int source) {
		buildLevels(source, source);
		std::vector<int> side(graph_.size(), 0);
		for (std::size_t v = 0; v < graph_.size(); ++v) {
			side[v] = level_[v] != -1 ? 1 : 0;
		}
		return side;
	}

private:
	std::vector<std::vector<ResidualEdge>> graph_;
	std::vector<int> level_;
	std::vector<std::size_t> nextEdge_;

	// Suma dos flujos detectando desbordamiento de long long.
	static long long addChecked(long long total, long long pushed) {
		if (total > FLOW_INF - pushed) {
			throw std::overflow_error("El flujo maximo excede long long");
		}
		return total + pushed;
	}

	// BFS desde source sobre aristas con capacidad residual; true si alcanza sink.
	bool buildLevels(int source, int sink) {
		std::fill(level_.begin(), level_.end(), -1);
		std::queue<int> pending;
		level_[source] = 0;
		pending.push(source);
		while (!pending.empty()) {
			int u = pending.front();
			pending.pop();
			for (const ResidualEdge& edge : graph_[u]) {
				if (edge.capacity > 0 && level_[edge.to] == -1) {
					level_[edge.to] = level_[u] + 1;
					pending.push(edge.to);
				}
			}
		}
		return level_[sink] != -1;
	}

	// DFS que empuja flujo solo por aristas que avanzan un nivel.
	long long sendFlow(int u, int sink, long long available) {
		if (u == sink) {
			return available;
		}
		long long pushed = 0;
		while (pushed == 0 && nextEdge_[u] < graph_[u].size()) {
			ResidualEdge& edge = graph_[u][nextEdge_[u]];
			if (edge.capacity > 0 && level_[edge.to] == level_[u] + 1) {
				pushed = sendFlow(edge.to, sink, std::min(available, edge.capacity));
				if (pushed > 0) {
					edge.capacity -= pushed;
					graph_[edge.to][edge.reverseIndex].capacity += pushed;
				}
			}
			if (pushed == 0) {
				++nextEdge_[u];
			}
		}
		return pushed;
	}

	// Busca el camino aumentante mas corto, lo satura y regresa el flujo enviado.
	long long augmentShortestPath(int source, int sink) {
		std::vector<std::pair<int, std::size_t>> parent(graph_.size(), {-1, 0});
		std::queue<int> pending;
		parent[source] = {source, 0};
		pending.push(source);
		while (!pending.empty() && parent[sink].first == -1) {
			int u = pending.front();
			pending.pop();
			for (std::size_t i = 0; i < graph_[u].size(); ++i) {
				const ResidualEdge& edge = graph_[u][i];
				if (edge.capacity > 0 && parent[edge.to].first == -1) {
					parent[edge.to] = {u, i};
					pending.push(edge.to);
				}
			}
		}
		if (parent[sink].first == -1) {
			return 0;
		}
		long long bottleneck = FLOW_INF;
		for (int v = sink; v != source; v = parent[v].first) {
			bottleneck = std::min(bottleneck, graph_[parent[v].first][parent[v].second].capacity);
		}
		for (int v = sink; v != source; v = parent[v].first) {
			ResidualEdge& edge = graph_[parent[v].first][parent[v].second];
			edge.capacity -= bottleneck;
			graph_[edge.to][edge.reverseIndex].capacity += bottleneck;
		}
		return bottleneck;
	}
};

// Valida la entrada, ejecuta el algoritmo elegido y arma el resultado completo.
inline MaxFlowResult solveMaxFlow(
	int n,
	const std::vector<FlowEdge>& edges,
	int source,
	int sink,
	bool useDinic
) {
	if (n <= 0) {
		throw std::invalid_argument("El numero de nodos debe ser positivo");
	}
	if (source < 0 || source >= n || sink < 0 || sink >= n) {
		throw std::invalid_argument("Source o sink fuera de rango");
	}
	for (const FlowEdge& edge : edges) {
		if (edge.from < 0 || edge.from >= n || edge.to < 0 || edge.to >= n) {
			throw std::invalid_argument("Extremo de arista fuera de rango");
		}
		if (edge.capacity < 0) {
			throw std::invalid_argument("Las capacidades no pueden ser negativas");
		}
	}

	FlowNetwork network(n);
	std::vector<std::size_t> positions;
	positions.reserve(edges.size());
	for (const FlowEdge& edge : edges) {
		positions.push_back(network.addEdge(edge.from, edge.to, edge.capacity));
	}

	MaxFlowResult result;
	result.edges = edges;
	if (source != sink) {
		result.flow = useDinic ? network.dinic(source, sink) : network.edmondsKarp(source, sink);
	}
	result.edgeFlow.resize(edges.size(), 0);
	for (std::size_t i = 0; i < edges.size(); ++i) {
		result.edgeFlow[i] = edges[i].capacity - network.residual(edges[i].from, positions[i]);
	}
	result.minCutSide = network.sourceSide(source);
	return result;
}

}  // namespace detail

// Calcula el flujo maximo desde source hasta sink con Dinic.
// Los nodos se identifican con indices de 0 a n - 1.
// Las capacidades deben ser no negativas.
// Si source == sink, el flujo maximo se define como 0.
inline MaxFlowResult maxFlow(
	int n,
	const std::vector<FlowEdge>& edges,
	int source,
	int sink
) {
	return detail::solveMaxFlow(n, edges, source, sink, true);
}

// Misma interfaz que maxFlow, resuelta con Edmonds-Karp (tecnica de comparacion).
inline MaxFlowResult edmondsKarp(
	int n,
	const std::vector<FlowEdge>& edges,
	int source,
	int sink
) {
	return detail::solveMaxFlow(n, edges, source, sink, false);
}

}  // namespace sentinela

#endif  // MAX_FLOW_H
