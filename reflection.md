# Reflexión técnica — SENTINELA DEL NORTE

TC2038 Análisis y Diseño de Algoritmos Avanzados · Hackathon integrador
Equipo: ver `team.txt` · Fuente del documento `ReflexSENTINEL_A01255674.pdf`

> Todas las cifras de este documento provienen de `results/benchmarks.md` (generado con
> `make bench`), de `tests/grader_check.py` y del Digital Twin (`make app`). Equipo de
> medición: Apple M4, 24 GiB, macOS (Darwin 27), Apple clang 21, `-std=c++17 -O2`.

---

## 1. Problema y modelo del sistema

La OTAN necesita una sola plataforma para cuatro áreas que hoy trabajan aisladas:
infraestructura (conectar centrales al menor costo), mantenimiento (rutas de visita),
capacidad (información máxima entre dos puntos) y comercial (asignar clientes a su central
más cercana), además de ciberseguridad (firmas, secuencias simétricas y fragmentos
compartidos en transmisiones).

Modelamos el sistema en tres capas:

| Capa | Componente | Responsabilidad |
|---|---|---|
| Núcleo algorítmico | `src/main.cpp` + 8 encabezados C++17 | Los siete algoritmos; protocolo JSON por stdin/stdout y modo archivos. Se ejecuta sin interfaz. |
| Integración | `app/server.py` (biblioteca estándar de Python) | Prepara entradas (CSV → JSON, proyección plana, caminos mínimos para el TSP sobre la red) y ejecuta el binario en cada cálculo. |
| Visualización | `app/static/` (HTML/JS + Leaflet) | Network Explorer, Algorithm Laboratory, Cybersecurity Console y Scenario Manager. |

**Decisión clave:** el núcleo es *header-only* con un único `main.cpp`. El autograder compila
solo `src/main.cpp` y sus encabezados, de modo que cualquier `.cpp` adicional rompería el
enlazado; además exige exactamente un `.cpp` en la entrega.

El protocolo es `{"mode": ..., "input": ...}` → un objeto JSON. Campos opcionales que el
autograder no envía (`algorithm`, `queries`) permiten que la aplicación ejecute la técnica
alternativa y consultas de ubicación sin alterar la salida evaluada.

---

## 2. Análisis por problema

Para cada problema se implementaron **dos técnicas** en C++ y se verificó que coinciden.

### 2.1 Árbol de expansión mínima (MST)

- **Formulación.** Grafo no dirigido ponderado G = (V, E), w: E → ℤ. Buscar T ⊆ E acíclico
  y conexo que minimice Σ w(e). Si G no es conexo no existe árbol (se reporta error).
- **Elegida: Kruskal + Disjoint Set Union** (compresión de caminos, unión por rango).
  Correctitud por la *propiedad de corte*: la arista más ligera que cruza un corte pertenece
  a algún MST; Kruskal agrega aristas en orden creciente y la DSU descarta las que cierran ciclos.
  Tiempo O(E log E) por el ordenamiento, memoria O(V + E).
- **Alternativa: Prim con heap binario.** O(E log V), memoria O(V + E).
- **Resultados.** Con E ≈ 7n, ambas crecen casi linealmente (pendiente log-log 1.09 y 1.10,
  consistente con E log E). Kruskal es ~2.3× más rápido (12.6 ms vs 29.7 ms con n = 32 000,
  ≈ 224 000 aristas): el ordenamiento es secuencial y no necesita listas de adyacencia.
  Prim convendría en grafos densos con matriz de adyacencia (O(V²)).
- **Condiciones.** Pesos negativos permitidos; aristas repetidas o con extremos inválidos se
  ignoran; desempate determinista por (peso, u, v).

### 2.2 Problema del viajante (TSP)

- **Formulación.** Matriz de costos n × n; ciclo hamiltoniano desde el nodo 0 de costo mínimo.
  Sobre la red real un tramo puede pasar por centrales intermedias: el servidor calcula la
  métrica de caminos mínimos (Floyd–Warshall) y el núcleo resuelve el TSP sobre ella.
- **Exacto: Held–Karp.** dp[S][v] = costo mínimo de salir de 0, visitar S y terminar en v.
  Correcto por subestructura óptima. Tiempo O(n²·2ⁿ), memoria O(n·2ⁿ).
