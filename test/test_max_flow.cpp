
#include "../src/max_flow.h"

#include <cassert>
#include <iostream>
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

int main() {
    testClassicNetwork();
    testNoPath();
    testZeroCapacity();
    testSameSourceAndSink();
    testNegativeCapacity();

    std::cout << "Todas las pruebas de Max Flow pasaron.\n";
    return 0;
}
