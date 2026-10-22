# Guía para la defensa técnica — SENTINELA DEL NORTE

En la defensa **cualquier integrante puede recibir preguntas de cualquiera de los siete
algoritmos**. Esta guía resume, por problema: la idea en palabras de pizarrón, por qué es
correcto, su complejidad, la alternativa y por qué no la elegimos, dónde está en el código,
casos límite y las preguntas más probables con una respuesta corta. Al final hay un guion de
demo y ejercicios para practicar a mano.

> Regla de oro: **primero la idea, luego la complejidad, luego el código.** Si no recuerdas un
> detalle, explica qué invariante mantiene el algoritmo; eso suele bastar.

---

## 0. El sistema en 60 segundos

- **Núcleo C++17:** un solo `src/main.cpp` + encabezados (*header-only*). El autograder compila
  solo `main.cpp`, por eso no hay otros `.cpp` en `src/`.
- **Protocolo:** lee `{"mode": ..., "input": ...}` por stdin y escribe un objeto JSON por stdout.
  Siete modos: `mst`, `tsp`, `flow`, `voronoi`, `matching`, `palindrome`, `common`. Con 5 archivos
  como argumentos imprime el formato de texto de 9 líneas de la especificación.
- **App (Digital Twin):** `python3 app/server.py` sirve la interfaz y **ejecuta el binario C++ en
  cada cálculo** (el registro del Scenario Manager lo demuestra). El servidor solo prepara entradas.
- **Datos:** topología, coordenadas, capacidad base (40 Gbps) y demandas reales de SNDlib `geant`;
  costos = distancia geodésica; escenarios, capacidades 10/100 y transmisiones = simulación.

**Preguntas probables**
- *¿Por qué todo es header-only?* — El autograder compila únicamente `main.cpp`; un `.cpp`
  adicional no se enlaza y además penaliza la estructura (exige exactamente uno).
- *¿La interfaz calcula algo?* — No. Solo dibuja; cada resultado viene de `./sentinel` vía
  `POST /api/run`. Los campos opcionales `algorithm` y `queries` permiten elegir la técnica
  alternativa y consultar ubicaciones sin cambiar la salida que evalúa el autograder.
- *¿Por qué JSON propio?* — El reto exige C++ sin dependencias externas; `src/json.h` es un
  analizador descendente recursivo pequeño.

---

## 1. Árbol de expansión mínima — `src/mst.h`

**Problema.** Conectar todas las centrales con el menor costo total, sin ciclos.

**Idea (Kruskal + DSU).** Ordena las aristas de menor a mayor peso; agrega cada una si une dos
componentes distintas (lo decide la *Disjoint Set Union*); se detiene al tener n − 1 aristas.
- DSU: `find` con compresión de caminos, `unite` por rango → casi O(1) amortizado (α(n)).
- Código: `kruskalMST`, clase `DSU`; alternativa `primMST`.

**Correctitud.** *Propiedad de corte:* para cualquier corte del grafo, la arista más ligera que
lo cruza pertenece a algún MST. Cuando Kruskal acepta una arista, es la más ligera que conecta
su componente con el resto → es segura. Rechazar una arista que cierra ciclo es correcto porque
la más pesada de un ciclo nunca es necesaria.

**Complejidad.** O(E log E) por el ordenamiento; memoria O(V + E).
**Alternativa: Prim con heap**, O(E log V): crece un solo árbol desde un nodo. Kruskal fue
~2.3× más rápido en nuestros benchmarks (E ≈ 7n) porque ordenar es secuencial y no necesita
listas de adyacencia. Prim convendría en grafos densos (con matriz, O(V²)).

**Casos límite.** Grafo no conexo → no hay árbol (el modo `mst` responde error; Kruskal regresa
un bosque); un nodo → costo 0; pesos negativos funcionan igual; aristas repetidas o lazos se
ignoran.

**Preguntas probables**
- *¿Por qué la compresión de caminos?* — Aplana los árboles de la DSU: `find` queda casi
  constante; sin ella podría ser O(n) por operación.
