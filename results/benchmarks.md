# Resultados de benchmarks

Generado por `bench/benchmark.py` el 2026-10-21T19:33:08. Mediana de 3 corridas; `elapsed_ms` es el tiempo del algoritmo medido en C++ (sin E/S JSON), `wall_ms` el del proceso completo y `RSS` la memoria residente maxima del proceso. Ambiente en [environment.md](environment.md).

## mst

Complejidad teorica: O(E log E) / O(E log V), E = 7n.

| tecnica | n | elapsed_ms | wall_ms | RSS (MB) | coincide |
|---|---:|---:|---:|---:|:---:|
| kruskal | 500 | 0.133 | 13.0 | 4.6 | si |
| prim | 500 | 0.302 | 13.3 | 4.9 | si |
| kruskal | 2000 | 0.660 | 45.1 | 12.5 | si |
| prim | 2000 | 1.328 | 47.2 | 13.5 | si |
| kruskal | 8000 | 2.844 | 178.6 | 44.4 | si |
| prim | 8000 | 6.078 | 185.5 | 48.3 | si |
| kruskal | 32000 | 12.568 | 739.6 | 166.1 | si |
| prim | 32000 | 29.727 | 757.1 | 183.5 | si |

- Pendiente log-log de `kruskal`: **1.09** (tiempo ~ n^1.09).
- Pendiente log-log de `prim`: **1.10** (tiempo ~ n^1.10).

## tsp

Complejidad teorica: exacto O(n^2 2^n); heuristica O(n^2) por pasada.

| tecnica | n | elapsed_ms | wall_ms | RSS (MB) | coincide |
|---|---:|---:|---:|---:|:---:|
| exact | 8 | 0.024 | 2.2 | 1.7 | si |
| heuristic | 8 | 0.001 | 2.2 | 1.7 | si |
| exact | 10 | 0.113 | 2.1 | 2.0 | si |
| heuristic | 10 | 0.001 | 2.1 | 1.8 | si |
| exact | 12 | 0.536 | 2.7 | 2.6 | si |
| heuristic | 12 | 0.001 | 2.1 | 1.8 | si |
| exact | 14 | 2.716 | 4.9 | 5.4 | si |
| heuristic | 14 | 0.002 | 2.1 | 1.8 | si |
| exact | 16 | 13.186 | 16.1 | 16.9 | si |
| heuristic | 16 | 0.002 | 2.2 | 1.8 | si |
| heuristic | 100 | 0.040 | 7.4 | 3.5 |  |
| heuristic | 200 | 0.194 | 20.9 | 8.1 |  |
| heuristic | 400 | 0.660 | 79.5 | 25.1 |  |
| heuristic | 800 | 2.763 | 305.2 | 80.5 |  |

- Pendiente log-log de `exact`: **9.07** (tiempo ~ n^9.07).
- Pendiente log-log de `heuristic`: **2.01** (tiempo ~ n^2.01).

## flow

Complejidad teorica: Dinic O(V^2 E); Edmonds-Karp O(V E^2), E = 20n.

| tecnica | n | elapsed_ms | wall_ms | RSS (MB) | coincide |
|---|---:|---:|---:|---:|:---:|
| dinic | 100 | 0.127 | 9.4 | 3.7 | si |
| edmonds-karp | 100 | 0.197 | 9.0 | 3.7 | si |
| dinic | 200 | 0.315 | 15.3 | 5.3 | si |
| edmonds-karp | 200 | 0.298 | 15.2 | 5.3 | si |
| dinic | 400 | 0.651 | 28.6 | 8.5 | si |
| edmonds-karp | 400 | 0.851 | 29.1 | 8.5 | si |
| dinic | 800 | 1.043 | 55.0 | 15.7 | si |
| edmonds-karp | 800 | 0.726 | 52.2 | 15.7 | si |
| dinic | 1600 | 2.049 | 107.1 | 28.0 | si |
| edmonds-karp | 1600 | 5.048 | 110.6 | 28.0 | si |

- Pendiente log-log de `dinic`: **0.98** (tiempo ~ n^0.98).
- Pendiente log-log de `edmonds-karp`: **1.06** (tiempo ~ n^1.06).

## voronoi

Complejidad teorica: poda ~O(n^2 log n) peor caso; sin poda O(n^2 k).

| tecnica | n | elapsed_ms | wall_ms | RSS (MB) | coincide |
|---|---:|---:|---:|---:|:---:|
| pruned | 100 | 0.291 | 3.3 | 2.2 | si |
| brute | 100 | 0.791 | 3.6 | 2.2 | si |
| pruned | 250 | 1.617 | 5.1 | 2.6 | si |
| brute | 250 | 4.894 | 8.4 | 2.6 | si |
| pruned | 500 | 6.618 | 11.6 | 3.0 | si |
| brute | 500 | 19.713 | 24.8 | 2.9 | si |
| pruned | 1000 | 28.690 | 37.3 | 3.8 | si |
| brute | 1000 | 75.716 | 84.2 | 3.9 | si |
| pruned | 2000 | 127.091 | 141.8 | 5.6 | si |
| brute | 2000 | 330.392 | 344.6 | 5.6 | si |

- Pendiente log-log de `pruned`: **2.03** (tiempo ~ n^2.03).
- Pendiente log-log de `brute`: **2.01** (tiempo ~ n^2.01).

## matching

Complejidad teorica: O(n + m).

