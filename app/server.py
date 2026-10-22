"""SENTINEL Digital Twin: servidor HTTP (solo biblioteca estandar).

Sirve la interfaz web y ejecuta el nucleo C++ (`sentinel`) para cada calculo: la interfaz
nunca usa resultados precalculados. El servidor solo prepara entradas (lectura de CSV,
proyeccion plana para Voronoi, distancias por caminos minimos para el TSP sobre la red) y
reenvia la respuesta JSON del ejecutable.

Uso:
    python3 app/server.py [--port 8000]
Variables: SENTINEL_BIN (ruta al ejecutable; por defecto ./sentinel, se compila si falta).
"""
import argparse
import csv
import json
import math
import os
import shutil
import subprocess
import sys
import time
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

APP_DIR = Path(__file__).resolve().parent
REPO_DIR = APP_DIR.parent
DATA_DIR = APP_DIR / "data"
STATIC_DIR = APP_DIR / "static"
BINARY = Path(os.environ.get("SENTINEL_BIN", REPO_DIR / "sentinel")).resolve()
RUN_TIMEOUT_S = 30
EARTH_RADIUS_KM = 6371.0
MODES = {"mst", "tsp", "flow", "voronoi", "matching", "palindrome", "common"}


# ---------------------------------------------------------------- nucleo C++

def ensure_binary():
    """Compila src/main.cpp si el ejecutable no existe o es mas viejo que las fuentes."""
    sources = list((REPO_DIR / "src").glob("*"))
    newest = max(path.stat().st_mtime for path in sources)
    if BINARY.exists() and BINARY.stat().st_mtime >= newest:
        return
    compiler = shutil.which("c++") or shutil.which("g++") or shutil.which("clang++")
    if compiler is None:
        sys.exit("No se encontro un compilador de C++ y no existe " + str(BINARY))
    print(f"Compilando {BINARY.name} con {compiler} ...", flush=True)
    subprocess.run([compiler, "-std=c++17", "-O2", str(REPO_DIR / "src" / "main.cpp"), "-o", str(BINARY)],
                   check=True)


def run_sentinel(mode, payload):
    """Ejecuta el nucleo C++ con {mode, input} y regresa (respuesta, ms de pared)."""
    start = time.perf_counter()
    proc = subprocess.run([str(BINARY)], input=json.dumps({"mode": mode, "input": payload}),
                          capture_output=True, text=True, timeout=RUN_TIMEOUT_S)
    wall_ms = (time.perf_counter() - start) * 1000
    try:
        answer = json.loads(proc.stdout)
    except json.JSONDecodeError:
        answer = {"error": f"salida invalida del nucleo (codigo {proc.returncode}): {proc.stdout[:200]}"}
    return answer, wall_ms


# ---------------------------------------------------------------- datos