- *¿El MST es único?* — Solo si los pesos son distintos; desempatamos por (peso, u, v) para que
  la salida sea determinista.
- *¿Qué cambia en el Blackout?* — Si el enlace caído estaba en el MST, Kruskal toma la siguiente
  arista más barata que reconecta: con GR–IT caído el MST sube de 16 240 a 16 581 km.

---

## 2. Problema del viajante — `src/tsp.h`

**Problema.** Recorrer todas las centrales una vez y volver al inicio con distancia mínima.
Sobre la red, un tramo entre dos centrales puede pasar por intermedias: el servidor calcula la
matriz de caminos mínimos (Floyd–Warshall) y el núcleo resuelve el TSP sobre esa matriz.

**Exacto: Held–Karp** (`solveTSPExact`). `dp[S][v]` = costo mínimo de salir de 0, visitar
exactamente el conjunto S y terminar en v. Transición: `dp[S ∪ {w}][w] = min(dp[S][v] + c(v,w))`.
Respuesta: `min_v dp[todos][v] + c(v,0)`; el camino se reconstruye con `parent`.
- Tiempo O(n²·2ⁿ), memoria O(n·2ⁿ). Admite hasta 20 nodos; lo usamos hasta 15 (`TSP_EXACT_LIMIT`).

**Aproximado: vecino más cercano + 2-opt** (`solveTSPHeuristic`).
- Vecino más cercano: desde 0, ir siempre a la central no visitada más cercana. O(n²).
- 2-opt: si invertir el tramo `tour[i..j]` acorta la ruta (cambiar aristas (a,b),(c,d) por
  (a,c),(b,d)), hacerlo; repetir hasta que no haya mejora. O(n²) por pasada.
- `solveTSP` elige: ≤ 15 nodos → exacto; si no → heurística. El resultado indica `exact`.

**Correctitud.** Held–Karp: subestructura óptima (la mejor ruta que termina en v con el conjunto
S contiene la mejor ruta a su penúltimo nodo con S − {v}). La heurística **no garantiza el
óptimo**; garantiza un ciclo hamiltoniano válido cuyo costo reportado coincide con la ruta.

**Por qué el límite de 15.** Memoria: con n = 16 ya son 16.9 MB y cada 2 nodos el tiempo se
multiplica ≈ 5 (0.54 → 2.7 → 13.2 ms para n = 12, 14, 16), igual a 2²·((n+2)/n)². Con 20 nodos
serían > 250 MB, demasiado para el compilador web del autograder (que prueba hasta 450 nodos).

**Casos límite.** n = 1 → ruta `[0, 0]`, costo 0; sin ciclo hamiltoniano (aristas −1) → error;
matriz no cuadrada → error; exacto con más de 20 nodos → error explícito.

**Preguntas probables**
- *¿Por qué no fuerza bruta?* — (n−1)! rutas: con 12 nodos ya son ~40 millones; Held–Karp
  reutiliza subproblemas y baja a n²·2ⁿ.
- *¿Qué tan mala puede ser la heurística?* — El vecino más cercano puede quedar Θ(log n) veces
  sobre el óptimo en instancias métricas; 2-opt elimina cruces y mejora mucho en la práctica. En
  450 puntos sobre un círculo encuentra el perímetro óptimo; en 100 instancias pequeñas nunca
  fue mejor que el exacto (sería imposible).
- *¿Por qué 2-opt solo con matrices simétricas?* — Invertir un tramo cambia el sentido de sus
  aristas internas; si c(a,b) ≠ c(b,a) el delta O(1) deja de ser válido.

---

## 3. Flujo máximo — `src/max_flow.h`

**Problema.** Máxima información de s a t sin exceder capacidades y con conservación del flujo
en nodos intermedios. **Capacidad ≠ demanda:** el algoritmo usa capacidades; las demandas solo
se comparan contra el resultado. Los enlaces son full-duplex → un arco por sentido.

