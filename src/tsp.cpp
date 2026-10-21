
#include "tsp.h"

#include <limits>
#include <stdexcept>
#include <cstddef>

namespace sentinela {

TSPResult solveTSP(const std::vector<std::vector<long long>>& graph) {
    const std::size_t n = graph.size();

    if (n == 0) {
        return {};
    }

    if (n > 20) {
        throw std::invalid_argument("TSP exacto admite como maximo 20 nodos");
    }

    for (const auto& row : graph) {
        if (row.size() != n) {
            throw std::invalid_argument("La matriz de costos debe ser cuadrada");
        }
    }

    if (n == 1) {
        return {true, 0, {0, 0}};
    }

    const long long INF = std::numeric_limits<long long>::max() / 4;
    const std::size_t states = std::size_t{1} << n;
    const std::size_t fullMask = states - 1;

    std::vector<std::vector<long long>> dp(
        states, std::vector<long long>(n, INF));

    std::vector<std::vector<int>> parent(
        states, std::vector<int>(n, -1));

    dp[1][0] = 0;

    for (std::size_t mask = 1; mask < states; ++mask) {
        if ((mask & 1U) == 0) {
            continue;
        }

        for (std::size_t u = 0; u < n; ++u) {
            if (dp[mask][u] == INF) {
                continue;
            }

            for (std::size_t v = 1; v < n; ++v) {
                const std::size_t bit = std::size_t{1} << v;

                if ((mask & bit) != 0 || graph[u][v] == -1) {
                    continue;
                }

                const std::size_t nextMask = mask | bit;
                const long long candidate = dp[mask][u] + graph[u][v];

                if (candidate < dp[nextMask][v]) {
                    dp[nextMask][v] = candidate;
                    parent[nextMask][v] = static_cast<int>(u);
                }
            }
        }
    }

    long long bestCost = INF;
    int last = -1;

    for (std::size_t u = 1; u < n; ++u) {
        if (dp[fullMask][u] == INF || graph[u][0] == -1) {
            continue;
        }

        const long long candidate = dp[fullMask][u] + graph[u][0];

        if (candidate < bestCost) {
            bestCost = candidate;
            last = static_cast<int>(u);
        }
    }

    if (last == -1) {
        return {};
    }

    std::vector<int> reversedTour;
    std::size_t mask = fullMask;
    int current = last;

    while (current != 0) {
        reversedTour.push_back(current);

        const int previous = parent[mask][current];
        mask ^= std::size_t{1} << current;
        current = previous;

        if (current < 0) {
            return {};
        }
    }

    reversedTour.push_back(0);

    TSPResult result;
    result.feasible = true;
    result.cost = bestCost;
    result.tour.assign(reversedTour.rbegin(), reversedTour.rend());
    result.tour.push_back(0);

    return result;
}

}  // namespace sentinela
