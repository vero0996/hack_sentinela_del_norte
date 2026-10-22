"""Verificador local de los modos mst, tsp, flow, voronoi, matching, palindrome y common.

Replica las referencias, generadores y validadores del autograder del sitio
(SENTINELA DEL NORTE v1.1.0-web) y el linter de estilo aplicado a src/main.cpp.

Uso:
    g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic src/main.cpp -o sentinel
    python3 tests/grader_check.py ./sentinel [src/main.cpp]
"""
import json
import math
import random
import re
import subprocess
import sys
import time
from collections import deque

PROFILES = {
    "lite": dict(mst_n=400, mst_extra=2000, tsp_n=60, flow_n=80, flow_extra=600, voronoi_n=80,
                 matching_len=50000, pal_len=50000, common_len=12000),
    "normal": dict(mst_n=1200, mst_extra=7000, tsp_n=120, flow_n=180, flow_extra=1800, voronoi_n=180,
                   matching_len=220000, pal_len=220000, common_len=55000),
    "huge": dict(mst_n=5000, mst_extra=30000, tsp_n=450, flow_n=450, flow_extra=9000, voronoi_n=500,
                 matching_len=1000000, pal_len=1000000, common_len=180000),
}
MODES = ("mst", "tsp", "flow", "voronoi", "matching", "palindrome", "common")
HEX = "0123456789ABCDEF"


def close(a, b, tol=1e-5):
    return isinstance(a, (int, float)) and abs(a - b) <= tol * max(1, abs(b))


# ---------- referencias (puertos de las funciones del sitio) ----------

def ref_mst(case):
    n = len(case["nodes"])
    parent = list(range(n))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    cost = picked = 0
    for w, u, v in sorted((e["weight"], e["u"], e["v"]) for e in case["edges"]):
        a, b = find(u), find(v)
        if a != b:
            parent[b] = a
            cost += w
            picked += 1
    return cost


def ref_tsp(case):
    d = case["distances"]
    n = len(d)
    if n <= 1:
        return 0
    full = (1 << n) - 1
    dp = [[math.inf] * n for _ in range(1 << n)]
    dp[1][0] = 0
    for mask in range(1, 1 << n, 2):
        for u in range(n):
            if dp[mask][u] < math.inf:
                for v in range(1, n):
                    if not mask & (1 << v):
                        nxt = mask | (1 << v)
                        dp[nxt][v] = min(dp[nxt][v], dp[mask][u] + d[u][v])
    return min(dp[full][u] + d[u][0] for u in range(1, n))


def ref_flow(case):
    n = case["n"]
    graph = [[] for _ in range(n)]
    to, cap = [], []
    for e in case["edges"]:
        graph[e["u"]].append(len(to)); to.append(e["v"]); cap.append(e["capacity"])
        graph[e["v"]].append(len(to)); to.append(e["u"]); cap.append(0)
    s, t, total = case["source"], case["sink"], 0
    while True:
        par = [-1] * n
        par[s] = -2
        queue = deque([s])
        while queue and par[t] == -1:
            u = queue.popleft()
            for i in graph[u]:
                if cap[i] > 0 and par[to[i]] == -1:
                    par[to[i]] = i
                    queue.append(to[i])
        if par[t] == -1:
            return total
        bottleneck, v = math.inf, t
        while v != s:
            i = par[v]; bottleneck = min(bottleneck, cap[i]); v = to[i ^ 1]
        v = t
        while v != s:
            i = par[v]; cap[i] -= bottleneck; cap[i ^ 1] += bottleneck; v = to[i ^ 1]
        total += bottleneck