**Idea (Dinic).** Repetir: (1) BFS desde s sobre el grafo residual para numerar niveles;
(2) DFS que solo avanza de nivel i a i+1 y envía *flujo bloqueante*, con un puntero por nodo
(`nextEdge_`) para no revisar dos veces aristas agotadas. Termina cuando t no es alcanzable.
- Código: `detail::FlowNetwork::dinic`, `buildLevels`, `sendFlow`; alternativa
  `edmondsKarp` → `augmentShortestPath`. Público: `maxFlow(n, edges, s, t)`.
- Grafo residual: cada arista guarda la posición de su reversa; empujar f resta f a la arista y
  suma f a la reversa (eso permite "deshacer" flujo).

**Correctitud.** *Teorema flujo máximo = corte mínimo:* cuando no hay camino aumentante, los
nodos alcanzables desde s en el residual forman un corte cuyas aristas están saturadas → el flujo
es igual a su capacidad → es máximo. Devolvemos ese corte (`minCutSide`) y el flujo por arista
(`edgeFlow`); las pruebas verifican capacidad, conservación y capacidad del corte = flujo.

**Complejidad.** Dinic O(V²·E); Edmonds–Karp O(V·E²). Memoria O(V + E).

**Resultado honesto de los benchmarks.** En redes aleatorias (E = 20n) ambos escalan casi
lineal, muy por debajo del peor caso, y Edmonds–Karp ganó en tamaños intermedios (0.73 vs 1.04 ms
con n = 800); Dinic solo se separó en la mayor (2.0 vs 5.0 ms). Elegimos Dinic por su cota
garantizada, no por el caso promedio.

**Casos límite.** s = t → 0; sin camino → 0; capacidad negativa → error; desbordamiento de
`long long` detectado (`overflow_error`).

**Preguntas probables**
- *¿Por qué hacen falta aristas reversas?* — Para poder redirigir flujo enviado por un mal
  camino; sin ellas el voraz puede quedarse en un flujo no máximo.
- *¿Por qué Edmonds–Karp termina en tiempo polinomial y Ford–Fulkerson no?* — Usar el camino más
  corto (BFS) hace que la distancia de s a cada nodo no disminuya; cada arista se satura O(V)
  veces → O(VE) aumentos.
- *En la app, ¿qué es la línea roja punteada?* — El corte mínimo: los enlaces que limitan el
  flujo. Londres → Atenas = 80 Gbps porque Grecia solo tiene DE–GR y GR–IT (40 + 40).
- *Ejemplo de capacidad ≠ demanda:* SNDlib pide 241 Gbps de Suiza a Francia, pero el flujo
  máximo CH → FR es 120 Gbps (los tres enlaces de Suiza). No hay enrutamiento que lo cubra.

---

## 4. Diagramas de Voronoi — `src/voronoi.h`

**Problema.** Asignar cada ubicación (cliente/país) a su central más cercana: regiones de
Voronoi sobre coordenadas planas, recortadas al dominio, y consulta de ubicación. Las centrales
están en latitud/longitud → proyección equirectangular en km centrada en la latitud media.

**Idea (intersección de semiplanos).** La región del sitio p es la intersección de los
semiplanos "más cerca de p que de q" para todo q. Cada semiplano lo define la **mediatriz** de pq.
Se empieza con el rectángulo del dominio y se recorta con cada mediatriz (Sutherland–Hodgman:
recorrer los vértices, conservar los que quedan del lado de p, agregar el punto donde una arista
cruza la recta).
- Semiplano usado: `2(qx − px)·x + 2(qy − py)·y ≤ |q|² − |p|²` (sale de |x − p|² ≤ |x − q|²).
- Poda: visitar los otros sitios por distancia creciente y parar cuando `dist(p, q) > 2·radio`
  de la celda: esa mediatriz ya no puede cortarla.
- Código: `voronoiCells`, `clipHalfPlane`, `detail::clipByBisector`, `detail::cellRadius`;
  alternativa `voronoiCellsBrute`; consulta `nearestSite`; área `polygonArea` (fórmula del zapato).