- **Aproximado: vecino más cercano + 2-opt.** O(n²) para construir y O(n²) por pasada de 2-opt.
  No garantiza el óptimo: el vecino más cercano puede quedar Θ(log n) veces sobre el óptimo
  en instancias métricas; 2-opt elimina cruces y reduce mucho el error en la práctica.
- **Exacto vs aproximado.** `solveTSP` usa Held–Karp hasta 15 nodos y la heurística por encima.
  El límite viene de la memoria: con n = 16 Held–Karp ya usa 16.9 MB de RSS y cada 2 nodos más
  multiplica el tiempo por ≈ 5 (0.54 → 2.7 → 13.2 ms para n = 12, 14, 16), lo que coincide con
  la razón teórica 2²·((n+2)/n)². Con n = 20 superaría 250 MB, demasiado para el compilador web.
  La heurística escala como n^2.01 (2.8 ms con 800 nodos).
- **Validación.** En 100 instancias euclidianas aleatorias la heurística nunca es mejor que
  el exacto (sería imposible); en 450 puntos sobre un círculo encuentra el perímetro óptimo.
  En la red SNDlib completa (22 centrales, incluida Nueva York) la ruta aproximada mide
  29 761 km; en la instancia de 10 centrales de Europa central el óptimo exacto es 3 752 km.

### 2.3 Flujo máximo

- **Formulación.** Red dirigida con capacidades c(u, v) ≥ 0, fuente s y sumidero t; maximizar
  el flujo respetando capacidad (0 ≤ f ≤ c) y conservación en nodos internos.
  **Capacidad ≠ demanda:** el algoritmo usa solo capacidades; las demandas (`demands.csv`) se
  comparan contra el resultado. Los enlaces físicos son full-duplex: un arco por sentido.
- **Elegida: Dinic.** BFS construye el grafo de niveles y un DFS con punteros de arista envía
  flujo bloqueante. O(V²·E).
- **Alternativa: Edmonds–Karp.** Camino aumentante más corto por BFS, O(V·E²).
- **Correctitud.** Teorema de flujo máximo / corte mínimo. El núcleo devuelve el flujo por arco
  y el lado de cada nodo en el corte; las pruebas verifican capacidad, conservación y que la
  capacidad del corte sea igual al flujo, y que Dinic y Edmonds–Karp coincidan en 200 redes.
- **Resultados (y una sorpresa).** En redes aleatorias con E = 20n ambos escalan casi
  linealmente (pendientes 0.98 y 1.06), muy por debajo de sus cotas de peor caso, porque los
  caminos aumentantes son cortos. Edmonds–Karp incluso gana en tamaños intermedios
  (0.73 vs 1.04 ms con n = 800); Dinic solo se separa en la instancia mayor (2.0 vs 5.0 ms con
  n = 1 600). Elegimos Dinic por su mejor cota garantizada, no por estos casos promedio.

### 2.4 Diagramas de Voronoi

- **Formulación.** Sitios p₁..pₙ en el plano y un dominio rectangular D. La región de pᵢ es
  {x ∈ D : |x − pᵢ| ≤ |x − pⱼ| ∀j}. Como las centrales están en latitud/longitud, se proyectan
  a un plano equirectangular en km centrado en la latitud media del dominio.
- **Elegida: intersección de semiplanos por sitio.** Cada celda empieza como D y se recorta
  (Sutherland–Hodgman) con la mediatriz respecto a cada otro sitio. Poda: los otros sitios se
  visitan por distancia creciente y se detiene cuando la distancia supera dos veces el radio
  actual de la celda, pues esa mediatriz ya no la corta. Tolerancia 1e-9.
- **Alternativa: la misma construcción sin poda** (oráculo en pruebas). Fortune (barrido,
  O(n log n)) o el dual de Delaunay son asintóticamente óptimos, pero requieren recorte
  posterior y son más frágiles numéricamente.
- **Corrección de nuestra estimación inicial.** Esperábamos un comportamiento cercano a
  O(n log n) por la poda. La medición muestra pendiente **2.03**: la poda reduce el recorte
  ≈ 2.6× (127 vs 330 ms con 2 000 sitios), pero ordenar las distancias de *todos* los sitios
  para cada celda cuesta O(n log n) por sitio y domina. Con n ≤ 500 (13 ms) es suficiente;
  la mejora natural es un índice espacial (rejilla o k-d tree) para obtener vecinos en orden.