| tecnica | n | elapsed_ms | wall_ms | RSS (MB) | coincide |
|---|---:|---:|---:|---:|:---:|
| kmp | 10000 | 0.096 | 4.0 | 1.9 | si |
| z | 10000 | 0.087 | 3.6 | 1.9 | si |
| kmp | 100000 | 0.932 | 18.1 | 2.8 | si |
| z | 100000 | 0.800 | 17.8 | 3.2 | si |
| kmp | 1000000 | 9.354 | 165.9 | 11.1 | si |
| z | 1000000 | 7.908 | 158.2 | 15.2 | si |
| kmp | 4000000 | 37.437 | 626.5 | 33.1 | si |
| z | 4000000 | 32.599 | 624.7 | 52.2 | si |

- Pendiente log-log de `kmp`: **1.00** (tiempo ~ n^1.00).
- Pendiente log-log de `z`: **0.99** (tiempo ~ n^0.99).

## palindrome

Complejidad teorica: Manacher O(n); expansion O(n^2) peor caso.

| tecnica | n | elapsed_ms | wall_ms | RSS (MB) | coincide |
|---|---:|---:|---:|---:|:---:|
| manacher | 10000 | 0.070 | 4.5 | 2.0 | si |
| expand | 10000 | 0.052 | 4.2 | 1.9 | si |
| manacher | 100000 | 0.629 | 17.9 | 3.5 | si |
| expand | 100000 | 0.504 | 17.6 | 2.7 | si |
| manacher | 1000000 | 6.144 | 153.8 | 18.1 | si |
| expand | 1000000 | 4.946 | 151.0 | 10.4 | si |

- Pendiente log-log de `manacher`: **0.97** (tiempo ~ n^0.97).
- Pendiente log-log de `expand`: **0.99** (tiempo ~ n^0.99).

## palindrome-worst

Complejidad teorica: cadena 'AAAA...': peor caso de la expansion.

| tecnica | n | elapsed_ms | wall_ms | RSS (MB) | coincide |
|---|---:|---:|---:|---:|:---:|
| manacher | 1000 | 0.006 | 2.7 | 1.7 | si |
| expand | 1000 | 0.202 | 3.0 | 1.7 | si |
| manacher | 4000 | 0.018 | 2.9 | 1.8 | si |
| expand | 4000 | 3.068 | 5.9 | 1.7 | si |
| manacher | 16000 | 0.078 | 3.9 | 2.0 | si |
| expand | 16000 | 49.235 | 54.0 | 1.8 | si |
| manacher | 64000 | 0.280 | 7.8 | 2.6 | si |
| expand | 64000 | 761.828 | 770.0 | 2.1 | si |

- Pendiente log-log de `manacher`: **0.94** (tiempo ~ n^0.94).
- Pendiente log-log de `expand`: **1.98** (tiempo ~ n^1.98).

## common

Complejidad teorica: automata O((n+m) sigma); DP O(n m).

| tecnica | n | elapsed_ms | wall_ms | RSS (MB) | coincide |
|---|---:|---:|---:|---:|:---:|
| automaton | 1000 | 0.035 | 2.7 | 1.9 | si |
| dp | 1000 | 1.281 | 3.7 | 1.7 | si |
| dp | 2000 | 5.241 | 7.8 | 1.8 |  |
| automaton | 4000 | 0.130 | 3.0 | 2.2 | si |
| dp | 4000 | 21.589 | 24.9 | 1.8 | si |
| dp | 8000 | 89.148 | 93.3 | 2.0 |  |
| automaton | 16000 | 0.553 | 5.5 | 3.7 |  |
| automaton | 64000 | 3.093 | 16.6 | 9.1 |  |
| automaton | 256000 | 15.903 | 63.4 | 30.6 |  |

- Pendiente log-log de `automaton`: **1.11** (tiempo ~ n^1.11).
- Pendiente log-log de `dp`: **2.04** (tiempo ~ n^2.04).

## Casos limite

| caso | modo | respuesta |
|---|---|---|
| MST grafo no conexo | mst | `{"error": "El grafo no es conexo: no existe arbol de expansion"}` |
| MST un nodo | mst | `{"cost": 0, "edges": []}` |
| TSP un nodo | tsp | `{"cost": 0, "route": [0, 0], "exact": true}` |
| TSP sin ciclo hamiltoniano | tsp | `{"error": "No existe un ciclo hamiltoniano desde el nodo 0"}` |
| TSP exacto con 21 nodos | tsp | `{"error": "TSP exacto admite como maximo 20 nodos"}` |
| Flujo origen = destino | flow | `{"flow": 0, "edge_flow": [0], "min_cut": [1, 1]}` |
| Flujo sin camino | flow | `{"flow": 0, "edge_flow": [0], "min_cut": [1, 1, 0]}` |
| Flujo capacidad negativa | flow | `{"error": "Las capacidades no pueden ser negativas"}` |
| Voronoi un sitio | voronoi | `{"cells": [[[0, 0], [10, 0], [10, 10], [0, 10]]]}` |
| Voronoi sitios repetidos | voronoi | `{"error": "Hay sitios repetidos"}` |
| Voronoi sitios colineales | voronoi | `{"cells": [[[0, 0], [2, 0], [1.9999999999999996, 10], [0, 10]], [[2, 0], [5, 0], [5, 10], [1.9999999999999996,…` |
| Firma vacia y firma mas larga que el texto | matching | `{"matches": [[1, -1]]}` |
| Firma al inicio y al final | matching | `{"matches": [[1, 5]]}` |
| Palindromo de cadena vacia | palindrome | `{"ranges": [[0, 0], [1, 1]]}` |
| Palindromo con empate | palindrome | `{"ranges": [[1, 3]]}` |
| Subcadena sin caracteres comunes | common | `{"range": [0, 0]}` |
| Subcadena con caracteres no hex | common | `{"range": [1, 3]}` |
