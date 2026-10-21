# SENTINEL DEL NORTE

Hackathon integrador — TC2038
Lenguaje del núcleo algorítmico: C++17

## Arquitectura

El ejecutable procesa un único objeto JSON desde stdin y devuelve
un único objeto JSON por stdout. El campo `mode` identifica el
algoritmo y `input` contiene sus datos.

## Módulos

- `src/main.cpp`: entrada, despacho de modos y salida JSON.
- `src/json.h`: soporte para procesamiento JSON.
- `src/mst.h`: árbol de expansión mínima.
- `src/tsp.h`: problema del viajante.
- `src/max_flow.h`: flujo máximo.
- `src/voronoi.h`: geometría computacional y proximidad.
- `src/matching.h`: búsqueda de patrones.
- `src/palindrome.h`: subcadena palíndroma más larga.
- `src/common_substring.h`: subcadena común más larga.

## Network Optimization

### MST
Algoritmo principal propuesto: Kruskal con Disjoint Set Union.
Alternativa para evaluación: Prim con heap.
Complejidad esperada de Kruskal: O(E log E).

### TSP
Algoritmo exacto propuesto: Held-Karp para instancias pequeñas.
Alternativa escalable: vecino más cercano con mejora 2-opt.
Complejidad de Held-Karp: O(n^2 * 2^n) en tiempo y
O(n * 2^n) en memoria.

## Validación pendiente

- Confirmar el esquema JSON exacto del autograder.
- Obtener y revisar los archivos oficiales de entrada.
- Comparar las implementaciones con casos pequeños de referencia.
- Ejecutar pruebas de estrés y registrar tiempos y memoria.
- Documentar compilador, opciones y procedimiento de ejecución.

## Trazabilidad de datos

La topología debe identificarse como procedente de SNDlib.
Las transformaciones y los escenarios de simulación deben documentarse
por separado de los datos originales.