**Correctitud.** La celda es exactamente la intersección de todos los semiplanos ∩ dominio; la
poda solo omite semiplanos que contienen toda la celda actual (si la mediatriz está a más de
`radio` de p, todos los vértices de la celda ya cumplen la desigualdad).

**Complejidad (corregimos nuestra estimación).** Esperábamos casi O(n log n); medimos **n^2.03**.
La poda reduce el recorte ≈ 2.6×, pero ordenar las distancias de todos los sitios para cada
celda cuesta O(n log n) por sitio → O(n² log n). Mejora: índice espacial (rejilla o k-d tree).
Alternativas asintóticamente óptimas: Fortune (barrido, O(n log n)) o dual de Delaunay; más
frágiles numéricamente y requieren recorte posterior.

**Propiedades que verificamos sin conocer la respuesta exacta.** Las áreas suman el área del
dominio; cada celda contiene su sitio; ningún vértice está más cerca de otro sitio que del propio;
la versión con poda y sin poda coinciden. Tolerancia `GEOMETRY_EPS = 1e-9`.

**Casos límite.** Un sitio → todo el dominio; sitios repetidos → error; colineales → franjas;
sitio fuera del dominio → celda vacía (Nueva York en el mapa de Europa).

**Preguntas probables**
- *¿Por qué proyectar?* — Voronoi euclidiano requiere coordenadas planas; en grados, un grado de
  longitud no mide lo mismo que uno de latitud. La proyección distorsiona lejos de la latitud
  central (limitación documentada).
- *¿Cómo manejan el borde del dominio?* — La celda empieza siendo el dominio, así que siempre
  queda recortada; con tolerancia 1e-9 para puntos sobre la recta.

---

## 5. Búsqueda de firmas — `src/matching.h`

**Problema.** Primera posición (base 1) de cada firma en cada transmisión, o "no encontrada".
Salida de texto: 6 líneas `true <pos>` / `false` (tres firmas en transmission1, luego en 2).

**Idea (KMP).** La *función de prefijo* `pi[i]` = longitud del borde propio más largo de
`P[0..i]` (prefijo que también es sufijo). Al fallar una comparación no se retrocede en el
texto: se salta en el patrón a `pi[matched − 1]`.
- Código: `prefixFunction`, `findFirstKMP`, `matchAll`; alternativa `zFunction`, `findFirstZ`.

**Correctitud.** Tras un fallo con `matched` caracteres coincidentes, la siguiente alineación que
puede funcionar es el borde más largo de lo ya coincidente; las posiciones intermedias no pueden
ser coincidencia (contradirían la maximalidad del borde).

**Complejidad.** O(n + m) tiempo: el índice del texto solo avanza y `matched` baja a lo sumo
tantas veces como subió. Memoria O(m).
**Alternativa: algoritmo Z** sobre `P + T`: `z[i]` = prefijo común más largo de la cadena y su
sufijo en i; hay coincidencia donde `z[i] ≥ m`. También O(n + m) pero memoria O(n + m). Medimos Z
~13 % más rápido pero con 58 % más memoria (52 vs 33 MB con 4·10⁶ caracteres): elegimos KMP.

**Casos límite.** Firma vacía → posición 1 (igual que `indexOf` de la referencia); firma más
larga que el texto → no encontrada; firmas traslapadas o repetidas → la primera aparición.

**Preguntas probables**
- *¿Por qué no Rabin–Karp?* — O(n + m) esperado, pero con colisiones de hash hay que verificar y
  el peor caso es O(nm).
- *¿Y con muchas firmas?* — Aho–Corasick busca todas a la vez en O(n + Σm + coincidencias); con
  tres firmas KMP es más simple.

---

## 6. Palíndromo más largo — `src/palindrome.h`

**Problema.** Subcadena contigua palíndroma más larga (par o impar), por transmisión; empate →
menor posición inicial. Salida: 2 líneas `inicio fin`.

**Idea (Manacher).** Mantener el palíndromo conocido que llega más a la derecha `[l, r]`. Para un
centro i dentro de él, su espejo `l + r − i` ya tiene un radio calculado: i hereda al menos
`min(radio_espejo, r − i + 1)` y solo se expande más allá de r. Dos arreglos: `odd` (centro en un
carácter) y `even` (centro entre dos caracteres).
- Código: `longestPalindromeManacher`; alternativa `longestPalindromeExpand`.