- **Propiedades verificadas sin conocer la respuesta:** las áreas suman el área del dominio,
  cada celda contiene su sitio y cada vértice no está más cerca de otro sitio que del propio.
  La consulta de ubicación (`nearestSite`, O(n)) asigna los 43 países a 18 de las 22 centrales;
  Nueva York queda fuera del dominio europeo y su región recortada es vacía (caso límite real).

### 2.5 Búsqueda de firmas

- **Formulación.** Para cada transmisión T (n) y firma P (m), primera posición (base 1) de P
  en T, o “no encontrada”.
- **Elegida: Knuth–Morris–Pratt.** La función de prefijo evita retroceder en T. O(n + m)
  tiempo, O(m) memoria.
- **Alternativa: algoritmo Z** sobre P + T: también O(n + m) pero con memoria O(n + m).
- **Resultados.** Ambos lineales (pendientes 1.00 y 0.99). Z resultó ~13 % más rápido
  (32.6 vs 37.4 ms con 4·10⁶ caracteres) pero usó 58 % más memoria (52 vs 33 MB). Elegimos KMP
  por memoria, que importa en el compilador web; la diferencia de tiempo es marginal.
  Coinciden con `std::string::find` en 2 000 casos aleatorios.

### 2.6 Palíndromo más largo

- **Formulación.** Subcadena contigua palíndroma más larga (par o impar); empate → menor inicio.
- **Elegida: Manacher.** Reutiliza el espejo dentro del palíndromo más a la derecha: O(n)
  tiempo y memoria (dos arreglos de radios, sin cadena intercalada).
- **Alternativa: expansión desde el centro.** O(n²) en el peor caso, O(1) memoria.
- **Resultados (dependen de la entrada).** En hexadecimal aleatorio la expansión es *más rápida*
  (4.9 vs 6.1 ms con 10⁶ caracteres): casi todos los radios son pequeños. En el peor caso
  (`AAAA…`) se vuelve cuadrática (pendiente 1.98): 762 ms contra 0.28 ms de Manacher con 64 000
  caracteres. Elegimos Manacher por su garantía lineal; la prueba grande del autograder incluye
  un palíndromo de 500 001 caracteres.

### 2.7 Subcadena común más larga

- **Formulación.** Subcadena contigua común más larga (no subsecuencia); posiciones en la
  primera transmisión; empate → menor inicio en la primera.
- **Elegida: autómata de sufijos** sobre la segunda transmisión (construcción en línea de
  Blumer et al.), recorriendo la primera con la longitud de la coincidencia actual.
  O((n + m)·σ) tiempo y O(m·σ) memoria, con σ = símbolos distintos (16 en hexadecimal; el
  alfabeto se compacta, así que también acepta caracteres no hexadecimales).
- **Alternativa: programación dinámica** con dos filas: O(n·m) tiempo, O(m) memoria.
- **Resultados.** Autómata pendiente 1.11 (15.9 ms con 256 000 caracteres); DP pendiente
  2.04 (89 ms ya con 8 000). Con las transmisiones de ~200 000 caracteres de la prueba grande
  la DP necesitaría ~4·10¹⁰ operaciones. Un arreglo de sufijos con LCP (O(n log n)) sería otra
  alternativa válida.

### 2.8 Resumen experimental

| Problema | Técnica elegida | Pendiente medida | Teoría | Alternativa | Pendiente |
|---|---|---:|---|---|---:|
| MST (E ≈ 7n) | Kruskal | 1.09 | E log E | Prim | 1.10 |
| TSP | Held–Karp | ×5 cada 2 nodos | n²·2ⁿ | Vecino + 2-opt | 2.01 |
| Flujo (E = 20n) | Dinic | 0.98 | V²E (peor caso) | Edmonds–Karp | 1.06 |
| Voronoi | Semiplanos + poda | 2.03 | n² log n (por ordenamiento) | Sin poda | 2.01 |
| Firmas | KMP | 1.00 | n + m | Z | 0.99 |
| Palíndromo (peor caso) | Manacher | 0.94 | n | Expansión | 1.98 |
| Subcadena común | Autómata de sufijos | 1.11 | (n + m)σ | DP | 2.04 |

Las 73 mediciones con ambas técnicas coinciden (0 desacuerdos). Los casos límite (grafo no
conexo, TSP de 1 nodo o sin ciclo, origen = destino, sitios repetidos o colineales, firma vacía,
cadena vacía, sin caracteres comunes) están documentados en `results/benchmarks.md`.

---

## 3. Validación contra el autograder

