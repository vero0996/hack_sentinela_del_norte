# SENTINEL DEL NORTE

Hackathon integrador — TC2038
Lenguaje del núcleo algorítmico: C++17

## Inicio rápido

Requisitos: un compilador C++17 (g++ o clang++), Python 3.8+ y `make`. Sin
dependencias externas ni `pip install`.

```bash
make            # compila ./sentinel (núcleo C++)
make test       # 7 suites unitarias + verificador del autograder + modo archivos
make app        # SENTINEL Digital Twin en http://127.0.0.1:8000
make bench      # benchmarks reproducibles -> results/
make grader-zip       # dist/sentinela_grader.zip (solo src, README, team.txt, test sin .cpp)
make deliverable-zip  # dist/sentinela_entrega.zip (paquete completo para Canvas)
```

## Estructura

```
src/          núcleo C++: main.cpp + encabezados (header-only)
app/          Digital Twin: server.py, static/ (HTML/JS/Leaflet), data/ (SNDlib geant + simulación)
tests/        pruebas unitarias C++, grader_check.py, samples/ del modo archivos
bench/        benchmark.py
results/      benchmarks.md, benchmarks.csv, environment.md
reflection.md fuente de ReflexSENTINEL_….pdf
defense.md    guía de estudio para la defensa técnica
```

## Arquitectura

El ejecutable procesa un único objeto JSON desde stdin y devuelve
un único objeto JSON por stdout. El campo `mode` identifica el
algoritmo y `input` contiene sus datos. Campos opcionales que el autograder
no envía: `input.algorithm` (técnica alternativa, p. ej. `"prim"`,
`"edmonds-karp"`, `"dp"`) y `input.queries` en Voronoi (central más cercana).
Toda respuesta agrega `elapsed_ms` (tiempo del algoritmo medido en C++); `flow`
agrega `edge_flow` y `min_cut`, y `tsp` agrega `exact`.

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

## Compilación y ejecución

El autograder del sitio compila **únicamente** `src/main.cpp` junto con los
encabezados `src/*.h` (clang → wasm, `-std=c++17 -O2 -Wall -Wextra -Wpedantic`).
Por eso todos los módulos son *header-only*: no debe existir otro `.cpp` en `src/`.

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic src/main.cpp -o sentinel
echo '{"mode":"flow","input":{"n":2,"edges":[{"u":0,"v":1,"capacity":5}],"source":0,"sink":1}}' | ./sentinel
# {"flow":5}
```

Si el modo no existe o la entrada es inválida, el programa escribe
`{"error": "..."}` y termina con código 1.

### Protocolo por modo

| mode | input | salida |
|------|-------|--------|
| `mst` | `{nodes:[0..n-1], edges:[{u,v,weight}]}` | `{cost, edges:[[u,v,w],...]}` |
| `tsp` | `{distances: matriz n×n}` | `{cost, route:[0,...,0]}` |
| `flow` | `{n, edges:[{u,v,capacity}], source, sink}` (aristas dirigidas) | `{flow}` |
| `voronoi` | `{sites:[[x,y],...], bounds:[xmin,ymin,xmax,ymax]}` | `{cells:[[[x,y],...],...]}` |
| `matching` | `{transmissions:[t1,t2], patterns:[p1,...]}` | `{matches:[[pos,...],[pos,...]]}` (base 1, -1 = no aparece) |
| `palindrome` | `{transmissions:[t1,...]}` | `{ranges:[[inicio,fin],...]}` |
| `common` | `{transmissions:[t1,t2]}` | `{range:[inicio,fin]}` (posiciones en t1) |

### Modo archivos

```bash
./sentinel transmission1.txt transmission2.txt mcode1.txt mcode2.txt mcode3.txt
```

Normaliza cada archivo (ignora espacios y saltos de línea, pasa a mayúsculas) e
imprime el formato de texto de la especificación (9 líneas):

1. 6 líneas de firmas: mcode1..3 en transmission1 y luego en transmission2,
   cada una `true <posición>` o `false`.
2. 2 líneas `<inicio> <fin>`: palíndromo más largo de cada transmisión.
3. 1 línea `<inicio> <fin>`: subcadena común más larga, en transmission1.

Ejemplo verificable en `tests/samples/` (`expected_output.txt`).

## Pruebas

```bash
make test
# equivalente manual:
for t in mst tsp max_flow voronoi matching palindrome common_substring; do
  g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic tests/test_$t.cpp -o test_$t && ./test_$t
