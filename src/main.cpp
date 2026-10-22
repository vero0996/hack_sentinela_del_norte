// SENTINELA DEL NORTE - Nucleo algoritmico (TC2038)
//
// Descripcion: lee un objeto JSON {"mode": ..., "input": ...} desde la entrada
// estandar, ejecuta el algoritmo indicado por mode y escribe un unico objeto
// JSON con el resultado en la salida estandar.
//
// Modos implementados:
//   mst     -> arbol de expansion minima (Kruskal)     salida {"cost": X, "edges": [[u,v,w],...]}
//   tsp     -> Held-Karp o vecino cercano + 2-opt      salida {"cost": X, "route": [0,...,0]}
//   flow    -> flujo maximo (Dinic)                    salida {"flow": X}
//   voronoi -> regiones de Voronoi recortadas al dominio salida {"cells": [...]}
//   matching   -> firmas con KMP                        salida {"matches": [[...],[...]]}
//   palindrome -> palindromo mas largo (Manacher)       salida {"ranges": [[i,f],...]}
//   common     -> subcadena comun (automata de sufijos) salida {"range": [i,f]}
// Modo archivos: ./sentinel transmission1.txt transmission2.txt mcode1.txt mcode2.txt mcode3.txt
//
// Autores: Equipo SENTINELA DEL NORTE (ver team.txt)
// Matriculas: A0XXXXXXX, A0XXXXXXX, A0XXXXXXX, A0XXXXXXX
// Fecha de creacion: 2026-10-21
// Ultima modificacion: 2026-10-21
//
// Compilacion: g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic src/main.cpp -o sentinel
// Uso: ./sentinel < entrada.json
// Campos opcionales (usados por la aplicacion): input.algorithm elige la tecnica
// alternativa; toda respuesta agrega "elapsed_ms" (tiempo del algoritmo).

#include <chrono>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "common_substring.h"
#include "json.h"
#include "matching.h"
#include "max_flow.h"
#include "mst.h"
#include "palindrome.h"
#include "tsp.h"
#include "voronoi.h"

using namespace sentinela;

// Mide el tiempo transcurrido desde su creacion, en milisegundos.
class Stopwatch {
public:
	Stopwatch() : start_(std::chrono::steady_clock::now()) {}

	// Milisegundos transcurridos desde la creacion.
	double elapsedMs() const {
		std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - start_;
		return elapsed.count();
	}

private:
	std::chrono::steady_clock::time_point start_;
};

// Tecnica pedida en input.algorithm, o fallback si no se especifica.
std::string algorithmOf(const JsonValue& input, const std::string& fallback) {
	std::string algorithm = fallback;
	if (input.has("algorithm")) {
		algorithm = input.at("algorithm").asString();
	}
	return algorithm;
}

// Lanza si la tecnica pedida no es ninguna de las dos admitidas por el modo.
void requireAlgorithm(const std::string& algorithm, const std::string& first, const std::string& second) {
	if (algorithm != first && algorithm != second) {
		throw std::invalid_argument("Algoritmo desconocido: " + algorithm);
	}
}

// Agrega "elapsed_ms" al final de un objeto JSON ya formateado.
std::string withElapsed(const std::string& json, double elapsedMs) {
	std::ostringstream out;
	out << std::fixed << std::setprecision(3) << elapsedMs;
	return json.substr(0, json.size() - 1) + ",\"elapsed_ms\":" + out.str() + "}";
}

// Escribe una lista de enteros como arreglo JSON.
template <typename Number>
std::string formatList(const std::vector<Number>& values) {
	std::ostringstream out;
	out << "[";
	for (std::size_t i = 0; i < values.size(); ++i) {
		out << (i > 0 ? "," : "") << values[i];
	}
	out << "]";
	return out.str();
}

