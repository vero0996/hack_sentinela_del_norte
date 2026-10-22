"""Benchmarks reproducibles del nucleo SENTINEL (ambas tecnicas de cada problema).

Para cada problema genera instancias pequenas, medianas y grandes (semilla fija), ejecuta el
binario con cada tecnica (campo input.algorithm), y registra:
  - elapsed_ms: tiempo del algoritmo medido dentro de C++ (mediana de REPEATS corridas)
  - wall_ms: tiempo total del proceso (lectura/escritura JSON incluida)
  - peak_rss_mb: memoria residente maxima del proceso (os.wait4)
  - agree: si ambas tecnicas coinciden en el resultado
Tambien ejecuta casos limite y ajusta la pendiente log-log (exponente empirico) por serie.

Uso:
    python3 bench/benchmark.py ./sentinel            # escribe results/benchmarks.{csv,md} y results/environment.md
    python3 bench/benchmark.py ./sentinel --quick    # tamanos reducidos
"""
import argparse
import csv
import datetime
import json
import math
import os
import platform
import random
import statistics
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
RESULTS = REPO / "results"
REPEATS = 3
SEED = 2038
HEX = "0123456789ABCDEF"
COMPILE_FLAGS = "-std=c++17 -O2"


# ------------------------------------------------------------------ ejecucion