**Correctitud.** Dentro de un palíndromo, la vecindad de i es el reflejo de la de su espejo, así
que el radio heredado es válido; cada expansión explícita mueve r a la derecha.
**Complejidad.** O(n): r solo crece, como máximo n veces en total. Memoria O(n).

**Alternativa: expansión desde el centro** (2n − 1 centros), O(n²) peor caso, O(1) memoria.
**Resultado que hay que saber explicar:** en hexadecimal aleatorio la expansión fue *más rápida*
(4.9 vs 6.1 ms con 10⁶ caracteres) porque casi todos los radios son pequeños; en el peor caso
`AAAA…` se vuelve cuadrática: 762 ms contra 0.28 ms de Manacher con 64 000 caracteres. Elegimos
Manacher por la garantía (la prueba grande trae un palíndromo de 500 001 caracteres).

**Casos límite.** Cadena vacía → `0 0`; un carácter → `1 1`; empate (`ABACDC`) → `1 3`.

---

## 7. Subcadena común más larga — `src/common_substring.h`

**Problema.** Fragmento contiguo más largo presente en ambas transmisiones; posiciones en la
primera; empate → menor inicio en la primera. **No es subsecuencia** (la subsecuencia permite
saltos). Salida: 1 línea `inicio fin`.

**Idea (autómata de sufijos).** Se construye un autómata que reconoce exactamente las subcadenas
de la segunda transmisión (construcción en línea de Blumer et al., agregando un carácter a la vez
con estados clon). Luego se recorre la primera transmisión: si existe transición con el siguiente
carácter, la coincidencia crece; si no, se siguen los *enlaces de sufijo* hasta poder continuar
(la coincidencia se acorta a la longitud del estado). La mayor longitud vista es la respuesta.
- Código: clase `SuffixAutomaton` (`extend`, `longestMatchIn`), `longestCommonSubstring`;
  alternativa `longestCommonSubstringDP`.
- Alfabeto compacto: σ = caracteres distintos de la segunda (16 en hex). Lo hicimos así porque un
  caso fijo del autograder trae letras no hexadecimales (`ZZABCYYCBA`).

**Complejidad.** O((n + m)·σ) tiempo, O(m·σ) memoria; el autómata tiene ≤ 2m estados.
**Alternativa: DP** `dp[i][j]` = longitud del sufijo común de `A[0..i)` y `B[0..j)`; con dos filas
O(n·m) tiempo y O(m) memoria. Medimos DP n^2.04 (89 ms ya con 8 000) frente a autómata n^1.11
(15.9 ms con 256 000): con las ~200 000 de la prueba grande la DP haría ~4·10¹⁰ operaciones.
Otra opción válida: arreglo de sufijos + LCP, O(n log n).

**Empates.** Al recorrer la primera de izquierda a derecha y guardar solo mejoras *estrictas*, el
primer final con la longitud máxima da el menor inicio.

**Casos límite.** Sin caracteres comunes → `0 0`; cadenas iguales → toda la cadena.

---

## 8. Validación y benchmarks (lo que más preguntan)

- `make test`: 7 suites unitarias (pruebas diferenciales entre técnicas, propiedades, casos
  límite), `tests/grader_check.py` (réplica del autograder: 400/400 funcionales, 21/21 de estrés,
  estilo 5/5) y el modo archivos contra `tests/samples/expected_output.txt`.
- `make bench` → `results/`: ambas técnicas, tamaños pequeños/medianos/grandes, mediana de 3,
  tiempo del algoritmo medido en C++, memoria máxima (RSS), coincidencia entre técnicas y
  pendiente log-log contra la teoría; equipo y compilador en `results/environment.md`.
- **Cómo leer la pendiente log-log:** si t ≈ c·nᵏ, entonces log t = log c + k·log n; la pendiente
  estima k. Lineal ≈ 1, cuadrático ≈ 2. Para algo exponencial (Held–Karp) se mira el factor por
  cada +2 nodos, no la pendiente.