// Convierte {nodes, edges:[{u,v,weight}]} y calcula el arbol de expansion minima.
// algorithm: kruskal (por defecto) o prim.
std::string runMst(const JsonValue& input) {
	int n = static_cast<int>(input.at("nodes").size());
	std::vector<sentinel::MSTEdge> edges;
	for (const JsonValue& edge : input.at("edges").asArray()) {
		int from = static_cast<int>(edge.at("u").asInt());
		int to = static_cast<int>(edge.at("v").asInt());
		long long weight = edge.at("weight").asInt();
		edges.push_back({from, to, weight});
	}
	std::string algorithm = algorithmOf(input, "kruskal");
	requireAlgorithm(algorithm, "kruskal", "prim");
	Stopwatch timer;
	sentinel::MSTResult result = algorithm == "prim" ? sentinel::primMST(n, edges) : sentinel::kruskalMST(n, edges);
	double elapsed = timer.elapsedMs();
	if (!result.connected) {
		throw std::invalid_argument("El grafo no es conexo: no existe arbol de expansion");
	}
	std::ostringstream out;
	out << "{\"cost\":" << result.total_cost << ",\"edges\":[";
	for (std::size_t i = 0; i < result.edges.size(); ++i) {
		if (i > 0) {
			out << ",";
		}
		out << "[" << result.edges[i].u << "," << result.edges[i].v << "," << result.edges[i].weight << "]";
	}
	out << "]}";
	return withElapsed(out.str(), elapsed);
}

// Convierte {distances: matriz n x n} y calcula un ciclo hamiltoniano desde el nodo 0.
// algorithm: auto (por defecto), exact o heuristic.
std::string runTsp(const JsonValue& input) {
	std::vector<std::vector<long long>> graph;
	for (const JsonValue& row : input.at("distances").asArray()) {
		std::vector<long long> costs;
		for (const JsonValue& cost : row.asArray()) {
			costs.push_back(cost.asInt());
		}
		graph.push_back(costs);
	}
	std::string algorithm = algorithmOf(input, "auto");
	if (algorithm != "auto") {
		requireAlgorithm(algorithm, "exact", "heuristic");
	}
	Stopwatch timer;
	TSPResult result;
	if (algorithm == "exact") {
		result = solveTSPExact(graph);
	} else if (algorithm == "heuristic") {
		result = solveTSPHeuristic(graph);
	} else {
		result = solveTSP(graph);
	}
	double elapsed = timer.elapsedMs();
	if (!result.feasible) {
		throw std::invalid_argument("No existe un ciclo hamiltoniano desde el nodo 0");
	}
	std::string json = "{\"cost\":" + std::to_string(result.cost) + ",\"route\":" + formatList(result.tour) +
		",\"exact\":" + (result.exact ? "true" : "false") + "}";
	return withElapsed(json, elapsed);
}

// Convierte {n, edges:[{u,v,capacity}], source, sink} en aristas y calcula el flujo maximo.
// algorithm: dinic (por defecto) o edmonds-karp. Agrega el flujo por arista y el corte minimo.
std::string runFlow(const JsonValue& input) {
	int n = static_cast<int>(input.at("n").asInt());
	std::vector<FlowEdge> edges;
	for (const JsonValue& edge : input.at("edges").asArray()) {
		int from = static_cast<int>(edge.at("u").asInt());
		int to = static_cast<int>(edge.at("v").asInt());
		long long capacity = edge.at("capacity").asInt();
		edges.push_back({from, to, capacity});
	}
	int source = static_cast<int>(input.at("source").asInt());
	int sink = static_cast<int>(input.at("sink").asInt());
	std::string algorithm = algorithmOf(input, "dinic");
	requireAlgorithm(algorithm, "dinic", "edmonds-karp");
	Stopwatch timer;
	MaxFlowResult result = algorithm == "edmonds-karp" ? edmondsKarp(n, edges, source, sink) : maxFlow(n, edges, source, sink);
	double elapsed = timer.elapsedMs();
	std::string json = "{\"flow\":" + std::to_string(result.flow) + ",\"edge_flow\":" + formatList(result.edgeFlow) +
		",\"min_cut\":" + formatList(result.minCutSide) + "}";
	return withElapsed(json, elapsed);
}

// Escribe un poligono como arreglo JSON de pares [x, y].
std::string formatPolygon(const Polygon& polygon) {
	std::ostringstream out;
	out << "[";
	for (std::size_t i = 0; i < polygon.size(); ++i) {
		if (i > 0) {
			out << ",";
		}
		out << "[" << formatJsonNumber(polygon[i].x) << "," << formatJsonNumber(polygon[i].y) << "]";
	}
	out << "]";
	return out.str();
}

// Lee un arreglo JSON de pares [x, y].
std::vector<Point> readPoints(const JsonValue& array) {
	std::vector<Point> points;
	for (const JsonValue& point : array.asArray()) {
		points.push_back({point.at(0).asNumber(), point.at(1).asNumber()});
	}
	return points;
}

