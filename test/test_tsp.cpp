
#include "../src/tsp.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <random>
#include <stdexcept>
#include <utility>
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

// Comprueba que el recorrido sea un ciclo hamiltoniano desde 0 y que su costo coincida.
void checkTour(const Graph& graph, const TSPResult& result) {
    const std::size_t n = graph.size();

    assert(result.feasible);
    assert(result.tour.size() == n + 1);
    assert(result.tour.front() == 0);
    assert(result.tour.back() == 0);

    std::vector<int> sorted(result.tour.begin(), result.tour.end() - 1);
    std::sort(sorted.begin(), sorted.end());
    for (std::size_t i = 0; i < n; ++i) {
        assert(sorted[i] == static_cast<int>(i));
    }

    long long cost = 0;
    for (std::size_t i = 0; i < n; ++i) {
        cost += graph[result.tour[i]][result.tour[i + 1]];
    }
    assert(cost == result.cost);
}

// Matriz simetrica de distancias euclidianas redondeadas entre puntos aleatorios.
Graph randomEuclidean(std::mt19937& rng, int n) {
    std::vector<std::pair<double, double>> points;
    for (int i = 0; i < n; ++i) {
        points.push_back({static_cast<double>(rng() % 1000), static_cast<double>(rng() % 1000)});
    }

    Graph graph(n, std::vector<long long>(n, 0));
    for (int u = 0; u < n; ++u) {
        for (int v = 0; v < n; ++v) {
            graph[u][v] = std::llround(std::hypot(
                points[u].first - points[v].first,
                points[u].second - points[v].second));
        }
    }
    return graph;
}

void testHeuristicNeverBeatsExact() {
    std::mt19937 rng(2038);

    for (int round = 0; round < 100; ++round) {
        const Graph graph = randomEuclidean(rng, 2 + static_cast<int>(rng() % 9));
        const TSPResult exact = solveTSPExact(graph);
        const TSPResult heuristic = solveTSPHeuristic(graph);

        checkTour(graph, exact);
        checkTour(graph, heuristic);
        assert(exact.exact && !heuristic.exact);
        assert(heuristic.cost >= exact.cost);
    }

    std::cout << "Caso 5: heuristica valida y nunca mejor que Held-Karp OK\n";
}

void testLargeCircle() {
    const int n = 450;
    const double pi = std::acos(-1.0);
    std::vector<std::pair<double, double>> points;

    for (int i = 0; i < n; ++i) {
        const double angle = 2 * pi * i / n;
        points.push_back({100000 * std::cos(angle), 100000 * std::sin(angle)});
    }

    std::mt19937 rng(7);
    std::shuffle(points.begin() + 1, points.end(), rng);

    Graph graph(n, std::vector<long long>(n, 0));
    for (int u = 0; u < n; ++u) {
        for (int v = 0; v < n; ++v) {
            graph[u][v] = std::llround(std::hypot(
                points[u].first - points[v].first,
                points[u].second - points[v].second));
        }
    }

    const TSPResult result = solveTSP(graph);
    checkTour(graph, result);
    assert(!result.exact);

    // El perimetro del circulo (~628 318) es el optimo; 2-opt debe quedar muy cerca.
    assert(result.cost < 628318 * 1.05);

    std::cout << "Caso 6: 450 nodos en circulo con heuristica (costo "
              << result.cost << ") OK\n";
}

void testDispatchAndLimits() {
    std::mt19937 rng(11);
    const Graph small = randomEuclidean(rng, static_cast<int>(TSP_EXACT_LIMIT));
    const Graph large = randomEuclidean(rng, static_cast<int>(TSP_EXACT_LIMIT) + 1);

    assert(solveTSP(small).exact);
    assert(!solveTSP(large).exact);

    bool threw = false;
    try {
        solveTSPExact(randomEuclidean(rng, 21));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    std::cout << "Caso 7: seleccion exacto/heuristico y limite de 20 nodos OK\n";
}

int main() {
    testCompleteGraph();
    testNoHamiltonianCycle();
    testSingleNode();
    testInvalidMatrix();
    testHeuristicNeverBeatsExact();
    testLargeCircle();
    testDispatchAndLimits();

    std::cout << "Todas las pruebas de TSP pasaron.\n";
    return 0;
}
