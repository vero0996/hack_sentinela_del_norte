#include "../src/voronoi.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace sentinela;

bool nearlyEqual(double a, double b, double tolerance) {
    return std::fabs(a - b) <= tolerance * std::max(1.0, std::fabs(b));
}

// Un punto esta en un poligono convexo antihorario si queda a la izquierda de cada lado.
bool insideConvex(const Polygon& polygon, const Point& point) {
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const Point& a = polygon[i];
        const Point& b = polygon[(i + 1) % polygon.size()];
        const double cross = (b.x - a.x) * (point.y - a.y) - (b.y - a.y) * (point.x - a.x);
        if (cross < -1e-7) {
            return false;
        }
    }
    return true;
}

// Propiedades que no requieren conocer la respuesta exacta:
// cada celda contiene su sitio, sus vertices estan en el dominio y no mas lejos de su
// sitio que de cualquier otro, y las areas suman el area del dominio.
void checkDiagramProperties(
    const std::vector<Point>& sites,
    double xmin, double ymin, double xmax, double ymax,
    const VoronoiResult& result
) {
    assert(result.cells.size() == sites.size());
    double totalArea = 0;

    for (std::size_t i = 0; i < sites.size(); ++i) {
        const Polygon& cell = result.cells[i];
        assert(cell.size() >= 3);
        assert(insideConvex(cell, sites[i]));
        totalArea += polygonArea(cell);

        for (const Point& vertex : cell) {
            assert(vertex.x >= xmin - 1e-7 && vertex.x <= xmax + 1e-7);
            assert(vertex.y >= ymin - 1e-7 && vertex.y <= ymax + 1e-7);
            const double own = std::hypot(vertex.x - sites[i].x, vertex.y - sites[i].y);
            for (const Point& other : sites) {
                const double distance = std::hypot(vertex.x - other.x, vertex.y - other.y);
                assert(own <= distance + 1e-6 * std::max(1.0, own));
            }
        }
    }

    assert(nearlyEqual(totalArea, (xmax - xmin) * (ymax - ymin), 1e-9));
}

void testSingleSite() {
    const std::vector<Point> sites = {{30, 40}};
    const auto result = voronoiCells(sites, 0, 0, 100, 100);

    assert(result.cells.size() == 1);
    assert(nearlyEqual(polygonArea(result.cells[0]), 10000, 1e-12));

    std::cout << "Caso 1: un sitio ocupa todo el dominio OK\n";
}

void testTwoSites() {
    const std::vector<Point> sites = {{25, 50}, {75, 50}};
    const auto result = voronoiCells(sites, 0, 0, 100, 100);

    assert(nearlyEqual(polygonArea(result.cells[0]), 5000, 1e-12));
    assert(nearlyEqual(polygonArea(result.cells[1]), 5000, 1e-12));
    checkDiagramProperties(sites, 0, 0, 100, 100, result);

    std::cout << "Caso 2: dos sitios dividen el dominio a la mitad OK\n";
}

void testGrid() {
    std::vector<Point> sites;
    for (int x = 5; x < 100; x += 10) {
        for (int y = 5; y < 100; y += 10) {
            sites.push_back({static_cast<double>(x), static_cast<double>(y)});
        }
    }

    const auto result = voronoiCells(sites, 0, 0, 100, 100);
    for (const Polygon& cell : result.cells) {
        assert(nearlyEqual(polygonArea(cell), 100, 1e-9));
    }
    checkDiagramProperties(sites, 0, 0, 100, 100, result);

    std::cout << "Caso 3: malla regular de 100 sitios con celdas de area 100 OK\n";
}

void testCollinearSites() {
    const std::vector<Point> sites = {{10, 50}, {20, 50}, {40, 50}, {80, 50}};
    const auto result = voronoiCells(sites, 0, 0, 100, 100);

    assert(nearlyEqual(polygonArea(result.cells[0]), 1500, 1e-12));
    assert(nearlyEqual(polygonArea(result.cells[3]), 4000, 1e-12));
    checkDiagramProperties(sites, 0, 0, 100, 100, result);

    std::cout << "Caso 4: sitios colineales generan franjas OK\n";
}

void testPrunedMatchesBrute() {
    std::mt19937 rng(2038);

    for (int round = 0; round < 30; ++round) {
        const int count = 2 + static_cast<int>(rng() % 120);
        std::set<std::pair<int, int>> used;
        std::vector<Point> sites;

        while (static_cast<int>(sites.size()) < count) {
            const int x = 1 + static_cast<int>(rng() % 99999);
            const int y = 1 + static_cast<int>(rng() % 99999);
            if (used.insert({x, y}).second) {
                sites.push_back({static_cast<double>(x), static_cast<double>(y)});
            }
        }

        const auto pruned = voronoiCells(sites, 0, 0, 100000, 100000);
        const auto brute = voronoiCellsBrute(sites, 0, 0, 100000, 100000);
        checkDiagramProperties(sites, 0, 0, 100000, 100000, pruned);

        for (std::size_t i = 0; i < sites.size(); ++i) {
            assert(nearlyEqual(polygonArea(pruned.cells[i]), polygonArea(brute.cells[i]), 1e-9));
        }

        for (int query = 0; query < 50; ++query) {
            const Point point{
                static_cast<double>(rng() % 100001),
                static_cast<double>(rng() % 100001)
            };
            const int owner = nearestSite(sites, point);
            assert(insideConvex(pruned.cells[owner], point));
        }
    }

    std::cout << "Caso 5: poda coincide con fuerza bruta y consultas caen en su celda OK\n";
}

void testInvalidInput() {
    bool threwDuplicate = false;
    bool threwEmpty = false;
    bool threwDomain = false;

    try {
        voronoiCells({{1, 1}, {1, 1}}, 0, 0, 10, 10);
    } catch (const std::invalid_argument&) {
        threwDuplicate = true;
    }

    try {
        voronoiCells({}, 0, 0, 10, 10);
    } catch (const std::invalid_argument&) {
        threwEmpty = true;
    }

    try {
        voronoiCells({{1, 1}}, 0, 0, 0, 10);
    } catch (const std::invalid_argument&) {
        threwDomain = true;
    }

    assert(threwDuplicate && threwEmpty && threwDomain);
    assert(nearestSite({}, {0, 0}) == -1);

    std::cout << "Caso 6: entradas invalidas rechazadas OK\n";
}

int main() {
    testSingleSite();
    testTwoSites();
    testGrid();
    testCollinearSites();
    testPrunedMatchesBrute();
    testInvalidInput();

    std::cout << "Todas las pruebas de Voronoi pasaron.\n";
    return 0;
}