def run_once(binary, mode, payload):
    """Ejecuta el binario y regresa (respuesta, wall_ms, peak_rss_mb)."""
    data = json.dumps({"mode": mode, "input": payload}).encode()
    start = time.perf_counter()
    proc = subprocess.Popen([binary], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    proc.stdin.write(data)
    proc.stdin.close()
    output = proc.stdout.read()
    proc.stdout.close()
    _, status, usage = os.wait4(proc.pid, 0)
    proc.returncode = os.waitstatus_to_exitcode(status)
    wall_ms = (time.perf_counter() - start) * 1000
    # ru_maxrss: bytes en macOS, KiB en Linux.
    divisor = 1024 * 1024 if sys.platform == "darwin" else 1024
    answer = json.loads(output) if output else {"error": f"sin salida (codigo {proc.returncode})"}
    return answer, wall_ms, usage.ru_maxrss / divisor


def measure(binary, mode, payload, algorithm):
    payload = dict(payload, algorithm=algorithm)
    samples = [run_once(binary, mode, payload) for _ in range(REPEATS)]
    answer = samples[-1][0]
    if "error" in answer:
        return answer, None, None, None
    elapsed = statistics.median(s[0]["elapsed_ms"] for s in samples)
    wall = statistics.median(s[1] for s in samples)
    rss = max(s[2] for s in samples)
    return answer, elapsed, wall, rss


# ------------------------------------------------------------------ generadores

def gen_mst(rng, n):
    edges = [{"u": i, "v": i + 1, "weight": rng.randint(1, 1000)} for i in range(n - 1)]
    edges += [{"u": rng.randrange(n), "v": rng.randrange(n), "weight": rng.randint(1, 1000)} for _ in range(6 * n)]
    return {"nodes": list(range(n)), "edges": [e for e in edges if e["u"] != e["v"]]}


def gen_tsp(rng, n):
    points = [(rng.randint(0, 10000), rng.randint(0, 10000)) for _ in range(n)]
    return {"distances": [[round(math.dist(a, b)) for b in points] for a in points]}


def gen_flow(rng, n):
    edges = [{"u": i, "v": i + 1, "capacity": rng.randint(100, 10000)} for i in range(n - 1)]
    seen = {(i, i + 1) for i in range(n - 1)}
    while len(edges) < 20 * n:
        u, v = rng.randrange(n), rng.randrange(n)
        if u != v and (u, v) not in seen:
            seen.add((u, v))
            edges.append({"u": u, "v": v, "capacity": rng.randint(1, 10000)})
    return {"n": n, "edges": edges, "source": 0, "sink": n - 1}


def gen_voronoi(rng, n):
    points = set()
    while len(points) < n:
        points.add((rng.randint(1, 99999), rng.randint(1, 99999)))
    return {"sites": [list(p) for p in points], "bounds": [0, 0, 100000, 100000]}


def hex_string(rng, n):
    return "".join(rng.choices(HEX, k=n))


def gen_matching(rng, n):
    signatures = ["DEADBEEFCAFEBABE", "0123456789ABCDEF0123456789ABCDEF", "FACEFACEFACEFACEFACE"]
    a, b = hex_string(rng, n), hex_string(rng, n)
    return {"transmissions": [a[:n // 3] + signatures[0] + a[n // 3:], b[:2 * n // 3] + signatures[1] + b[2 * n // 3:]],
            "patterns": signatures}


def gen_palindrome(rng, n):
    half = hex_string(rng, n // 4)
    side = hex_string(rng, n // 4)
    return {"transmissions": [side + half + "A" + half[::-1] + side, hex_string(rng, n)]}


def gen_palindrome_worst(rng, n):
    # Peor caso de la expansion desde centro: cada centro se expande hasta el borde.
    return {"transmissions": ["A" * n]}


def gen_common(rng, n):
    shared = hex_string(rng, max(8, n // 8))
    half = n // 2
    return {"transmissions": [hex_string(rng, half) + shared + hex_string(rng, half),
                              hex_string(rng, half) + shared + hex_string(rng, half)]}


# family -> (mode, generator, [(algorithm, sizes)], result key, theoretical exponent note)
SUITES = {
    "mst": ("mst", gen_mst, [("kruskal", [500, 2000, 8000, 32000]), ("prim", [500, 2000, 8000, 32000])],
            "cost", "O(E log E) / O(E log V), E = 7n"),
    "tsp": ("tsp", gen_tsp, [("exact", [8, 10, 12, 14, 16]), ("heuristic", [8, 10, 12, 14, 16, 100, 200, 400, 800])],
            "cost", "exacto O(n^2 2^n); heuristica O(n^2) por pasada"),
    "flow": ("flow", gen_flow, [("dinic", [100, 200, 400, 800, 1600]), ("edmonds-karp", [100, 200, 400, 800, 1600])],
             "flow", "Dinic O(V^2 E); Edmonds-Karp O(V E^2), E = 20n"),
    "voronoi": ("voronoi", gen_voronoi, [("pruned", [100, 250, 500, 1000, 2000]), ("brute", [100, 250, 500, 1000, 2000])],
                None, "poda ~O(n^2 log n) peor caso; sin poda O(n^2 k)"),
    "matching": ("matching", gen_matching, [("kmp", [10000, 100000, 1000000, 4000000]), ("z", [10000, 100000, 1000000, 4000000])],
                 "matches", "O(n + m)"),
    "palindrome": ("palindrome", gen_palindrome, [("manacher", [10000, 100000, 1000000]), ("expand", [10000, 100000, 1000000])],
                   "ranges", "Manacher O(n); expansion O(n^2) peor caso"),
    "palindrome-worst": ("palindrome", gen_palindrome_worst, [("manacher", [1000, 4000, 16000, 64000]), ("expand", [1000, 4000, 16000, 64000])],
                         "ranges", "cadena 'AAAA...': peor caso de la expansion"),
    "common": ("common", gen_common, [("automaton", [1000, 4000, 16000, 64000, 256000]), ("dp", [1000, 2000, 4000, 8000])],
               "range", "automata O((n+m) sigma); DP O(n m)"),
}

QUICK_LIMIT = {"mst": 8000, "tsp": 400, "flow": 400, "voronoi": 500, "matching": 1000000,
               "palindrome": 100000, "palindrome-worst": 16000, "common": 16000}


def voronoi_areas(answer):
    def area(poly):
        return abs(sum(poly[i][0] * poly[(i + 1) % len(poly)][1] - poly[(i + 1) % len(poly)][0] * poly[i][1]
                       for i in range(len(poly)))) / 2
    return [area(cell) for cell in answer["cells"]]


def agrees(family, key, first, second):
    if family == "voronoi":
        a, b = voronoi_areas(first), voronoi_areas(second)
        return all(abs(x - y) <= 1e-6 * max(1, y) for x, y in zip(a, b))
    if family == "tsp":
        # El exacto es optimo: la heuristica nunca puede ser menor.
        return second["cost"] >= first["cost"]
    return first[key] == second[key]


def loglog_slope(points):
    points = [(n, t) for n, t in points if t and t > 0.005]
    if len(points) < 2:
        return None
    xs = [math.log(n) for n, _ in points]
    ys = [math.log(t) for _, t in points]
    mx, my = statistics.fmean(xs), statistics.fmean(ys)
    den = sum((x - mx) ** 2 for x in xs)
    return sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / den if den else None


# ------------------------------------------------------------------ casos limite

EDGE_CASES = [
    ("MST grafo no conexo", "mst", {"nodes": [0, 1, 2, 3], "edges": [{"u": 0, "v": 1, "weight": 1}, {"u": 2, "v": 3, "weight": 1}]}),
    ("MST un nodo", "mst", {"nodes": [0], "edges": []}),
    ("TSP un nodo", "tsp", {"distances": [[0]]}),
    ("TSP sin ciclo hamiltoniano", "tsp", {"distances": [[0, 5, -1], [-1, 0, 7], [-1, -1, 0]]}),
    ("TSP exacto con 21 nodos", "tsp", {"distances": [[0 if i == j else 1 for j in range(21)] for i in range(21)], "algorithm": "exact"}),
    ("Flujo origen = destino", "flow", {"n": 2, "edges": [{"u": 0, "v": 1, "capacity": 10}], "source": 0, "sink": 0}),
    ("Flujo sin camino", "flow", {"n": 3, "edges": [{"u": 0, "v": 1, "capacity": 10}], "source": 0, "sink": 2}),
    ("Flujo capacidad negativa", "flow", {"n": 2, "edges": [{"u": 0, "v": 1, "capacity": -1}], "source": 0, "sink": 1}),
    ("Voronoi un sitio", "voronoi", {"sites": [[5, 5]], "bounds": [0, 0, 10, 10]}),
    ("Voronoi sitios repetidos", "voronoi", {"sites": [[5, 5], [5, 5]], "bounds": [0, 0, 10, 10]}),
    ("Voronoi sitios colineales", "voronoi", {"sites": [[1, 5], [3, 5], [7, 5]], "bounds": [0, 0, 10, 10]}),
    ("Firma vacia y firma mas larga que el texto", "matching", {"transmissions": ["ABC"], "patterns": ["", "ABCD"]}),
    ("Firma al inicio y al final", "matching", {"transmissions": ["ABCDEF"], "patterns": ["AB", "EF"]}),
    ("Palindromo de cadena vacia", "palindrome", {"transmissions": ["", "A"]}),
    ("Palindromo con empate", "palindrome", {"transmissions": ["ABACDC"]}),
    ("Subcadena sin caracteres comunes", "common", {"transmissions": ["AAAA", "BBBB"]}),
    ("Subcadena con caracteres no hex", "common", {"transmissions": ["ABCXXCBA", "ZZABCYYCBA"]}),
]


# ------------------------------------------------------------------ reporte

def environment(binary):
    def sh(cmd):
        try:
            return subprocess.run(cmd, capture_output=True, text=True, timeout=10).stdout.strip()
        except (OSError, subprocess.SubprocessError):
            return "n/d"
    cpu = sh(["sysctl", "-n", "machdep.cpu.brand_string"]) if sys.platform == "darwin" else sh(["sh", "-c", "grep -m1 'model name' /proc/cpuinfo | cut -d: -f2"])
    memory = sh(["sysctl", "-n", "hw.memsize"]) if sys.platform == "darwin" else sh(["sh", "-c", "grep MemTotal /proc/meminfo"])
    compiler = sh(["c++", "--version"]).splitlines()
    return {
        "fecha": datetime.datetime.now().isoformat(timespec="seconds"),
        "sistema": f"{platform.system()} {platform.release()} ({platform.machine()})",
        "cpu": cpu or "n/d",
        "memoria": f"{int(memory) / 2**30:.0f} GiB" if memory.isdigit() else memory,
        "compilador": compiler[0] if compiler else "n/d",
        "opciones": COMPILE_FLAGS,
        "python": platform.python_version(),
        "binario": str(binary),
        "repeticiones": REPEATS,
        "semilla": SEED,
    }


def write_reports(rows, slopes, edge_rows, env, quick):
    RESULTS.mkdir(exist_ok=True)
    with open(RESULTS / "benchmarks.csv", "w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    lines = ["# Resultados de benchmarks", "",
             f"Generado por `bench/benchmark.py` el {env['fecha']}{' (modo rapido)' if quick else ''}. "
             f"Mediana de {REPEATS} corridas; `elapsed_ms` es el tiempo del algoritmo medido en C++ "
             "(sin E/S JSON), `wall_ms` el del proceso completo y `RSS` la memoria residente maxima del proceso. "
             "Ambiente en [environment.md](environment.md).", ""]
    for family, (_, _, _, _, theory) in SUITES.items():
        family_rows = [r for r in rows if r["family"] == family]
        if not family_rows:
            continue
        lines += [f"## {family}", "", f"Complejidad teorica: {theory}.", "",
                  "| tecnica | n | elapsed_ms | wall_ms | RSS (MB) | coincide |", "|---|---:|---:|---:|---:|:---:|"]
        for r in family_rows:
            elapsed = "error" if r["elapsed_ms"] == "" else f"{float(r['elapsed_ms']):.3f}"
            wall = "" if r["wall_ms"] == "" else f"{float(r['wall_ms']):.1f}"
            rss = "" if r["peak_rss_mb"] == "" else f"{float(r['peak_rss_mb']):.1f}"
            lines.append(f"| {r['algorithm']} | {r['n']} | {elapsed} | {wall} | {rss} | {r['agree']} |")
        lines.append("")
        for algorithm, slope in slopes.get(family, {}).items():
            if slope is not None:
                lines.append(f"- Pendiente log-log de `{algorithm}`: **{slope:.2f}** (tiempo ~ n^{slope:.2f}).")
        lines.append("")
    lines += ["## Casos limite", "", "| caso | modo | respuesta |", "|---|---|---|"]
    for name, mode, answer in edge_rows:
        text = json.dumps(answer, ensure_ascii=False)
        lines.append(f"| {name} | {mode} | `{text[:110]}{'…' if len(text) > 110 else ''}` |")
    (RESULTS / "benchmarks.md").write_text("\n".join(lines) + "\n", encoding="utf-8")

    env_lines = ["# Ambiente de los benchmarks", ""] + [f"- **{k}**: {v}" for k, v in env.items()] + [
        "", "## Procedimiento", "",
        "```bash",
        f"c++ {COMPILE_FLAGS} src/main.cpp -o sentinel",
        "python3 bench/benchmark.py ./sentinel",
        "```", "",
        "Las instancias se generan con semilla fija, asi que el procedimiento es reproducible; "
        "los tiempos absolutos dependen del equipo.",
    ]
    (RESULTS / "environment.md").write_text("\n".join(env_lines) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary")
    parser.add_argument("--quick", action="store_true")
    args = parser.parse_args()
    binary = str(Path(args.binary).resolve())

    rows, slopes = [], {}
    for family, (mode, generator, plans, key, _) in SUITES.items():
        rng = random.Random(f"{SEED}-{family}")
        sizes = sorted({n for _, ns in plans for n in ns if not args.quick or n <= QUICK_LIMIT[family]})
        answers = {}
        for n in sizes:
            payload = generator(rng, n)
            for algorithm, algorithm_sizes in plans:
                if n not in algorithm_sizes:
                    continue
                answer, elapsed, wall, rss = measure(binary, mode, payload, algorithm)
                answers[(algorithm, n)] = answer
                print(f"{family:17s} {algorithm:13s} n={n:>8} "
                      f"{'ERROR ' + answer['error'] if elapsed is None else f'{elapsed:10.3f} ms  rss {rss:6.1f} MB'}", flush=True)
                rows.append({"family": family, "mode": mode, "algorithm": algorithm, "n": n,
                             "elapsed_ms": "" if elapsed is None else round(elapsed, 4),
                             "wall_ms": "" if wall is None else round(wall, 2),
                             "peak_rss_mb": "" if rss is None else round(rss, 2), "agree": ""})
            first, second = plans[0][0], plans[1][0]
            if (first, n) in answers and (second, n) in answers:
                a, b = answers[(first, n)], answers[(second, n)]
                ok = "error" not in a and "error" not in b and agrees(family, key, a, b)
                for row in rows[-2:]:
                    row["agree"] = "si" if ok else "NO"
        slopes[family] = {algorithm: loglog_slope([(r["n"], r["elapsed_ms"] or None) for r in rows
                                                   if r["family"] == family and r["algorithm"] == algorithm
                                                   and (family != "tsp" or algorithm == "exact" or r["n"] >= 100)])
                          for algorithm, _ in plans}

    edge_rows = [(name, mode, run_once(binary, mode, payload)[0]) for name, mode, payload in EDGE_CASES]
    for name, _, answer in edge_rows:
        answer.pop("elapsed_ms", None)
    write_reports(rows, slopes, edge_rows, environment(binary), args.quick)
    disagreements = [r for r in rows if r["agree"] == "NO"]
    print(f"\nEscrito en {RESULTS}/ ({len(rows)} mediciones, {len(disagreements)} desacuerdos)")
    sys.exit(1 if disagreements else 0)


if __name__ == "__main__":
    main()