// Convierte {sites:[[x,y],...], bounds:[xmin,ymin,xmax,ymax]} y calcula las celdas de Voronoi.
// algorithm: pruned (por defecto) o brute. Si hay "queries", agrega el sitio mas cercano a cada una.
std::string runVoronoi(const JsonValue& input) {
	std::vector<Point> sites = readPoints(input.at("sites"));
	const JsonValue& BOUNDS = input.at("bounds");
	std::string algorithm = algorithmOf(input, "pruned");
	requireAlgorithm(algorithm, "pruned", "brute");
	Stopwatch timer;
	VoronoiResult result = algorithm == "brute" ?
		voronoiCellsBrute(sites, BOUNDS.at(0).asNumber(), BOUNDS.at(1).asNumber(), BOUNDS.at(2).asNumber(), BOUNDS.at(3).asNumber()) :
		voronoiCells(sites, BOUNDS.at(0).asNumber(), BOUNDS.at(1).asNumber(), BOUNDS.at(2).asNumber(), BOUNDS.at(3).asNumber());
	std::vector<int> owners;
	if (input.has("queries")) {
		for (Point& query : readPoints(input.at("queries"))) {
			owners.push_back(nearestSite(sites, query));
		}
	}
	double elapsed = timer.elapsedMs();
	std::ostringstream out;
	out << "{\"cells\":[";
	for (std::size_t i = 0; i < result.cells.size(); ++i) {
		if (i > 0) {
			out << ",";
		}
		out << formatPolygon(result.cells[i]);
	}
	out << "]";
	if (input.has("queries")) {
		out << ",\"owners\":" << formatList(owners);
	}
	out << "}";
	return withElapsed(out.str(), elapsed);
}

// Lee un arreglo JSON de cadenas.
std::vector<std::string> readStrings(const JsonValue& array) {
	std::vector<std::string> values;
	for (const JsonValue& item : array.asArray()) {
		values.push_back(item.asString());
	}
	return values;
}

// Escribe un rango [inicio, fin] como arreglo JSON.
std::string formatRange(const std::pair<int, int>& range) {
	return "[" + std::to_string(range.first) + "," + std::to_string(range.second) + "]";
}

// Busca cada firma en cada transmision: {transmissions, patterns} -> {"matches": [[...],...]}.
// algorithm: kmp (por defecto) o z.
std::string runMatching(const JsonValue& input) {
	std::vector<std::string> transmissions = readStrings(input.at("transmissions"));
	std::vector<std::string> patterns = readStrings(input.at("patterns"));
	std::string algorithm = algorithmOf(input, "kmp");
	requireAlgorithm(algorithm, "kmp", "z");
	Stopwatch timer;
	std::vector<std::vector<int>> matches = matchAll(transmissions, patterns, algorithm == "z");
	double elapsed = timer.elapsedMs();
	std::ostringstream out;
	out << "{\"matches\":[";
	for (std::size_t t = 0; t < matches.size(); ++t) {
		out << (t > 0 ? "," : "") << formatList(matches[t]);
	}
	out << "]}";
	return withElapsed(out.str(), elapsed);
}

// Palindromo mas largo de cada transmision: {transmissions} -> {"ranges": [[i,f],...]}.
// algorithm: manacher (por defecto) o expand.
std::string runPalindrome(const JsonValue& input) {
	std::vector<std::string> transmissions = readStrings(input.at("transmissions"));
	std::string algorithm = algorithmOf(input, "manacher");
	requireAlgorithm(algorithm, "manacher", "expand");
	Stopwatch timer;
	std::vector<std::pair<int, int>> ranges;
	for (std::string& transmission : transmissions) {
		ranges.push_back(algorithm == "expand" ? longestPalindromeExpand(transmission) : longestPalindromeManacher(transmission));
	}
	double elapsed = timer.elapsedMs();
	std::ostringstream out;
	out << "{\"ranges\":[";
	for (std::size_t t = 0; t < ranges.size(); ++t) {
		out << (t > 0 ? "," : "") << formatRange(ranges[t]);
	}
	out << "]}";
	return withElapsed(out.str(), elapsed);
}