def ref_voronoi(case):
    sites = case["sites"]
    x0, y0, x1, y1 = case["bounds"]
    cells = []
    for ci, (e, s) in enumerate(sites):
        poly = [[x0, y0], [x1, y0], [x1, y1], [x0, y1]]
        for ni, (r, i) in enumerate(sites):
            if ci == ni or not poly:
                continue
            a, o, u = 2 * (r - e), 2 * (i - s), r * r + i * i - e * e - s * s
            out = []
            for k in range(len(poly)):
                p, q = poly[k], poly[(k + 1) % len(poly)]
                rp, rq = a * p[0] + o * p[1] - u, a * q[0] + o * q[1] - u
                if rp <= 1e-9:
                    out.append(p)
                if (rp < -1e-9 and rq > 1e-9) or (rp > 1e-9 and rq < -1e-9):
                    t = rp / (rp - rq)
                    out.append([p[0] + t * (q[0] - p[0]), p[1] + t * (q[1] - p[1])])
            poly = out
        cells.append(poly)
    return cells


def ref_matching(case):
    return [[(t.find(p) + 1) if t.find(p) >= 0 else -1 for p in case["patterns"]]
            for t in case["transmissions"]]


def ref_palindrome_one(s):
    # Manacher sobre "^#a#b#...#$", igual que la referencia del sitio.
    if not s:
        return [0, 0]
    t = "^#" + "#".join(s) + "#$"
    radius = [0] * len(t)
    center = right = 0
    best, start = 1, 0
    for e in range(1, len(t) - 1):
        if e < right:
            radius[e] = min(right - e, radius[2 * center - e])
        while t[e + 1 + radius[e]] == t[e - 1 - radius[e]]:
            radius[e] += 1
        if e + radius[e] > right:
            center, right = e, e + radius[e]
        length, begin = radius[e], (e - radius[e]) >> 1
        if length > best or (length == best and begin < start):
            best, start = length, begin
    return [start + 1, start + best]


def ref_common(case):
    first, second = case["transmissions"]
    previous = [0] * (len(second) + 1)
    best = start = second_start = 0
    for i in range(len(first)):
        current = [0] * (len(second) + 1)
        for j in range(len(second)):
            if first[i] == second[j]:
                length = current[j + 1] = previous[j] + 1
                s1, s2 = i - length + 1, j - length + 1
                if length > best or (length == best and (s1 < start or (s1 == start and s2 < second_start))):
                    best, start, second_start = length, s1, s2
        previous = current
    return [0, 0] if best == 0 else [start + 1, start + best]


def area(poly):
    if len(poly) < 3:
        return 0
    return abs(sum(poly[k][0] * poly[(k + 1) % len(poly)][1] - poly[(k + 1) % len(poly)][0] * poly[k][1]
                   for k in range(len(poly)))) / 2


# ---------- validadores (mismas reglas que el sitio) ----------