`tests/grader_check.py` replica los generadores, las referencias y los validadores del
autograder del sitio (perfiles lite, normal y huge) y su linter de estilo. Resultado:
**400/400 casos funcionales**, **21/21 pruebas de estrés** (la más lenta, 0.16 s en nativo frente
a un límite de 18 s en wasm) y **5/5 en estilo**. Las siete suites unitarias (`make test`) pasan
sin advertencias con `-Wall -Wextra -Wpedantic` en clang y g++.

Tres hallazgos del análisis del autograder cambiaron el diseño:

1. Solo compila `main.cpp` + encabezados → todos los módulos pasaron a *header-only*.
2. Las pruebas grandes de TSP llegan a 450 nodos → se agregó la heurística, Held–Karp no escala.
3. Un caso fijo de subcadena común contiene letras no hexadecimales (`ZZABCYYCBA`) → el
   autómata usa un alfabeto compacto en lugar de 16 símbolos fijos.

---

## 4. Integración y visualización (SENTINEL Digital Twin)

`make app` levanta el servidor en `http://127.0.0.1:8000`. **Cada resultado se calcula en el
núcleo C++ en ese momento**; el registro del Scenario Manager muestra cada ejecución con su
tiempo de algoritmo y total.

- **Network Explorer:** mapa offline (contornos de Natural Earth) con centrales y enlaces por
  capacidad; capas de MST, ruta de mantenimiento (badge exacto/aproximado), flujo máximo con
  grosor proporcional y corte mínimo resaltado, regiones de Voronoi y asignación de países; un
  clic en el mapa consulta la central más cercana en C++.
- **Algorithm Laboratory:** técnica A vs B sobre el escenario o una instancia aleatoria de
  tamaño n, verificación de consistencia y curva de escalabilidad.
- **Cybersecurity Console:** transmisiones con firmas, palíndromo y subcadena común resaltados,
  salida de 9 líneas de la especificación y correspondencia de simulación con enlace al mapa.
- **Scenario Manager:** escenarios predefinidos, desactivación de enlaces, carga de escenarios
  JSON y comparación de indicadores entre escenarios.

### Blackout Scenario

En la topología SNDlib, Grecia solo tiene dos enlaces (DE–GR y GR–IT, 40 Gbps cada uno). El
escenario `blackout-gr-it` elimina GR–IT, que pertenece al MST y al corte mínimo Londres → Atenas:

| Indicador | SNDlib base | Blackout GR–IT |
|---|---:|---:|
| MST | 16 240 km | 16 581 km (+2.1 %) |
| Ruta de mantenimiento (22 centrales, aproximada) | 29 761 km | 30 059 km (+1.0 %) |
| Flujo máximo Londres → Atenas | 80 Gbps | 40 Gbps |
| Corte mínimo | DE–GR, GR–IT | DE–GR |

Grecia queda con un solo enlace: un segundo fallo la aislaría. El corte mínimo indica dónde
invertir: en el escenario `mixed-capacity` los dos enlaces hacia Grecia pasan a 100 Gbps y el
flujo Londres → Atenas sube de 80 a 200 Gbps.

**Capacidad ≠ demanda.** La matriz de demandas de SNDlib (pico escalado, 3 Tbps en total) supera
las capacidades de módulo en algunos pares: la mayor demanda es Suiza → Francia con 241 Gbps,
pero el flujo máximo CH → FR es 120 Gbps (corte AT–CH, CH–FR, CH–IT: los tres enlaces de Suiza).
Ningún enrutamiento puede cubrirla sin agregar capacidad; la demanda UK → GR (0.16 Gbps) sí se
cubre en todos los escenarios.

---

## 5. Datos y trazabilidad

`app/data/generate_data.py` (reproducible) separa tres categorías:

- **Datos reales sin modificar:** `app/data/sndlib/geant.txt`, la instancia `geant` de SNDlib
  (único dataset externo): 22 nodos con coordenadas, 36 enlaces con módulo de 40 000 Mbit/s y
  462 demandas en Mbit/s (Uhlig/TOTEM, 2005).
- **Transformaciones:** ids cortos (`at1.at` → `AT`), nombres de país y ciudad; costo = distancia
  geodésica haversine entre las coordenadas SNDlib, como pide el reto (el costo de módulo de
  SNDlib se conserva en `edges.csv` y coincide casi exactamente: 804 vs 803.8 km en AT–CH);
  capacidad base 40 Gbps; demandas convertidas a Gbps.
