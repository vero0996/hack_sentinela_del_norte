"""Genera los archivos de entrada del Digital Twin a partir de SNDlib GEANT.

DATOS REALES (sin modificar, en app/data/sndlib/geant.txt):
  - SNDlib `geant` (native format v1.0), https://sndlib.put.poznan.pl
    Topologia (22 nodos, 36 enlaces), coordenadas, capacidad de modulo (40 000 Mbit/s)
    y matriz de demandas (462 pares, Mbit/s; origen Uhlig/TOTEM, 2005).

TRANSFORMACIONES (documentadas, reproducibles):
  - Ids cortos (at1.at -> AT), nombres de pais en ingles/espanol y ciudad de cada central.
  - Costo del enlace = distancia geodesica (haversine, km) entre las coordenadas SNDlib,
    como indica el reto ("distancias geodesicas ilustrativas"), no el costo de modulo SNDlib
    (que se conserva en edges.csv para trazabilidad).
  - Capacidad base = capacidad de modulo SNDlib (40 Gbps); demandas convertidas a Gbps.

CONSTRUCCIONES DE SIMULACION (no son datos de SNDlib):
  - Escenarios con capacidades de 10/100 Gbps, enlaces caidos y configuraciones de prueba.
  - Transmisiones hexadecimales, firmas mcode y mapping.csv (formato del sitio del reto).
  - countries.csv: centroides aproximados de paises para consultas de Voronoi.

Uso: python3 app/data/generate_data.py   (reproducible, semilla fija)
"""
import csv
import json
import math
import random
import re
from pathlib import Path

OUT = Path(__file__).resolve().parent
SNDLIB_FILE = OUT / "sndlib" / "geant.txt"
SEED = 2038
EARTH_RADIUS_KM = 6371.0

# sndlib id -> (id corto, nombre en ingles, nombre en espanol, ciudad)
NODE_INFO = {
    "at1.at": ("AT", "Austria", "Austria", "Vienna"),
    "be1.be": ("BE", "Belgium", "Belgica", "Brussels"),
    "ch1.ch": ("CH", "Switzerland", "Suiza", "Geneva"),
    "cz1.cz": ("CZ", "Czechia", "Chequia", "Prague"),
    "de1.de": ("DE", "Germany", "Alemania", "Frankfurt"),
    "es1.es": ("ES", "Spain", "Espana", "Madrid"),
    "fr1.fr": ("FR", "France", "Francia", "Paris"),
    "gr1.gr": ("GR", "Greece", "Grecia", "Athens"),
    "hr1.hr": ("HR", "Croatia", "Croacia", "Zagreb"),
    "hu1.hu": ("HU", "Hungary", "Hungria", "Budapest"),
    "ie1.ie": ("IE", "Ireland", "Irlanda", "Dublin"),
    "il1.il": ("IL", "Israel", "Israel", "Tel Aviv"),
    "it1.it": ("IT", "Italy", "Italia", "Milan"),
    "lu1.lu": ("LU", "Luxembourg", "Luxemburgo", "Luxembourg"),
    "nl1.nl": ("NL", "Netherlands", "Paises Bajos", "Amsterdam"),
    "ny1.ny": ("NY", "United States", "Estados Unidos", "New York"),
    "pl1.pl": ("PL", "Poland", "Polonia", "Poznan"),
    "pt1.pt": ("PT", "Portugal", "Portugal", "Lisbon"),
    "se1.se": ("SE", "Sweden", "Suecia", "Stockholm"),
    "si1.si": ("SI", "Slovenia", "Eslovenia", "Ljubljana"),
    "sk1.sk": ("SK", "Slovakia", "Eslovaquia", "Bratislava"),
    "uk1.uk": ("UK", "United Kingdom", "Reino Unido", "London"),
}

# Escenario simulado de capacidades mixtas: troncales a 100 Gbps y enlaces perifericos a 10 Gbps.
UPGRADED_100G = [("UK", "NL"), ("UK", "FR"), ("DE", "NL"), ("DE", "FR"), ("CH", "FR"),
                 ("CH", "IT"), ("DE", "IT"), ("AT", "DE"), ("CZ", "DE"), ("DE", "GR"), ("GR", "IT")]
LEGACY_10G = [("IE", "UK"), ("PT", "UK"), ("IL", "NL"), ("SE", "UK"), ("HR", "HU"), ("LU", "FR")]