// Subcadena comun mas larga: {transmissions:[t1,t2]} -> {"range": [i,f]} en t1.
// algorithm: automaton (por defecto) o dp.
std::string runCommon(const JsonValue& input) {
	std::vector<std::string> transmissions = readStrings(input.at("transmissions"));
	if (transmissions.size() != 2) {
		throw std::invalid_argument("Se requieren exactamente dos transmisiones");
	}
	std::string algorithm = algorithmOf(input, "automaton");
	requireAlgorithm(algorithm, "automaton", "dp");
	Stopwatch timer;
	std::pair<int, int> range = algorithm == "dp" ?
		longestCommonSubstringDP(transmissions[0], transmissions[1]) :
		longestCommonSubstring(transmissions[0], transmissions[1]);
	double elapsed = timer.elapsedMs();
	return withElapsed("{\"range\":" + formatRange(range) + "}", elapsed);
}

// Elige el algoritmo segun mode y regresa la respuesta JSON.
std::string dispatch(const std::string& mode, const JsonValue& input) {
	std::string answer;
	if (mode == "mst") {
		answer = runMst(input);
	} else if (mode == "tsp") {
		answer = runTsp(input);
	} else if (mode == "flow") {
		answer = runFlow(input);
	} else if (mode == "voronoi") {
		answer = runVoronoi(input);
	} else if (mode == "matching") {
		answer = runMatching(input);
	} else if (mode == "palindrome") {
		answer = runPalindrome(input);
	} else if (mode == "common") {
		answer = runCommon(input);
	} else {
		throw std::invalid_argument("Modo no implementado: " + mode);
	}
	return answer;
}

// Lee un archivo y lo normaliza: ignora espacios y saltos de linea, pasa a mayusculas.
std::string readNormalized(const std::string& path) {
	std::ifstream file(path);
	if (!file) {
		throw std::runtime_error("No se pudo abrir " + path);
	}
	std::string normalized;
	char symbol = ' ';
	while (file.get(symbol)) {
		if (!std::isspace(static_cast<unsigned char>(symbol))) {
			normalized.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(symbol))));
		}
	}
	return normalized;
}

// Modo archivos: imprime el formato de texto de la especificacion (9 lineas).
// Firmas: 3 lineas por transmision, "true posicion" o "false".
// Palindromos: "inicio fin" por transmision. Subcadena comun: "inicio fin" en la primera.
void runFiles(char* argv[]) {
	std::vector<std::string> transmissions = {readNormalized(argv[1]), readNormalized(argv[2])};
	std::vector<std::string> codes = {readNormalized(argv[3]), readNormalized(argv[4]), readNormalized(argv[5])};
	std::vector<std::vector<int>> matches = matchAll(transmissions, codes);
	for (std::vector<int>& row : matches) {
		for (int position : row) {
			if (position > 0) {
				std::cout << "true " << position << "\n";
			} else {
				std::cout << "false\n";
			}
		}
	}
	for (std::string& transmission : transmissions) {
		std::pair<int, int> range = longestPalindromeManacher(transmission);
		std::cout << range.first << " " << range.second << "\n";
	}
	std::pair<int, int> common = longestCommonSubstring(transmissions[0], transmissions[1]);
	std::cout << common.first << " " << common.second << "\n";
}

// Lee la solicitud JSON de stdin, despacha y reporta errores como JSON.
int runJson() {
	std::string text((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
	int status = 0;
	try {
		JsonValue request = parseJson(text);
		std::cout << dispatch(request.at("mode").asString(), request.at("input")) << "\n";
	} catch (const std::exception& error) {
		std::cout << "{\"error\":" << formatJsonString(error.what()) << "}\n";
		status = 1;
	}
	return status;
}

// Punto de entrada: sin argumentos usa el protocolo JSON; con 5 archivos usa el modo archivos.
int main(int argc, char* argv[]) {
	std::ios::sync_with_stdio(false);
	int status = 0;
	if (argc == 1) {
		status = runJson();
	} else if (argc == 6) {
		try {
			runFiles(argv);
		} catch (const std::exception& error) {
			std::cerr << "Error: " << error.what() << "\n";
			status = 1;
		}
	} else {
		std::cerr << "Uso: sentinel < solicitud.json\n";
		std::cerr << "     sentinel <transmission1> <transmission2> <mcode1> <mcode2> <mcode3>\n";
		status = 1;
	}
	return status;
}