- **Simulación:** capacidades de 10/100 Gbps, enlaces caídos y configuraciones de
  `scenarios.json`; transmisiones, firmas y `mapping.csv` (mismo formato
  `transmission,link,scenario,status` que publica el sitio). No contamos con el paquete oficial
  del reto (costos, capacidades, escenarios y transmisiones), por lo que estos archivos son
  propios; la interfaz lo indica. Detectar una firma no confirma una infección real.
- Mapa base: Natural Earth (dominio público) vía world-atlas (ISC); Leaflet (BSD-2), incluido
  localmente para funcionar sin internet.

---

## 6. Estándares aplicados

- **Estándar de codificación del curso**, verificado con un puerto del linter del autograder:
  encabezado con descripción, autores, matrículas y fecha; clases en PascalCase, funciones en
  camelCase con comentario previo, constantes en UPPER_SNAKE_CASE, llaves en la misma línea,
  una instrucción por línea, identificadores ASCII. Revisión manual: un tabulador por nivel y
  ningún `break`/`continue` en todo `src/`; cada ciclo expresa todas sus condiciones de término
  en su encabezado (p. ej. Kruskal se detiene con `aristas < n - 1`), lo que facilita razonar
  sobre invariantes de ciclo.
- **ISO/IEC 25010 (calidad del producto):** idoneidad funcional (pruebas contra oráculos y
  validadores del autograder), eficiencia de desempeño (benchmarks con tiempo y memoria),
  fiabilidad (validación de entradas y errores JSON en lugar de fallos), mantenibilidad (un
  módulo por problema, interfaz común), portabilidad (C++17 estándar, servidor sin dependencias).
- **Prácticas de prueba (ISO/IEC/IEEE 29119 como referencia):** pruebas unitarias por módulo,
  pruebas diferenciales entre técnicas, pruebas basadas en propiedades y casos límite.
- **Reproducibilidad:** semillas fijas, `results/environment.md` con equipo, compilador,
  opciones y procedimiento; `make test`, `make bench`, `make app`.
- **Ética profesional (Código de Ética ACM):** los datos de seguridad se presentan como
  simulación; no se atribuyen infecciones a partir de coincidencias de cadenas; se citan
  fuentes y licencias.

---

## 7. Limitaciones y trabajo futuro

- Voronoi es O(n² log n) por el ordenamiento por sitio; un índice espacial lo llevaría cerca de
  O(n log n). La proyección equirectangular distorsiona distancias lejos de la latitud central.
- La heurística de TSP no garantiza el óptimo y 2-opt solo se aplica a matrices simétricas;
  Or-opt o Lin–Kernighan mejorarían la calidad.
- La memoria medida incluye el árbol JSON de la entrada (p. ej. 166 MB para 224 000 aristas):
  un lector JSON en streaming la reduciría.
- Capacidades de escenario y transmisiones son simuladas; con el paquete oficial del reto solo se
  sustituyen los CSV y los `.txt` de `app/data/`.

---

## 8. Contribuciones del equipo

| Integrante | Matrícula | Contribución principal |
|---|---|---|
| Verónica Paola Zapata Sánchez | A01199193 | Network Optimization (MST, TSP) |
| Luis Álvaro Rosales Sálazar | A01255674 | Flow & Computational Geometry (flujo, Voronoi) |
| Fernando Hernandez Cantú | A00841856 | Cybersecurity Algorithms (firmas, palíndromos, subcadena común) |
| Alfonso José Morales Mallozzi | A00841550 | System Integration & Validation (app, pruebas, benchmarks) |

### Reflexiones individuales

**Luis Álvaro Rosales Sálazar (A01255674) — Flow & Computational Geometry.** Aprendi algo que parecia un poco obvio pero no me parecia tan claro: el flujo máximo siempre es igual al corte mínimo, y eso sirve para
saber si tu código está bien. Si al final la suma de las aristas saturadas del corte no da lo mismo
que el flujo, hay un error. Eso es lo que revisan las pruebas de tests/test_max_flow.cpp, además
de comparar contra Edmonds–Karp en 200 redes aleatorias. También me di cuenta de que el número
solo no dice mucho; lo útil es ver qué aristas forman el corte. Por ejemplo, de Londres a Atenas
solo salen 80 Gbps porque a Grecia nada más llegan dos enlaces (DE–GR y GR–IT), y en el escenario
*Blackout* GR–IT baja a 40 Gbps.

