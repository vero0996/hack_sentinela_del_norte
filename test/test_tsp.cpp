
#include "../src/tsp.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace sentinela;

using Graph = std::vector<std::vector<long long>>;

void testCompleteGraph() {
    Graph graph = {
        {-1, 10, 15, 20},
        {10, -1, 35, 25},
        {15, 35, -1, 30},
        {20, 25, 30, -1}
    };

    TSPResult result = solveTSP(graph);

    assert(result.feasible);
    assert(result.cost == 80);
    assert(result.tour.size() == 5);
    assert(result.tour.front() == 0);
    assert(result.tour.back() == 0);

    std::cout << "Caso 1: grafo completo OK\n";
}

void testNoHamiltonianCycle() {
    Graph graph = {
        {-1, 5, -1},
        {-1, -1, 7},
        {-1, -1, -1}
    };

    TSPResult result = solveTSP(graph);

    assert(!result.feasible);

    std::cout << "Caso 2: sin ciclo hamiltoniano OK\n";
}

void testSingleNode() {
    Graph graph = {{-1}};

    TSPResult result = solveTSP(graph);

    assert(result.feasible);
    assert(result.cost == 0);
    assert((result.tour == std::vector<int>{0, 0}));

    std::cout << "Caso 3: un solo nodo OK\n";
}

void testInvalidMatrix() {
    Graph graph = {
        {-1, 4},
        {4}
    };

    bool threw = false;

    try {
        solveTSP(graph);
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assert(threw);

    std::cout << "Caso 4: matriz no cuadrada OK\n";
}

int main() {
    testCompleteGraph();
    testNoHamiltonianCycle();
    testSingleNode();
    testInvalidMatrix();

    std::cout << "Todas las pruebas de TSP pasaron.\n";
    return 0;
}