done
python3 tests/grader_check.py ./sentinel src/main.cpp
```

`tests/grader_check.py` replica las referencias, generadores (perfiles lite,
normal y huge) y validadores del autograder para los siete modos, además del
linter de estilo aplicado a `main.cpp`.

> Los archivos `tests/*.cpp` no deben incluirse en el ZIP de entrega: el
> autograder exige exactamente un `.cpp` (`src/main.cpp`).

## Network Optimization

### MST (`src/mst.h`)
- **Elegido: Kruskal + Disjoint Set Union** (compresión de caminos y unión por
  rango). O(E log E) tiempo, O(V + E) memoria. Correcto por la propiedad de
  corte: la arista más ligera que cruza cualquier corte pertenece a algún MST.
  Desempate determinista por (peso, u, v).
- **Alternativa: Prim con heap binario** (`primMST`). O(E log V). Se prefiere
  Kruskal porque las entradas son listas de aristas dispersas (E ≈ 7V en la
  prueba grande) y no requiere construir listas de adyacencia; Prim conviene en
  grafos densos. Ambos producen el mismo costo (verificado en 200 grafos aleatorios).
- Si el grafo no es conexo no existe árbol de expansión; el modo `mst` reporta error.

### TSP (`src/tsp.h`)
- **Exacto: Held-Karp** (`solveTSPExact`), programación dinámica sobre
  subconjuntos. O(n²·2ⁿ) tiempo, O(n·2ⁿ) memoria. Se usa hasta
  `TSP_EXACT_LIMIT = 15` nodos (~6 MB); con 20 nodos superaría 250 MB, demasiado
  para el compilador web.
- **Aproximado: vecino más cercano + 2-opt** (`solveTSPHeuristic`). Vecino más
  cercano O(n²) construye un recorrido inicial; 2-opt de primera mejora invierte
  tramos mientras el costo baje, O(n²) por pasada. No garantiza el óptimo (en la
  métrica euclidiana el vecino más cercano puede ser Θ(log n) veces el óptimo; 2-opt
  lo reduce mucho en la práctica). En 450 nodos sobre un círculo obtiene el
  perímetro óptimo.
- `solveTSP` elige exacto o aproximado según n; el resultado indica `exact`.
- Limitaciones: 2-opt solo se aplica a matrices simétricas; en grafos
  incompletos (`-1`) la heurística puede no encontrar un ciclo aunque exista.

## Flow & Computational Geometry

### Flujo máximo (`src/max_flow.h`)
- **Elegido: Dinic** (`maxFlow`). BFS construye el grafo de niveles y un DFS
  con punteros de arista envía flujo bloqueante. O(V²·E) tiempo, O(V + E) memoria.
- **Alternativa: Edmonds-Karp** (`edmondsKarp`), camino aumentante más corto
  por BFS. O(V·E²). Dinic gana porque cada fase satura todos los caminos de la
  misma longitud; ambos coinciden en 200 redes aleatorias.
- Correctitud: teorema de flujo máximo / corte mínimo. El resultado incluye el
  flujo por arista (`edgeFlow`, para dibujarlo sobre la topología) y el lado
  de cada nodo en el corte mínimo (`minCutSide`). Las pruebas verifican
  capacidad, conservación y que la capacidad del corte sea igual al flujo.
- Capacidad del enlace ≠ demanda de tráfico: el algoritmo solo usa capacidades;
  las demandas son datos del escenario para comparar contra el flujo máximo.

### Voronoi (`src/voronoi.h`)
- **Elegido: intersección de semiplanos por sitio** (`voronoiCells`). Cada
  celda empieza como el rectángulo del dominio y se recorta (Sutherland-Hodgman)
  con la mediatriz respecto a cada otro sitio. Poda: los demás sitios se visitan
  por distancia creciente y se detiene cuando la distancia supera el doble del
  radio de la celda, porque esa mediatriz ya no puede cortarla.
  O(n² log n): la poda reduce el recorte (≈ 2.6× menos tiempo que sin poda con
  2 000 sitios), pero ordenar las distancias para cada sitio domina y el tiempo
  medido crece como n^2.03 (ver `results/benchmarks.md`). Un índice espacial
  (rejilla o k-d tree) lo acercaría a O(n log n). Robusto y recorta
  directamente al dominio.
- **Alternativa: misma construcción sin poda** (`voronoiCellsBrute`, O(n²·k))
  como oráculo; Fortune (barrido, O(n log n)) o el dual de Delaunay serían
  óptimos asintóticamente pero son más frágiles numéricamente y requieren recorte
  posterior al dominio. Con n ≤ 500 la diferencia no es relevante.
- Consulta de ubicación: `nearestSite` (O(n), empate al índice menor).
- Tolerancia `GEOMETRY_EPS = 1e-9`. Propiedades verificadas sin conocer la
  respuesta: las áreas suman el dominio, cada celda contiene su sitio y cada
  vértice no está más cerca de otro sitio que del propio.

## Cybersecurity Intelligence

Las transmisiones son hexadecimal en mayúsculas; las posiciones son en base 1
sobre la secuencia normalizada. Detectar una firma no confirma una infección real
(`mapping.csv` es una construcción de simulación).

### Firmas (`src/matching.h`)
- **Elegido: Knuth-Morris-Pratt** (`findFirstKMP`). La función de prefijo evita
  retroceder en el texto: O(n + m) tiempo, O(m) memoria. Se detiene en la
  primera aparición.
- **Alternativa: algoritmo Z** (`findFirstZ`) sobre `patrón + texto`. También
  O(n + m), pero necesita O(n + m) memoria (≈ 1 M enteros por transmisión
  grande), por eso se prefiere KMP. Rabin-Karp sería O(n + m) esperado pero con
  riesgo de colisiones; Aho-Corasick convendría con muchas firmas (aquí son 3).
- Convención: no encontrada = `-1` / `false`; patrón vacío = posición 1.
  Verificado contra `std::string::find` en 2000 casos aleatorios.

### Palíndromos (`src/palindrome.h`)
- **Elegido: Manacher** (`longestPalindromeManacher`). Reutiliza el espejo
  dentro del palíndromo más a la derecha para no recomparar: O(n) tiempo y
  memoria. Maneja longitudes impares y pares con dos arreglos de radios.
- **Alternativa: expansión desde el centro** (`longestPalindromeExpand`).
  O(n²) en el peor caso, O(1) memoria. En la prueba grande (10⁶ caracteres con
  un palíndromo de 500 001) sería demasiado lenta en cadenas repetitivas.
- Empate: menor posición inicial. Cadena vacía: `0 0`.

### Subcadena común (`src/common_substring.h`)
- **Elegido: autómata de sufijos** (`longestCommonSubstring`) construido sobre
  la segunda transmisión; se recorre la primera manteniendo la longitud de la
  coincidencia actual. O((n + m)·σ) tiempo y O(m·σ) memoria, con σ = caracteres
  distintos de la segunda (16 en hexadecimal). Al conservar solo mejoras
  estrictas se obtiene el inicio menor en la primera transmisión.
- **Alternativa: programación dinámica** (`longestCommonSubstringDP`) con dos
  filas: O(n·m) tiempo, O(m) memoria. Con transmisiones de ~200 000 caracteres
  serían 4·10¹⁰ operaciones; se usa como oráculo en pruebas pequeñas. Un arreglo
  de sufijos con LCP (O(n log n)) es otra opción válida.
- No confundir con subsecuencia común (permite saltos). Empates: menor inicio en
  la primera y luego en la segunda (este segundo criterio no cambia el rango
  reportado). Sin coincidencias: `0 0`.

## SENTINEL Digital Twin (`app/`)

```bash
make app                          # o: python3 app/server.py --port 8000
```

`app/server.py` usa solo la biblioteca estándar de Python: sirve la interfaz y,
para **cada** cálculo, ejecuta `./sentinel` (lo compila si falta o está
desactualizado). El servidor solo prepara entradas: lectura de CSV, proyección
equirectangular en km para Voronoi y caminos mínimos (Floyd–Warshall) para que
el TSP recorra la red. La interfaz funciona sin internet (Leaflet y el mapa base
están incluidos en `app/static/vendor/`).

- **Network Explorer:** capas de MST, ruta de mantenimiento, flujo máximo con
  corte mínimo y regiones de Voronoi; clic en el mapa = consulta de la central
  más cercana en C++.
- **Algorithm Laboratory:** técnica A vs B sobre el escenario o una instancia
  aleatoria, verificación de consistencia y curva de escalabilidad.
- **Cybersecurity Console:** firmas, palíndromo y subcadena común resaltados;
  salida de 9 líneas; correspondencia de simulación enlazada al mapa.
- **Scenario Manager:** escenarios predefinidos (incluye el *Blackout* GR–IT),
  desactivar enlaces, cargar un escenario JSON (plantilla descargable) y
  comparar indicadores entre escenarios; registro de ejecuciones del núcleo.

Enlaces directos para la demo: `/#overlay=flow`,
`/#scenario=blackout-gr-it&overlay=flow`, `/#tab=cyber`,
`/#tab=lab&labmode=common&lab=scale`.

## Benchmarks (`results/`)

`make bench` ejecuta `bench/benchmark.py`: ambas técnicas de cada problema con
instancias pequeñas, medianas y grandes (semilla fija), mediana de 3 corridas.
Registra el tiempo del algoritmo (C++), el tiempo total del proceso, la memoria
residente máxima, la coincidencia entre técnicas, la pendiente log-log contra
la complejidad teórica y casos límite. Equipo, compilador, opciones y
procedimiento quedan en `results/environment.md`. El análisis está en
`reflection.md`.

## Validación pendiente

- Sustituir los CSV por el paquete oficial del reto (costos, capacidades,
  escenarios y transmisiones) cuando esté disponible.
- Completar `team.txt`, las matrículas de `src/main.cpp` y la sección de
  contribuciones de `reflection.md`; exportar `reflection.md` a PDF.

## Trazabilidad de datos

`app/data/generate_data.py` (reproducible) separa tres categorías:

- **Datos reales, sin modificar:** `app/data/sndlib/geant.txt`, instancia
  `geant` de SNDlib (https://sndlib.put.poznan.pl, formato nativo v1.0): 22
  nodos con coordenadas, 36 enlaces con módulo de 40 000 Mbit/s y 462 demandas
  en Mbit/s (Uhlig/TOTEM, 2005).
- **Transformaciones:** ids cortos (`at1.at` → `AT`), nombres de país y ciudad;
  costo = distancia geodésica haversine en km entre las coordenadas SNDlib (el
  reto pide distancias geodésicas; el costo de módulo SNDlib se conserva en
  `edges.csv` como `sndlib_module_cost` y coincide casi exactamente); capacidad
  base = 40 Gbps; demandas convertidas a Gbps (`demands.csv` conserva Mbit/s).
- **Simulación (no SNDlib):** capacidades de 10/100 Gbps, enlaces caídos y
  configuraciones de `scenarios.json`; transmisiones, firmas `mcode` y
  `mapping.csv` (mismo formato que el botón “Descargar mapping.csv” del sitio:
  `transmission,link,scenario,status`); centroides de países en
  `countries.csv`. Detectar una firma no confirma una infección.
- Mapa base: Natural Earth (dominio público) vía world-atlas@2.0.2 (ISC),
  generado con `app/data/build_basemap.py`. Leaflet 1.9.4 (BSD-2, `app/static/vendor/leaflet/LICENSE`).