def read_csv(name):
    with open(DATA_DIR / name, newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def load_scenarios():
    with open(DATA_DIR / "scenarios.json", encoding="utf-8") as handle:
        return json.load(handle)


def project(lat, lon, lat0):
    """Proyeccion equirectangular en km centrada en la latitud lat0 (para Voronoi)."""
    x = EARTH_RADIUS_KM * math.radians(lon) * math.cos(math.radians(lat0))
    y = EARTH_RADIUS_KM * math.radians(lat)
    return [round(x, 3), round(y, 3)]


def link_key(u, v):
    return "-".join(sorted((u, v)))


def all_pairs_shortest_paths(n, edges):
    """Floyd-Warshall sobre los costos: un tramo del TSP puede pasar por nodos intermedios."""
    dist = [[0 if i == j else math.inf for j in range(n)] for i in range(n)]
    nxt = [[j if i == j else -1 for j in range(n)] for i in range(n)]
    for edge in edges:
        u, v, cost = edge["ui"], edge["vi"], edge["cost"]
        if cost < dist[u][v]:
            dist[u][v] = dist[v][u] = cost
            nxt[u][v], nxt[v][u] = v, u
    for k in range(n):
        for i in range(n):
            for j in range(n):
                if dist[i][k] + dist[k][j] < dist[i][j]:
                    dist[i][j] = dist[i][k] + dist[k][j]
                    nxt[i][j] = nxt[i][k]
    return dist, nxt


def expand_path(nxt, u, v):
    if nxt[u][v] == -1:
        return []
    path = [u]
    while u != v:
        u = nxt[u][v]
        path.append(u)
    return path


def build_scenario(scenario, disabled=()):
    """Arma todo lo que la interfaz necesita para un escenario (sin ejecutar algoritmos)."""
    nodes = read_csv("nodes.csv")
    index = {node["id"]: i for i, node in enumerate(nodes)}
    domain = scenario["voronoi"]["domain"]
    lat0 = (domain["lat_min"] + domain["lat_max"]) / 2

    capacities = {link_key(row["u"], row["v"]): int(row["capacity_gbps"]) for row in read_csv("capacities.csv")}
    for override in scenario.get("capacity_overrides", []):
        capacities[link_key(override["u"], override["v"])] = int(override["capacity_gbps"])
    removed = {link_key(link["u"], link["v"]) for link in scenario.get("removed_links", [])}
    removed |= set(disabled)

    edges, removed_edges = [], []
    for row in read_csv("edges.csv"):
        key = link_key(row["u"], row["v"])
        if row["u"] not in index or row["v"] not in index:
            continue
        edge = {
            "key": key, "u": row["u"], "v": row["v"], "ui": index[row["u"]], "vi": index[row["v"]],
            "cost": int(row["cost"]), "distance_km": float(row["distance_km"]),
            "capacity": capacities.get(key, 10),
        }
        (removed_edges if key in removed else edges).append(edge)

    node_list = []
    for i, node in enumerate(nodes):
        lat, lon = float(node["lat"]), float(node["lon"])
        in_domain = domain["lat_min"] <= lat <= domain["lat_max"] and domain["lon_min"] <= lon <= domain["lon_max"]
        node_list.append({"index": i, "id": node["id"], "name": node["name"], "city": node["city"],
                          "lat": lat, "lon": lon, "xy": project(lat, lon, lat0), "in_domain": in_domain})

    tsp_ids = [node["id"] for node in nodes] if scenario["tsp"]["nodes"] == "all" else scenario["tsp"]["nodes"]
    start = scenario["tsp"]["start"]
    tsp_ids = [start] + [node_id for node_id in tsp_ids if node_id != start]
    dist, nxt = all_pairs_shortest_paths(len(nodes), edges)
    tsp_idx = [index[node_id] for node_id in tsp_ids]
    reachable = all(dist[a][b] < math.inf for a in tsp_idx for b in tsp_idx)
    matrix = [[int(dist[a][b]) if dist[a][b] < math.inf else -1 for b in tsp_idx] for a in tsp_idx]
    legs = {f"{a}-{b}": expand_path(nxt, a, b) for a in tsp_idx for b in tsp_idx if a != b}

    countries = [{"country": row["country"], "lat": float(row["lat"]), "lon": float(row["lon"]),
                  "xy": project(float(row["lat"]), float(row["lon"]), lat0)} for row in read_csv("countries.csv")]
    lower = project(domain["lat_min"], domain["lon_min"], lat0)
    upper = project(domain["lat_max"], domain["lon_max"], lat0)

    demands = [{"source": row["source"], "target": row["target"], "demand_gbps": float(row["demand_gbps"])}
               for row in read_csv("demands.csv")]

    return {
        "scenario": {key: scenario[key] for key in ("id", "name", "description")},
        "synthetic_note": load_scenarios().get("note", ""),
        "nodes": node_list,
        "edges": edges,
        "removed_edges": removed_edges,
        "demands": demands,
        "flow": {**scenario["flow"], "source_index": index[scenario["flow"]["source"]],
                 "sink_index": index[scenario["flow"]["sink"]]},
        "tsp": {"start": start, "ids": tsp_ids, "indices": tsp_idx, "matrix": matrix,
                "legs": legs, "reachable": reachable},
        "voronoi": {"lat0": lat0, "domain": domain, "bounds": lower + upper, "countries": countries},
    }


def normalize(text):
    return "".join(text.split()).upper()


def resolve_link(link_text, nodes):
    """Traduce 'Austria – Suiza' (formato de mapping.csv) a los ids de las dos centrales."""
    by_name = {}
    for node in nodes:
        for name in (node["name"], node["name_es"], node["id"]):
            by_name[name.strip().lower()] = node["id"]
    parts = [part.strip().lower() for part in link_text.replace("—", "–").replace(" - ", "–").split("–")]
    ids = [by_name.get(part) for part in parts]
    return ids if len(ids) == 2 and all(ids) else [None, None]


def load_transmissions():
    raw = {name: (DATA_DIR / name).read_text(encoding="utf-8")
           for name in ("transmission1.txt", "transmission2.txt", "mcode1.txt", "mcode2.txt", "mcode3.txt")}
    nodes = read_csv("nodes.csv")
    mapping = []
    for row in read_csv("mapping.csv"):
        link_u, link_v = resolve_link(row["link"], nodes)
        mapping.append({**row, "link_u": link_u, "link_v": link_v})
    return {
        "transmissions": [normalize(raw["transmission1.txt"]), normalize(raw["transmission2.txt"])],
        "codes": [normalize(raw[f"mcode{i}.txt"]) for i in (1, 2, 3)],
        "mapping": mapping,
        "note": "mapping.csv es una construccion de simulacion: detectar una firma no confirma una infeccion real.",
    }


# ---------------------------------------------------------------- HTTP

class Handler(SimpleHTTPRequestHandler):
    def log_message(self, fmt, *args):
        if self.path.startswith("/api/"):
            sys.stderr.write(f"[api] {self.command} {self.path} -> {args[1] if len(args) > 1 else ''}\n")

    def send_json(self, payload, status=200):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def read_body(self):
        length = int(self.headers.get("Content-Length", 0))
        return json.loads(self.rfile.read(length) or b"{}")

    def do_GET(self):
        url = urlparse(self.path)
        try:
            if url.path == "/api/scenarios":
                data = load_scenarios()
                return self.send_json({"note": data.get("note", ""), "scenarios": [
                    {key: s[key] for key in ("id", "name", "description")} for s in data["scenarios"]]})
            if url.path.startswith("/api/scenario/"):
                scenario_id = url.path.rsplit("/", 1)[-1]
                matches = [s for s in load_scenarios()["scenarios"] if s["id"] == scenario_id]
                if not matches:
                    return self.send_json({"error": f"escenario desconocido: {scenario_id}"}, 404)
                disabled = [key for key in parse_qs(url.query).get("disabled", [""])[0].split(",") if key]
                return self.send_json(build_scenario(matches[0], disabled))
            if url.path == "/api/transmissions":
                return self.send_json(load_transmissions())
        except (KeyError, ValueError, OSError) as error:
            return self.send_json({"error": f"datos invalidos: {error}"}, 400)
        return super().do_GET()

    def do_POST(self):
        url = urlparse(self.path)
        try:
            body = self.read_body()
            if url.path == "/api/run":
                mode = body.get("mode")
                if mode not in MODES:
                    return self.send_json({"error": f"modo invalido: {mode}"}, 400)
                answer, wall_ms = run_sentinel(mode, body.get("input", {}))
                return self.send_json({"result": answer, "wall_ms": round(wall_ms, 2)},
                                      200 if "error" not in answer else 422)
            if url.path == "/api/scenario/custom":
                disabled = body.get("disabled", [])
                return self.send_json(build_scenario(body["scenario"], disabled))
        except subprocess.TimeoutExpired:
            return self.send_json({"error": f"el nucleo C++ excedio {RUN_TIMEOUT_S} s"}, 504)
        except (KeyError, ValueError, TypeError, json.JSONDecodeError) as error:
            return self.send_json({"error": f"solicitud invalida: {error}"}, 400)
        return self.send_json({"error": "ruta no encontrada"}, 404)


def main():
    parser = argparse.ArgumentParser(description="SENTINEL Digital Twin")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--host", default="127.0.0.1")
    args = parser.parse_args()
    ensure_binary()
    server = ThreadingHTTPServer((args.host, args.port), partial(Handler, directory=str(STATIC_DIR)))
    print(f"SENTINEL Digital Twin en http://{args.host}:{args.port}  (nucleo: {BINARY})", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nServidor detenido.")


if __name__ == "__main__":
    main()
