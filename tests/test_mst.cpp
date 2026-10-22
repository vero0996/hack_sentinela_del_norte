#include <cassert>
#include <iostream>
#include <random>
#include "../src/mst.h"

int main() {
    // Caso 1: grafo conectado
    std::vector<sentinel::MSTEdge> edges = {
        {0, 1, 10},
        {0, 2, 6},
        {0, 3, 5},
        {1, 3, 15},
        {2, 3, 4}
    };

    auto result = sentinel::kruskalMST(4, edges);

    assert(result.connected);
    assert(result.total_cost == 19);
    assert(result.edges.size() == 3);

    std::cout << "Caso 1: MST conectado OK\n";

    // Caso 2: grafo desconectado
    std::vector<sentinel::MSTEdge> disconnected = {
        {0, 1, 3},
        {2, 3, 5}
    };

    auto result2 = sentinel::kruskalMST(4, disconnected);

    assert(!result2.connected);
    assert(result2.total_cost == 8);
    assert(result2.edges.size() == 2);

    std::cout << "Caso 2: grafo desconectado OK\n";

    // Caso 3: pesos negativos
    std::vector<sentinel::MSTEdge> negative = {
        {0, 1, -2},
        {1, 2, 3},
        {0, 2, 4}
    };

    auto result3 = sentinel::kruskalMST(3, negative);

    assert(result3.connected);
    assert(result3.total_cost == 1);
    assert(result3.edges.size() == 2);

    std::cout << "Caso 3: pesos negativos OK\n";

    // Caso 4: un solo nodo, sin aristas
    auto result4 = sentinel::kruskalMST(1, {});

    assert(result4.connected);
    assert(result4.total_cost == 0);
    assert(result4.edges.empty());

    std::cout << "Caso 4: un solo nodo OK\n";

    // Caso 5: Prim coincide con Kruskal en grafos aleatorios (conexos y no conexos)
    std::mt19937 rng(2038);

    for (int round = 0; round < 200; ++round) {
        const int n = 1 + static_cast<int>(rng() % 40);
        std::vector<sentinel::MSTEdge> randomEdges;

        for (int u = 0; u < n; ++u) {
            for (int v = u + 1; v < n; ++v) {
                if (rng() % 100 < 15) {
                    randomEdges.push_back({u, v, static_cast<long long>(rng() % 100) - 20});
                }
            }
        }

        auto kruskal = sentinel::kruskalMST(n, randomEdges);
        auto prim = sentinel::primMST(n, randomEdges);

        assert(kruskal.total_cost == prim.total_cost);
        assert(kruskal.edges.size() == prim.edges.size());
        assert(kruskal.connected == prim.connected);
    }

    std::cout << "Caso 5: Prim coincide con Kruskal en 200 grafos aleatorios OK\n";
    std::cout << "Todas las pruebas de MST pasaron.\n";

    return 0;
}