def validate(mode, case, ans, exact=True, params=None):
    if not isinstance(ans, dict):
        return False, "response is not a JSON object"
    if mode == "mst":
        n = len(case["nodes"])
        edges = ans.get("edges")
        if not isinstance(edges, list) or len(edges) != n - 1:
            return False, "MST must return exactly n-1 edges"
        weights = {}
        for e in case["edges"]:
            key = (min(e["u"], e["v"]), max(e["u"], e["v"]))
            weights[key] = min(weights.get(key, math.inf), e["weight"])
        parent = list(range(n))

        def find(x):
            while parent[x] != x:
                parent[x] = parent[parent[x]]
                x = parent[x]
            return x

        total = 0
        for e in edges:
            u, v = e[0], e[1]
            key = (min(u, v), max(u, v))
            if key not in weights:
                return False, "MST uses a non-existing edge"
            a, b = find(u), find(v)
            if a == b:
                return False, "MST contains cycle"
            parent[a] = b
            total += weights[key]
        if len({find(i) for i in range(n)}) != 1:
            return False, "MST not connected"
        if not close(ans.get("cost"), total):
            return False, "reported MST cost does not match selected edges"
        return (True, "ok") if close(total, ref_mst(case)) else (False, "MST is not minimum")
    if mode == "tsp":
        d = case["distances"]
        n = len(d)
        route = ans.get("route")
        if not isinstance(route, list) or len(route) != n + 1:
            return False, "TSP route must have n+1 nodes"
        if route[0] != 0 or route[n] != 0 or sorted(route[:-1]) != list(range(n)):
            return False, "TSP route is not a Hamiltonian cycle from node 0"
        total = sum(d[route[k]][route[k + 1]] for k in range(n))
        if not close(ans.get("cost"), total):
            return False, "reported TSP cost does not match route"
        if exact and not close(total, ref_tsp(case)):
            return False, "TSP is not optimal on exact test"
        return True, "ok"
    if mode == "flow":
        return (True, "ok") if close(ans.get("flow"), ref_flow(case)) else (False, "wrong maximum flow")
    if mode == "matching":
        ok = ans.get("matches") == ref_matching(case)
        return (True, "ok") if ok else (False, "wrong string-match positions")
    if mode == "palindrome":
        ok = ans.get("ranges") == [ref_palindrome_one(t) for t in case["transmissions"]]
        return (True, "ok") if ok else (False, "wrong longest-palindrome ranges")
    if mode == "common":
        got = ans.get("range")
        if exact:
            return (True, "ok") if got == ref_common(case) else (False, "wrong longest-common-substring range")
        # Prueba grande: el sitio solo revisa longitud minima y presencia en la segunda transmision.
        if not (isinstance(got, list) and len(got) == 2 and all(isinstance(v, int) for v in got)):
            return False, "invalid common-substring response"
        first, second = case["transmissions"]
        if got == [0, 0]:
            return False, "empty range on large common-substring test"
        if not 1 <= got[0] <= got[1] <= len(first):
            return False, "invalid common-substring range"
        piece = first[got[0] - 1:got[1]]
        ok = len(piece) >= max(500, params["common_len"] // 8) and piece in second
        return (True, "ok") if ok else (False, "returned common substring is too short or absent in second transmission")
    expected = ref_voronoi(case)
    cells = ans.get("cells")
    if not isinstance(cells, list) or len(cells) != len(case["sites"]):
        return False, "wrong number of Voronoi cells"
    for k, cell in enumerate(cells):
        if not isinstance(cell, list) or len(cell) < 3:
            return False, f"invalid Voronoi cell {k}"
        if not close(area(cell), area(expected[k]), 0.003):
            return False, f"wrong Voronoi area for cell {k}"
    return True, "ok"


# ---------- generadores (misma forma que los del sitio) ----------

def functional_cases(mode, rng):
    cases = []
    if mode == "mst":
        for n in (4, 7, 11):
            edges = [{"u": k, "v": k + 1, "weight": rng.randint(1, 25)} for k in range(n - 1)]
            edges += [{"u": a, "v": b, "weight": rng.randint(1, 25)}
                      for a in range(n) for b in range(a + 2, n) if rng.random() < 0.4]
            cases.append({"nodes": list(range(n)), "edges": edges})
    elif mode == "tsp":
        for n in (4, 6, 9):
            d = [[0] * n for _ in range(n)]
            for a in range(n):
                for b in range(a + 1, n):
                    d[a][b] = d[b][a] = rng.randint(1, 60)
            cases.append({"distances": d})
    elif mode == "flow":
        for n in (4, 7, 10):
            edges = [{"u": k, "v": k + 1, "capacity": rng.randint(5, 30)} for k in range(n - 1)]
            edges += [{"u": a, "v": b, "capacity": rng.randint(1, 35)}
                      for a in range(n) for b in range(n) if a != b and rng.random() < 0.18]
            cases.append({"n": n, "edges": edges, "source": 0, "sink": n - 1})
    elif mode == "voronoi":
        grid = [[x, y] for x in range(5, 96, 10) for y in range(5, 96, 10)]
        for k in (2, 4, 7):
            cases.append({"sites": rng.sample(grid, k), "bounds": [0, 0, 100, 100]})
    elif mode == "matching":
        # Casos fijos del sitio + uno aleatorio con firma insertada.
        cases.append({"transmissions": ["AAABCDFFF", "001234ABCDEF"], "patterns": ["ABCD", "1234", "FFFF"]})
        a = "".join(rng.choice(HEX) for _ in range(2500))
        b = "".join(rng.choice(HEX) for _ in range(2600))
        sig = "FEDCBA9876543210"
        cases.append({"transmissions": [a[:1300] + sig + a[1300:], b], "patterns": [sig, "ABC", "0000000"]})
    elif mode == "palindrome":
        cases.append({"transmissions": ["AABCDCBAE", "ABCBA123321"]})
        cases.append({"transmissions": ["ABCDEF", "AAAABAAAA"]})
        cases.append({"transmissions": ["".join(rng.choice("AB") for _ in range(rng.randint(0, 30)))
                                        for _ in range(2)]})
    else:
        cases.append({"transmissions": ["AA1234BBB", "DD1234EE"]})
        cases.append({"transmissions": ["ABCXXCBA", "ZZABCYYCBA"]})
        cases.append({"transmissions": ["".join(rng.choice("ABC") for _ in range(rng.randint(0, 30)))
                                        for _ in range(2)]})
    return cases


def stress_case(mode, rng, p):
    if mode == "mst":
        n = p["mst_n"]
        edges = [{"u": k, "v": k + 1, "weight": rng.randint(1, 1000)} for k in range(n - 1)]
        seen = {(e["u"], e["v"]) for e in edges}
        while len(edges) < n - 1 + p["mst_extra"]:
            a, b = sorted((rng.randrange(n), rng.randrange(n)))
            if a != b and (a, b) not in seen:
                seen.add((a, b))
                edges.append({"u": a, "v": b, "weight": rng.randint(1, 1000)})
        return {"nodes": list(range(n)), "edges": edges}
    if mode == "tsp":
        n = p["tsp_n"]
        pts = []
        for k in range(n):
            angle, radius = 2 * math.pi * k / n, 100000 + rng.randint(-100, 100)
            pts.append((radius * math.cos(angle), radius * math.sin(angle)))
        d = [[round(math.hypot(a[0] - b[0], a[1] - b[1])) for b in pts] for a in pts]
        return {"distances": d}
    if mode == "flow":
        n = p["flow_n"]
        edges = [{"u": k, "v": k + 1, "capacity": rng.randint(100, 10000)} for k in range(n - 1)]
        seen = {(e["u"], e["v"]) for e in edges}
        while len(edges) < n - 1 + p["flow_extra"]:
            a, b = rng.randrange(n), rng.randrange(n)
            if a != b and (a, b) not in seen:
                seen.add((a, b))
                edges.append({"u": a, "v": b, "capacity": rng.randint(1, 10000)})
        return {"n": n, "edges": edges, "source": 0, "sink": n - 1}
    if mode == "voronoi":
        pts = set()
        while len(pts) < p["voronoi_n"]:
            pts.add((rng.randint(1, 99999), rng.randint(1, 99999)))
        return {"sites": [list(q) for q in pts], "bounds": [0, 0, 100000, 100000]}

    def hex_string(length):
        return "".join(rng.choices(HEX, k=length))

    if mode == "matching":
        n = p["matching_len"]
        sigs = ["DEADBEEFCAFEBABE", "0123456789ABCDEF0123456789ABCDEF",
                "FACEFACEFACEFACEFACE", "FFFFEEEEDDDDCCCCBBBB"]
        a, b = hex_string(n), hex_string(n)
        a = a[:n // 3] + sigs[0] + a[n // 3:]
        b = b[:2 * n // 3] + sigs[1] + b[2 * n // 3:]
        return {"transmissions": [a, b], "patterns": sigs}
    if mode == "palindrome":
        n = p["pal_len"]
        half = hex_string(n // 4)
        middle = half + "A" + half[::-1]
        return {"transmissions": [hex_string(max(1, n // 4)) + middle + hex_string(max(1, n // 4)),
                                  hex_string(n)]}
    n = p["common_len"]
    shared = hex_string(max(500, n // 8))
    half = n // 2
    return {"transmissions": [hex_string(half) + shared + hex_string(half),
                              hex_string(half) + shared + hex_string(half)]}


# ---------- linter de estilo de main.cpp (puerto del sitio) ----------

def lint(source):
    lines = source.split("\n")
    head = "\n".join(lines[:25])
    findings = []
    checks = [
        (r"(descripcion|descripción|description|purpose|proposito|propósito)", "header_description"),
        (r"(autor|author)", "header_author"),
        (r"(matricula|matrícula|student\s*id|A0\d{7,8})", "header_id"),
        (r"(fecha|date|\d{4}[-/]\d{1,2}[-/]\d{1,2})", "header_date"),
    ]
    for pattern, rule in checks:
        if not re.search(pattern, head, re.I):
            findings.append((rule, 1))
    function = re.compile(r"^\s*(?:[\w:<>,*&]+\s+)+([A-Za-z_]\w*)\s*\([^;]*\)\s*(?:const\s*)?\{")
    klass = re.compile(r"^\s*(?:class|struct)\s+([A-Za-z_]\w*)")
    constant = re.compile(r"^\s*(?:const|constexpr)\s+[\w:<>,*&]+\s+([A-Za-z_]\w*)")
    for number, line in enumerate(lines, 1):
        code = line.split("//")[0]
        if re.search(r"[áéíóúÁÉÍÓÚüÜñÑ]", code):
            findings.append(("identifier_ascii", number))
        if re.search(r"(?<![\w.])\.\d+|\d+\.(?!\d)", code):
            findings.append(("decimal_literal", number))
        m = klass.match(code)
        if m and not re.fullmatch(r"[A-Z][A-Za-z0-9]*", m.group(1)):
            findings.append(("class_pascal_case", number))
        m = function.match(code)
        if m:
            if m.group(1) != "main" and not re.fullmatch(r"[a-z][A-Za-z0-9]*", m.group(1)):
                findings.append(("function_camel_case", number))
            before = "\n".join(lines[max(0, number - 6):number - 1])
            if "//" not in before and "/*" not in before and "*" not in before:
                findings.append(("function_comment", number))
        m = constant.match(code)
        if m and not re.fullmatch(r"[A-Z][A-Z0-9]*(?:_[A-Z0-9]+)*", m.group(1)):
            findings.append(("constant_upper_snake", number))
        if re.search(r"\b(if|for|while|switch)\s*\([^;]*\)\s*$", code):
            findings.append(("opening_brace", number))
        if code.count(";") > 1 and "for" not in code:
            findings.append(("one_statement_per_line", number))
    return findings


def run(binary, mode, case):
    start = time.time()
    proc = subprocess.run([binary], input=json.dumps({"mode": mode, "input": case}),
                          capture_output=True, text=True)
    seconds = time.time() - start
    if proc.returncode != 0:
        return None, f"exit {proc.returncode}: {proc.stdout.strip()[:200]}", seconds
    try:
        return json.loads(proc.stdout), "", seconds
    except json.JSONDecodeError:
        return None, "invalid_json", seconds


def main():
    binary = sys.argv[1]
    source_path = sys.argv[2] if len(sys.argv) > 2 else "src/main.cpp"
    rng = random.Random(2038)
    failures = 0

    for mode in MODES:
        passed = total = 0
        for _ in range(20):
            for case in functional_cases(mode, rng):
                ans, error, _ = run(binary, mode, case)
                ok, reason = (False, error) if ans is None else validate(mode, case, ans)
                total += 1
                passed += ok
                if not ok:
                    print(f"  FAIL {mode}: {reason}")
        failures += total - passed
        print(f"functional {mode:10s} {passed}/{total}")

    for profile, params in PROFILES.items():
        for mode in MODES:
            case = stress_case(mode, rng, params)
            ans, error, seconds = run(binary, mode, case)
            ok, reason = (False, error) if ans is None else validate(mode, case, ans, False, params)
            failures += not ok
            print(f"stress {profile:6s} {mode:10s} {'ok  ' if ok else 'FAIL'} {seconds:.3f}s {'' if ok else reason}")

    with open(source_path, encoding="utf-8") as handle:
        findings = lint(handle.read())
    print(f"style main.cpp: {max(0, 5 - 0.5 * len(findings))}/5 {findings if findings else ''}")
    sys.exit(1 if failures or findings else 0)


if __name__ == "__main__":
    main()