COUNTRIES = [
    ("Albania", 41.15, 20.17), ("Andorra", 42.55, 1.60), ("Austria", 47.52, 14.55),
    ("Belarus", 53.71, 27.95), ("Belgium", 50.50, 4.47), ("Bosnia and Herzegovina", 43.92, 17.68),
    ("Bulgaria", 42.73, 25.49), ("Croatia", 45.10, 15.20), ("Cyprus", 35.13, 33.43),
    ("Czechia", 49.82, 15.47), ("Denmark", 56.26, 9.50), ("Estonia", 58.60, 25.01),
    ("Finland", 61.92, 25.75), ("France", 46.23, 2.21), ("Germany", 51.17, 10.45),
    ("Greece", 39.07, 21.82), ("Hungary", 47.16, 19.50), ("Iceland", 64.96, -19.02),
    ("Ireland", 53.41, -8.24), ("Israel", 31.05, 34.85), ("Italy", 41.87, 12.57),
    ("Kosovo", 42.60, 20.90), ("Latvia", 56.88, 24.60), ("Lithuania", 55.17, 23.88),
    ("Luxembourg", 49.82, 6.13), ("Malta", 35.94, 14.38), ("Moldova", 47.41, 28.37),
    ("Montenegro", 42.71, 19.37), ("Netherlands", 52.13, 5.29), ("North Macedonia", 41.61, 21.75),
    ("Norway", 60.47, 8.47), ("Poland", 51.92, 19.15), ("Portugal", 39.40, -8.22),
    ("Romania", 45.94, 24.97), ("Serbia", 44.02, 21.01), ("Slovakia", 48.67, 19.70),
    ("Slovenia", 46.15, 14.99), ("Spain", 40.46, -3.75), ("Sweden", 60.13, 18.64),
    ("Switzerland", 46.82, 8.23), ("Turkey", 38.96, 35.24), ("Ukraine", 48.38, 31.17),
    ("United Kingdom", 55.38, -3.44),
]

SIGNATURES = {
    "mcode1.txt": "DEADBEEFCAFE",
    "mcode2.txt": "0F1E2D3C4B5A",
    "mcode3.txt": "C0FFEE00BAD5EED",
}


def section(text, name):
    match = re.search(rf"^{name} \((.*?)^\)", text, re.S | re.M)
    if match is None:
        raise ValueError(f"Seccion {name} no encontrada en {SNDLIB_FILE}")
    return [line.strip() for line in match.group(1).splitlines() if line.strip()]


def parse_sndlib(path):
    """Lee nodos, enlaces y demandas del formato nativo de SNDlib."""
    text = path.read_text(encoding="utf-8")
    nodes = []
    for line in section(text, "NODES"):
        node_id, lon, lat = re.match(r"(\S+) \( (\S+) (\S+) \)", line).groups()
        nodes.append((node_id, float(lat), float(lon)))
    links = []
    for line in section(text, "LINKS"):
        match = re.match(r"(\S+) \( (\S+) (\S+) \) \S+ \S+ \S+ \S+ \( (\S+) (\S+) \)", line)
        link_id, source, target, module_capacity, module_cost = match.groups()
        links.append((link_id, source, target, float(module_capacity), float(module_cost)))
    demands = []
    for line in section(text, "DEMANDS"):
        match = re.match(r"\S+ \( (\S+) (\S+) \) \S+ (\S+)", line)
        source, target, value = match.groups()
        demands.append((source, target, float(value)))
    return nodes, links, demands


def haversine(lat1, lon1, lat2, lon2):
    lat1, lon1, lat2, lon2 = map(math.radians, (lat1, lon1, lat2, lon2))
    h = math.sin((lat2 - lat1) / 2) ** 2 + math.cos(lat1) * math.cos(lat2) * math.sin((lon2 - lon1) / 2) ** 2
    return 2 * EARTH_RADIUS_KM * math.asin(math.sqrt(h))