- **Por qué la memoria parece grande en MST (166 MB):** incluye el árbol JSON de la entrada
  (224 000 aristas); el algoritmo en sí usa O(V + E).

---

## 9. Guion de demo (≈ 5 minutos)

Arrancar con `make app` y abrir `http://127.0.0.1:8000`.

1. **Network Explorer** — topología SNDlib GÉANT. Botón *Flujo máximo* (Londres → Atenas):
   80 Gbps, corte DE–GR y GR–IT en rojo. Clic en el mapa → central más cercana (C++).
2. **Blackout** — `/#scenario=blackout-gr-it&overlay=flow`: el flujo baja a 40 Gbps y el corte
   queda en DE–GR; Grecia depende de un solo enlace.
3. **Algorithm Laboratory** — Subcadena común, *Curva de escalabilidad*
   (`/#tab=lab&labmode=common&lab=scale`): la DP crece cuadrático, el autómata casi plano.
4. **Cybersecurity Console** (`/#tab=cyber`) — salida de 9 líneas, firmas/palíndromo/subcadena
   resaltados, *Ver en mapa* del enlace Austria – Alemania.
5. **Scenario Manager** — *Calcular indicadores de todos los escenarios* y mostrar el registro de
   ejecuciones del núcleo C++ (prueba de que la interfaz no inventa resultados).

---

## 10. Ejercicios para practicar a mano (con respuesta)

1. **Kruskal.** Aristas (0-1,10), (0-2,6), (0-3,5), (1-3,15), (2-3,4). Orden: 4, 5, 6, 10, 15.
   Se aceptan 2-3 (4), 0-3 (5); 0-2 (6) cierra ciclo; 0-1 (10). **Costo 19.**
2. **Prefijo de KMP** para `ABABAC`: **[0, 0, 1, 2, 3, 0]**.
3. **Flujo (CLRS).** Red de 6 nodos de la prueba `testClassicNetwork`: **flujo máximo 23**.
   Identifica el corte mínimo y comprueba que su capacidad también es 23.
4. **Voronoi.** Sitios (25, 50) y (75, 50) en [0, 100]²: la mediatriz es x = 50; **cada celda
   mide 5 000**.
5. **Palíndromo.** `ABCBA123321`: candidatos `ABCBA` (5) y `123321` (6) → **6 11**.
6. **Subcadena común.** `AA1234BBB` y `DD1234EE` → `1234` → **3 6**. Para `ABCXXCBA` y
   `ZZABCYYCBA` hay empate de longitud 3 (`ABC` y `CBA`) → menor inicio → **1 3**.
7. **Held–Karp.** Matriz 4×4 de `testCompleteGraph` → **costo 80**. Calcula a mano
   `dp[{0,1}][1] = 10` y `dp[{0,1,2}][2] = min(10 + 35) = 45`.

---

## 11. Preguntas de cierre (cualquiera puede recibirlas)

- *¿Qué cambiarían con más tiempo?* — Índice espacial para Voronoi, Or-opt/Lin–Kernighan para el
  TSP, lector JSON en streaming para reducir memoria, usar el paquete oficial de datos del reto.
- *¿Qué datos son reales?* — Topología, coordenadas, capacidad de módulo y demandas de SNDlib
  (`app/data/sndlib/geant.txt` sin modificar). Costos geodésicos, escenarios, transmisiones y
  `mapping.csv` son transformaciones o simulación, y así lo dice la interfaz.
- *¿Una coincidencia de firma confirma una infección?* — No. `mapping.csv` es una construcción de
  simulación; la detección de una cadena solo indica que aparece en la transmisión.
- *¿Qué estándar de código siguieron?* — El del curso (encabezado con autores/matrículas/fecha,
  camelCase/PascalCase/UPPER_SNAKE_CASE, comentario antes de cada función, un tabulador por nivel,
  sin `break`/`continue`), verificado con el linter del autograder (5/5).
