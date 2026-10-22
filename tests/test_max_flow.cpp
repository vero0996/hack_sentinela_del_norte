
#include "../src/max_flow.h"

#include <cassert>
#include <cstddef>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

using namespace sentinela;

void testClassicNetwork() {
    std::vector<FlowEdge> edges = {
        {0, 1, 16}, {0, 2, 13},
        {1, 2, 10}, {2, 1, 4},
        {1, 3, 12}, {3, 2, 9},
        {2, 4, 14}, {4, 3, 7},
        {3, 5, 20}, {4, 5, 4}
    };

    const auto result = maxFlow(6, edges, 0, 5);
    assert(result.flow == 23);

    std::cout << "Caso 1: red clasica (flujo 23) OK\n";
}

void testNoPath() {
    std::vector<FlowEdge> edges = {
        {0, 1, 5}, {2, 3, 7}
    };

    const auto result = maxFlow(4, edges, 0, 3);
    assert(result.flow == 0);

    std::cout << "Caso 2: sin camino al destino OK\n";
}

void testZeroCapacity() {
    std::vector<FlowEdge> edges = {
        {0, 1, 0}, {1, 2, 0}
    };

    const auto result = maxFlow(3, edges, 0, 2);
    assert(result.flow == 0);

    std::cout << "Caso 3: capacidades cero OK\n";
}

void testSameSourceAndSink() {
    std::vector<FlowEdge> edges = {{0, 1, 10}};

    const auto result = maxFlow(2, edges, 0, 0);
    assert(result.flow == 0);

    std::cout << "Caso 4: origen igual a destino OK\n";
}

void testNegativeCapacity() {
    std::vector<FlowEdge> edges = {{0, 1, -5}};
    bool threw = false;

    try {
        maxFlow(2, edges, 0, 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assert(threw);

    std::cout << "Caso 5: capacidad negativa rechazada OK\n";
}

// Verifica capacidad, conservacion y que el corte minimo mida lo mismo que el flujo.
void checkFlowProperties(
    int n,
    const std::vector<FlowEdge>& edges,
    int source,
    int sink,
    const MaxFlowResult& result
) {
    assert(result.edgeFlow.size() == edges.size());
    assert(static_cast<int>(result.minCutSide.size()) == n);

    std::vector<long long> balance(n, 0);
    long long cutCapacity = 0;

    for (std::size_t i = 0; i < edges.size(); ++i) {
        assert(result.edgeFlow[i] >= 0);
        assert(result.edgeFlow[i] <= edges[i].capacity);
        balance[edges[i].from] -= result.edgeFlow[i];
        balance[edges[i].to] += result.edgeFlow[i];

        if (result.minCutSide[edges[i].from] == 1 &&
            result.minCutSide[edges[i].to] == 0) {
            cutCapacity += edges[i].capacity;
        }
    }

    for (int v = 0; v < n; ++v) {
        if (v != source && v != sink) {
            assert(balance[v] == 0);
        }
    }

    if (source != sink) {
        assert(balance[sink] == result.flow);
        assert(balance[source] == -result.flow);
        assert(result.minCutSide[source] == 1);
        assert(result.minCutSide[sink] == 0);
        assert(cutCapacity == result.flow);
    }
}

void testFlowPropertiesClassic() {
    std::vector<FlowEdge> edges = {
        {0, 1, 16}, {0, 2, 13},
        {1, 2, 10}, {2, 1, 4},
        {1, 3, 12}, {3, 2, 9},
        {2, 4, 14}, {4, 3, 7},
        {3, 5, 20}, {4, 5, 4}
    };

    checkFlowProperties(6, edges, 0, 5, maxFlow(6, edges, 0, 5));
    checkFlowProperties(6, edges, 0, 5, edmondsKarp(6, edges, 0, 5));

    std::cout << "Caso 6: conservacion y corte minimo en red clasica OK\n";
}

void testAntiParallelEdges() {
    std::vector<FlowEdge> edges = {
        {0, 1, 10}, {1, 0, 10},
        {1, 2, 5}, {2, 1, 3},
        {0, 2, 2}
    };

    const auto result = maxFlow(3, edges, 0, 2);
    assert(result.flow == 7);
    checkFlowProperties(3, edges, 0, 2, result);

    std::cout << "Caso 7: aristas antiparalelas OK\n";
}

void testDinicMatchesEdmondsKarp() {
    std::mt19937 rng(2038);

    for (int round = 0; round < 200; ++round) {
        const int n = 2 + static_cast<int>(rng() % 30);
        std::vector<FlowEdge> edges;

        for (int u = 0; u < n; ++u) {
            for (int v = 0; v < n; ++v) {
                if (u != v && rng() % 100 < 20) {
                    edges.push_back({u, v, static_cast<long long>(rng() % 50)});
                }
            }
        }

        const auto dinic = maxFlow(n, edges, 0, n - 1);
        const auto karp = edmondsKarp(n, edges, 0, n - 1);
        assert(dinic.flow == karp.flow);
        checkFlowProperties(n, edges, 0, n - 1, dinic);
        checkFlowProperties(n, edges, 0, n - 1, karp);
    }

    std::cout << "Caso 8: Dinic coincide con Edmonds-Karp en 200 grafos aleatorios OK\n";
}

int main() {
    testClassicNetwork();
    testNoPath();
    testZeroCapacity();
    testSameSourceAndSink();
    testNegativeCapacity();
    testFlowPropertiesClassic();
    testAntiParallelEdges();
    testDinicMatchesEdmondsKarp();

    std::cout << "Todas las pruebas de Max Flow pasaron.\n";
    return 0;
}
