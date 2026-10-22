#ifndef VORONOI_H
#define VORONOI_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace sentinela {

struct Point {
	double x;
	double y;
};

using Polygon = std::vector<Point>;

struct VoronoiResult {
	// cells[i] es la region de sites[i] recortada al dominio, en orden antihorario.
	std::vector<Polygon> cells;
};

namespace detail {

constexpr double GEOMETRY_EPS = 1e-9;

// Distancia euclidiana al cuadrado entre dos puntos.
inline double squaredDistance(const Point& a, const Point& b) {
	double dx = a.x - b.x;
	double dy = a.y - b.y;
	return dx * dx + dy * dy;
}

// Valida que el dominio sea un rectangulo no degenerado y que no haya sitios repetidos.
inline void validateVoronoiInput(
	const std::vector<Point>& sites,
	double xmin,
	double ymin,
	double xmax,
	double ymax
) {
	if (sites.empty()) {
		throw std::invalid_argument("Se requiere al menos un sitio");
	}
	if (!(xmin < xmax) || !(ymin < ymax)) {
		throw std::invalid_argument("El dominio debe tener area positiva");
	}
	std::vector<std::pair<double, double>> sorted;
	sorted.reserve(sites.size());
	for (const Point& site : sites) {
		sorted.push_back({site.x, site.y});
	}
	std::sort(sorted.begin(), sorted.end());
	if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end()) {
		throw std::invalid_argument("Hay sitios repetidos");
	}
}

// Rectangulo del dominio en orden antihorario.
inline Polygon boundsPolygon(double xmin, double ymin, double xmax, double ymax) {
	return {{xmin, ymin}, {xmax, ymin}, {xmax, ymax}, {xmin, ymax}};
}

}  // namespace detail

// Sutherland-Hodgman contra el semiplano a*x + b*y <= c.
// Conserva el orden de los vertices; regresa un poligono vacio si no queda area.
inline Polygon clipHalfPlane(const Polygon& polygon, double a, double b, double c) {
	Polygon clipped;
	std::size_t count = polygon.size();
	for (std::size_t i = 0; i < count; ++i) {
		const Point& current = polygon[i];
		const Point& next = polygon[(i + 1) % count];
		double currentSide = a * current.x + b * current.y - c;
		double nextSide = a * next.x + b * next.y - c;
		if (currentSide <= detail::GEOMETRY_EPS) {
			clipped.push_back(current);
		}
		bool crosses = (currentSide < -detail::GEOMETRY_EPS && nextSide > detail::GEOMETRY_EPS) ||
			(currentSide > detail::GEOMETRY_EPS && nextSide < -detail::GEOMETRY_EPS);
		if (crosses) {
			double t = currentSide / (currentSide - nextSide);
			clipped.push_back({current.x + t * (next.x - current.x), current.y + t * (next.y - current.y)});
		}
	}
	return clipped;
}

// Area de un poligono simple (formula del zapato), siempre no negativa.
inline double polygonArea(const Polygon& polygon) {
	double twiceArea = 0.0;
	std::size_t count = polygon.size();
	for (std::size_t i = 0; i < count; ++i) {
		const Point& current = polygon[i];
		const Point& next = polygon[(i + 1) % count];
		twiceArea += current.x * next.y - next.x * current.y;
	}
	return std::fabs(twiceArea) / 2.0;
}

namespace detail {

// Recorta la celda de site con la mediatriz respecto a other.
inline Polygon clipByBisector(const Polygon& cell, const Point& site, const Point& other) {
	double a = 2.0 * (other.x - site.x);
	double b = 2.0 * (other.y - site.y);
	double c = other.x * other.x + other.y * other.y - site.x * site.x - site.y * site.y;
	return clipHalfPlane(cell, a, b, c);
}

// Radio de la celda: distancia maxima de site a cualquiera de sus vertices.
inline double cellRadius(const Polygon& cell, const Point& site) {
	double best = 0.0;
	for (const Point& vertex : cell) {
		best = std::max(best, squaredDistance(vertex, site));
	}
	return std::sqrt(best);
}

}  // namespace detail

// Diagrama de Voronoi recortado al rectangulo [xmin, xmax] x [ymin, ymax].
// Cada celda es la interseccion del dominio con los semiplanos de las mediatrices.
// Poda: los demas sitios se visitan por distancia creciente y se detiene cuando
// la mediatriz queda mas lejos que el radio de la celda (ya no puede recortarla).
// Peor caso O(n^2 log n); en sitios bien distribuidos cada celda toca pocos vecinos.
inline VoronoiResult voronoiCells(
	const std::vector<Point>& sites,
	double xmin,
	double ymin,
	double xmax,
	double ymax
) {
	detail::validateVoronoiInput(sites, xmin, ymin, xmax, ymax);
	VoronoiResult result;
	result.cells.reserve(sites.size());
	std::vector<std::pair<double, std::size_t>> byDistance(sites.size());
	for (std::size_t i = 0; i < sites.size(); ++i) {
		for (std::size_t j = 0; j < sites.size(); ++j) {
			byDistance[j] = {detail::squaredDistance(sites[i], sites[j]), j};
		}
		std::sort(byDistance.begin(), byDistance.end());

		Polygon cell = detail::boundsPolygon(xmin, ymin, xmax, ymax);
		double radius = detail::cellRadius(cell, sites[i]);
		std::size_t k = 1;
		while (k < byDistance.size() && !cell.empty() && std::sqrt(byDistance[k].first) <= 2.0 * radius) {
			cell = detail::clipByBisector(cell, sites[i], sites[byDistance[k].second]);
			radius = detail::cellRadius(cell, sites[i]);
			++k;
		}
		result.cells.push_back(cell);
	}
	return result;
}

// Tecnica de comparacion: recorta cada celda con todas las mediatrices, sin poda. O(n^2).
inline VoronoiResult voronoiCellsBrute(
	const std::vector<Point>& sites,
	double xmin,
	double ymin,
	double xmax,
	double ymax
) {
	detail::validateVoronoiInput(sites, xmin, ymin, xmax, ymax);
	VoronoiResult result;
	result.cells.reserve(sites.size());
	for (std::size_t i = 0; i < sites.size(); ++i) {
		Polygon cell = detail::boundsPolygon(xmin, ymin, xmax, ymax);
		for (std::size_t j = 0; j < sites.size(); ++j) {
			if (j != i) {
				cell = detail::clipByBisector(cell, sites[i], sites[j]);
			}
		}
		result.cells.push_back(cell);
	}
	return result;
}

// Consulta de ubicacion: indice del sitio mas cercano a query.
// En empate gana el indice menor. Regresa -1 si no hay sitios.
inline int nearestSite(const std::vector<Point>& sites, const Point& query) {
	int best = -1;
	double bestDistance = 0.0;
	for (std::size_t i = 0; i < sites.size(); ++i) {
		double distance = detail::squaredDistance(sites[i], query);
		if (best == -1 || distance < bestDistance) {
			best = static_cast<int>(i);
			bestDistance = distance;
		}
	}
	return best;
}

}  // namespace sentinela

#endif  // VORONOI_H
