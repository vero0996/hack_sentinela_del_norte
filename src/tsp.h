#ifndef TSP_H
#define TSP_H

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace sentinela {

struct TSPResult {
	bool feasible = false;
	long long cost = 0;
	std::vector<int> tour;
	// true si el recorrido es optimo (Held-Karp), false si es heuristico.
	bool exact = false;
};

// Held-Karp se usa hasta este numero de nodos: O(n * 2^n) de memoria
// (n = 15 usa ~6 MB; n = 20 superaria 250 MB en el compilador web).
constexpr std::size_t TSP_EXACT_LIMIT = 15;
constexpr std::size_t TSP_EXACT_MAX = 20;

namespace detail {

constexpr long long TSP_INF = std::numeric_limits<long long>::max() / 4;

// Valida que la matriz sea cuadrada. En la matriz, -1 significa que no hay arista.
inline void validateTSPMatrix(const std::vector<std::vector<long long>>& graph) {
	for (const std::vector<long long>& row : graph) {
		if (row.size() != graph.size()) {
			throw std::invalid_argument("La matriz de costos debe ser cuadrada");
		}
	}
}

// Costo de la arista u -> v, o TSP_INF si no existe.
inline long long edgeCost(const std::vector<std::vector<long long>>& graph, int u, int v) {
	return graph[u][v] == -1 ? TSP_INF : graph[u][v];
}

// Suma el costo de un recorrido cerrado; TSP_INF si usa una arista inexistente.
inline long long tourCost(const std::vector<std::vector<long long>>& graph, const std::vector<int>& tour) {
	long long total = 0;
	for (std::size_t i = 0; i + 1 < tour.size() && total < TSP_INF; ++i) {
		long long step = edgeCost(graph, tour[i], tour[i + 1]);
		total = step == TSP_INF ? TSP_INF : total + step;
	}
	return total;
}

// Indica si la matriz es simetrica (requisito para el delta O(1) de 2-opt).
inline bool isSymmetric(const std::vector<std::vector<long long>>& graph) {
	bool symmetric = true;
	for (std::size_t u = 0; u < graph.size() && symmetric; ++u) {
		for (std::size_t v = u + 1; v < graph.size() && symmetric; ++v) {
			symmetric = graph[u][v] == graph[v][u];
		}
	}
	return symmetric;
}

// Vecino mas cercano desde el nodo 0; regresa {} si se queda sin aristas.
inline std::vector<int> nearestNeighborTour(const std::vector<std::vector<long long>>& graph) {
	std::size_t n = graph.size();
	std::vector<bool> visited(n, false);
	std::vector<int> tour = {0};
	visited[0] = true;
	bool stuck = false;
	while (tour.size() < n && !stuck) {
		int current = tour.back();
		int best = -1;
		for (std::size_t v = 0; v < n; ++v) {
			long long cost = edgeCost(graph, current, static_cast<int>(v));
			if (!visited[v] && cost < TSP_INF && (best == -1 || cost < graph[current][best])) {
				best = static_cast<int>(v);
			}
		}
		stuck = best == -1;
		if (!stuck) {
			visited[best] = true;
			tour.push_back(best);
		}
	}
	if (stuck || edgeCost(graph, tour.back(), 0) == TSP_INF) {
		return {};
	}
	tour.push_back(0);
	return tour;
}

// 2-opt de primera mejora: invierte tour[i..j] mientras reduzca el costo.
// Solo para matrices simetricas, donde invertir un tramo no cambia su costo interno.
inline void improveTwoOpt(const std::vector<std::vector<long long>>& graph, std::vector<int>& tour) {
	std::size_t last = tour.size() - 1;
	bool improved = true;
	while (improved) {
		improved = false;
		for (std::size_t i = 1; i + 1 < last; ++i) {
			for (std::size_t j = i + 1; j < last; ++j) {
				int a = tour[i - 1];
				int b = tour[i];
				int c = tour[j];
				int d = tour[j + 1];
				long long newAb = edgeCost(graph, a, c);
				long long newCd = edgeCost(graph, b, d);
				if (newAb < TSP_INF && newCd < TSP_INF) {
					long long delta = newAb + newCd - graph[a][b] - graph[c][d];
					if (delta < 0) {
						std::size_t left = i;
						std::size_t right = j;
						while (left < right) {
							std::swap(tour[left], tour[right]);
							++left;
							--right;
						}
						improved = true;
					}
				}
			}
		}
	}
}

} 

// TSP exacto con Held-Karp (programacion dinamica sobre subconjuntos).
// Tiempo O(n^2 * 2^n), memoria O(n * 2^n). Admite como maximo TSP_EXACT_MAX nodos.
// El recorrido empieza y termina en el nodo 0; -1 en la matriz indica arista inexistente.
inline TSPResult solveTSPExact(const std::vector<std::vector<long long>>& graph) {
	std::size_t n = graph.size();
	if (n > TSP_EXACT_MAX) {
		throw std::invalid_argument("TSP exacto admite como maximo 20 nodos");
	}
	detail::validateTSPMatrix(graph);
	TSPResult result;
	result.exact = true;
	if (n == 1) {
		result.feasible = true;
		result.tour = {0, 0};
	}
	if (n <= 1) {
		return result;
	}

	std::size_t states = std::size_t{1} << n;
	std::size_t fullMask = states - 1;
	std::vector<std::vector<long long>> dp(states, std::vector<long long>(n, detail::TSP_INF));
	std::vector<std::vector<int>> parent(states, std::vector<int>(n, -1));
	dp[1][0] = 0;

	for (std::size_t mask = 1; mask < states; mask += 2) {
		for (std::size_t u = 0; u < n; ++u) {
			if (dp[mask][u] != detail::TSP_INF) {
				for (std::size_t v = 1; v < n; ++v) {
					std::size_t bit = std::size_t{1} << v;
					if ((mask & bit) == 0 && graph[u][v] != -1) {
						std::size_t nextMask = mask | bit;
						long long candidate = dp[mask][u] + graph[u][v];
						if (candidate < dp[nextMask][v]) {
							dp[nextMask][v] = candidate;
							parent[nextMask][v] = static_cast<int>(u);
						}
					}
				}
			}
		}
	}

	long long bestCost = detail::TSP_INF;
	int last = -1;
	for (std::size_t u = 1; u < n; ++u) {
		if (dp[fullMask][u] != detail::TSP_INF && graph[u][0] != -1) {
			long long candidate = dp[fullMask][u] + graph[u][0];
			if (candidate < bestCost) {
				bestCost = candidate;
				last = static_cast<int>(u);
			}
		}
	}
	if (last == -1) {
		return result;
	}

	std::vector<int> reversedTour;
	std::size_t mask = fullMask;
	int current = last;
	while (current > 0) {
		reversedTour.push_back(current);
		int previous = parent[mask][current];
		mask ^= std::size_t{1} << current;
		current = previous;
	}
	reversedTour.push_back(0);

	result.feasible = true;
	result.cost = bestCost;
	result.tour.assign(reversedTour.rbegin(), reversedTour.rend());
	result.tour.push_back(0);
	return result;
}

// TSP aproximado: vecino mas cercano O(n^2) y mejora local 2-opt O(n^2) por pasada.
// No garantiza el optimo. Con matriz asimetrica solo aplica vecino mas cercano.
// Puede reportar infactible en grafos incompletos aunque exista un ciclo hamiltoniano.
inline TSPResult solveTSPHeuristic(const std::vector<std::vector<long long>>& graph) {
	detail::validateTSPMatrix(graph);
	TSPResult result;
	if (graph.size() == 1) {
		result.feasible = true;
		result.tour = {0, 0};
	}
	if (graph.size() <= 1) {
		return result;
	}
	std::vector<int> tour = detail::nearestNeighborTour(graph);
	if (tour.empty()) {
		return result;
	}
	if (detail::isSymmetric(graph)) {
		detail::improveTwoOpt(graph, tour);
	}
	result.feasible = true;
	result.cost = detail::tourCost(graph, tour);
	result.tour = tour;
	return result;
}

// Elige Held-Karp hasta TSP_EXACT_LIMIT nodos y la heuristica para instancias mayores.
inline TSPResult solveTSP(const std::vector<std::vector<long long>>& graph) {
	return graph.size() <= TSP_EXACT_LIMIT ? solveTSPExact(graph) : solveTSPHeuristic(graph);
}

}  // namespace sentinela

#endif  // TSP_H