def write_csv(name, header, rows):
    with open(OUT / name, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(header)
        writer.writerows(rows)


def wrap(text, width=64):
    return "\n".join(text[i:i + width] for i in range(0, len(text), width)) + "\n"


def make_transmissions(rng):
    hex_digits = "0123456789ABCDEF"

    def noise(n):
        return "".join(rng.choice(hex_digits) for _ in range(n))

    half = noise(40)
    palindrome1 = half + "7" + half[::-1]           # palindromo de 81 en transmission1
    shared = noise(120)                             # fragmento compartido entre ambas
    palindrome2 = "A5C3" + "E" * 6 + "3C5A"         # palindromo par en transmission2
    t1 = (noise(700) + SIGNATURES["mcode1.txt"] + noise(500) + palindrome1 + noise(600)
          + shared + noise(400) + SIGNATURES["mcode2.txt"] + noise(300))
    t2 = (noise(350) + SIGNATURES["mcode2.txt"] + noise(800) + shared + noise(450)
          + SIGNATURES["mcode3.txt"] + noise(300) + palindrome2 + noise(500))
    return t1, t2


def scenario(scenario_id, name, description, flow, tsp_nodes="all", overrides=(), removed=()):
    return {
        "id": scenario_id,
        "name": name,
        "description": description,
        "flow": {"source": flow[0], "sink": flow[1]},
        "tsp": {"start": "AT", "nodes": tsp_nodes},
        "voronoi": {"domain": {"lon_min": -25, "lat_min": 30, "lon_max": 45, "lat_max": 72}},
        "capacity_overrides": [{"u": u, "v": v, "capacity_gbps": c} for u, v, c in overrides],
        "removed_links": [{"u": u, "v": v} for u, v in removed],
    }


def main():
    sndlib_nodes, sndlib_links, sndlib_demands = parse_sndlib(SNDLIB_FILE)
    short = {sndlib_id: NODE_INFO[sndlib_id][0] for sndlib_id, _, _ in sndlib_nodes}
    coords = {sndlib_id: (lat, lon) for sndlib_id, lat, lon in sndlib_nodes}

    write_csv("nodes.csv", ["id", "sndlib_id", "name", "name_es", "city", "lat", "lon"], [
        (info[0], sndlib_id, info[1], info[2], info[3], coords[sndlib_id][0], coords[sndlib_id][1])
        for sndlib_id, info in ((n[0], NODE_INFO[n[0]]) for n in sndlib_nodes)
    ])

    edges, capacities = [], []
    for link_id, source, target, module_capacity, module_cost in sndlib_links:
        distance = haversine(*coords[source], *coords[target])
        edges.append((short[source], short[target], link_id, round(distance, 1), round(distance), module_cost))
        capacities.append((short[source], short[target], round(module_capacity / 1000)))
    write_csv("edges.csv", ["u", "v", "sndlib_link", "distance_km", "cost", "sndlib_module_cost"], edges)
    write_csv("capacities.csv", ["u", "v", "capacity_gbps"], capacities)
    write_csv("demands.csv", ["source", "target", "demand_mbps", "demand_gbps"], [
        (short[s], short[t], value, round(value / 1000, 3)) for s, t, value in sndlib_demands
    ])
    write_csv("countries.csv", ["country", "lat", "lon"], COUNTRIES)

    scenarios = {
        "note": ("Topologia, coordenadas, capacidad base (40 Gbps) y demandas: SNDlib geant. "
                 "Costos = distancia geodesica. Capacidades 10/100 Gbps, fallas y transmisiones: simulacion."),
        "scenarios": [
            scenario("sndlib-base", "SNDlib GEANT (base)",
                     "Topologia real de SNDlib; todos los enlaces con el modulo SNDlib de 40 Gbps.",
                     ("UK", "GR")),
            scenario("mixed-capacity", "Capacidades mixtas (simulado)",
                     "Troncales (incluidos los dos enlaces hacia Grecia) a 100 Gbps y enlaces perifericos a 10 Gbps sobre la topologia SNDlib.",
                     ("UK", "GR"),
                     overrides=[(u, v, 100) for u, v in UPGRADED_100G] + [(u, v, 10) for u, v in LEGACY_10G]),
            scenario("small-tsp", "Ruta corta (TSP exacto)",
                     "Mantenimiento de 10 centrales de Europa central: Held-Karp exacto.",
                     ("DE", "HU"), tsp_nodes=["AT", "DE", "CH", "IT", "SI", "HR", "HU", "SK", "CZ", "PL"]),
            scenario("blackout-gr-it", "Blackout GR-IT (simulado)",
                     "Falla el enlace Atenas-Milan (en el MST y en el corte minimo): recalculo de MST, flujo y corte.",
                     ("UK", "GR"), removed=[("GR", "IT")]),
        ],
    }
    with open(OUT / "scenarios.json", "w", encoding="utf-8") as handle:
        json.dump(scenarios, handle, indent=2, ensure_ascii=False)
        handle.write("\n")

    t1, t2 = make_transmissions(random.Random(f"{SEED}-transmissions"))
    (OUT / "transmission1.txt").write_text(wrap(t1), encoding="utf-8")
    (OUT / "transmission2.txt").write_text(wrap(t2), encoding="utf-8")
    for name, signature in SIGNATURES.items():
        (OUT / name).write_text(signature + "\n", encoding="utf-8")
    # Mismo formato que el boton "Descargar mapping.csv" del sitio del reto.
    write_csv("mapping.csv", ["transmission", "link", "scenario", "status"], [
        ("transmission1.txt", "Austria – Suiza", "Tráfico benigno", "pending"),
        ("transmission2.txt", "Austria – Alemania", "Tráfico DDoS", "pending"),
    ])
    print(f"{len(sndlib_nodes)} nodos, {len(sndlib_links)} enlaces y {len(sndlib_demands)} demandas de SNDlib "
          f"escritos en {OUT}")


if __name__ == "__main__":
    main()