La decisión que defiendo es haber elegido Dinic aunque nuestros propios benchmarks no lo favorecen en todo: 
Edmonds–Karp ganó con n = 800 (0.73 vs 1.04 ms) y Dinic solo se separó con n = 1 600 (2.0 vs 5.0 ms); lo elegimos
por su compejidad garantizada O(V²·E), no por su complejidad promedio. 

Lo que cambiaría es Voronoi: esperaba un comportamiento cercano a O(n log n) gracias a la poda, pero la medición dio
n^2.03, porque ordenar las distancias a todos los sitios para cada celda domina aunque la poda
reduzca el recorte ≈ 2.6× (127 vs 330 ms con 2 000 sitios). Con más tiempo usaría un índice
 (rejilla o k-d tree) para obtener los vecinos en orden sin ordenar a todos. Todas las
cifras igual están en results/benchmarks.md.

**Verónica Paola Zapata Sánchez (A01199193) — MST y TSP.**
Aprendí que elegir un algoritmo no depende solo de su complejidad
teórica, sino de la forma de los datos y de las restricciones del
entorno. En el MST, Kruskal con DSU resultó ~2.3× más rápido que Prim
(12.6 ms vs 29.7 ms con 32 000 nodos) porque nuestras entradas son
listas de aristas dispersas (E ≈ 7V); Prim habría ganado en grafos
densos. En el TSP aprendí que el límite del enfoque exacto lo marca la
memoria antes que el tiempo: Held-Karp usa O(n·2ⁿ) y con 20 nodos
superaba los 250 MB del compilador web.

Defendería dos decisiones. Primera, Kruskal por la propiedad de corte y
por su rendimiento medido, con Prim como alternativa y oráculo en
pruebas: 200 grafos aleatorios donde ambos dan el mismo costo. Segunda,
el criterio híbrido del TSP: Held-Karp hasta 15 nodos para garantizar el
óptimo y vecino más cercano + 2-opt por encima, informando en la salida
si el resultado es exacto o aproximado, para no presentar una
aproximación como óptimo.

Cambiaría la heurística del TSP: 2-opt no garantiza el óptimo y solo
aplica a matrices simétricas, así que probaría Or-opt o Lin-Kernighan, o
Christofides (garantía de 1.5×) al ser una métrica de caminos mínimos
que cumple la desigualdad triangular. También leería el JSON en
streaming, porque el árbol de entrada domina la memoria (166 MB con
224 000 aristas) y no el algoritmo en sí.

---

## Referencias

- Cormen, Leiserson, Rivest, Stein. *Introduction to Algorithms*, 4.ª ed. MIT Press, 2022.
- Kruskal, J. B. (1956). On the shortest spanning subtree of a graph. *Proc. AMS* 7(1).
- Prim, R. C. (1957). Shortest connection networks. *Bell System Technical Journal* 36(6).
- Held, M., Karp, R. M. (1962). A dynamic programming approach to sequencing problems. *SIAM J.* 10(1).
- Rosenkrantz, Stearns, Lewis (1977). An analysis of several heuristics for the TSP. *SIAM J. Comput.* 6(3).
- Dinic, E. A. (1970). Algorithm for solution of a problem of maximum flow. *Soviet Math. Doklady* 11.
- Edmonds, J., Karp, R. M. (1972). Theoretical improvements in algorithmic efficiency for network flow problems. *J. ACM* 19(2).
- Sutherland, I., Hodgman, G. (1974). Reentrant polygon clipping. *Comm. ACM* 17(1).
- Fortune, S. (1987). A sweepline algorithm for Voronoi diagrams. *Algorithmica* 2.
- Knuth, Morris, Pratt (1977). Fast pattern matching in strings. *SIAM J. Comput.* 6(2).
- Gusfield, D. (1997). *Algorithms on Strings, Trees and Sequences* (algoritmo Z). Cambridge UP.
- Manacher, G. (1975). A new linear-time on-line algorithm for finding the smallest initial palindrome of a string. *J. ACM* 22(3).
- Blumer et al. (1985). The smallest automaton recognizing the subwords of a text. *Theor. Comput. Sci.* 40.
- Orlowski, Wessäly, Pióro, Tomaszewski (2010). SNDlib 1.0 — Survivable Network Design Library. *Networks* 55(3).
- ISO/IEC 25010:2011; ISO/IEC/IEEE 29119; ACM Code of Ethics and Professional Conduct (2018).
