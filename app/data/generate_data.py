"""Genera los datos SINTETICOS del Digital Twin (inspirados en GEANT, no son datos de SNDlib).

- Nodos: centrales nacionales en coordenadas reales de ciudades, tomando como modelo el
  conjunto de nodos de la red `geant` de SNDlib (sin NY) mas centrales nordicas/balticas/balcanicas.
- Enlaces: topologia plausible construida a mano; costo = distancia geodesica (haversine, km).
- Capacidades (10/40/100 Gbps), demandas, escenarios y transmisiones: construcciones de simulacion.

Uso: python3 app/data/generate_data.py   (reproducible, semilla fija)
"""
import csv
import json
import math
import random
from pathlib import Path

OUT = Path(__file__).resolve().parent
SEED = 2038
EARTH_RADIUS_KM = 6371.0

NODES = [
    # id, name, city, country, lat, lon
    ("AT", "Austria", "Vienna", "AT", 48.2082, 16.3738),
    ("BE", "Belgium", "Brussels", "BE", 50.8503, 4.3517),
    ("BG", "Bulgaria", "Sofia", "BG", 42.6977, 23.3219),
    ("CH", "Switzerland", "Geneva", "CH", 46.2044, 6.1432),
    ("CZ", "Czechia", "Prague", "CZ", 50.0755, 14.4378),
    ("DE", "Germany", "Frankfurt", "DE", 50.1109, 8.6821),
    ("DK", "Denmark", "Copenhagen", "DK", 55.6761, 12.5683),
    ("EE", "Estonia", "Tallinn", "EE", 59.4370, 24.7536),
    ("ES", "Spain", "Madrid", "ES", 40.4168, -3.7038),
    ("FI", "Finland", "Helsinki", "FI", 60.1699, 24.9384),
    ("FR", "France", "Paris", "FR", 48.8566, 2.3522),
    ("GR", "Greece", "Athens", "GR", 37.9838, 23.7275),
    ("HR", "Croatia", "Zagreb", "HR", 45.8150, 15.9819),
    ("HU", "Hungary", "Budapest", "HU", 47.4979, 19.0402),
    ("IE", "Ireland", "Dublin", "IE", 53.3498, -6.2603),
    ("IL", "Israel", "Tel Aviv", "IL", 32.0853, 34.7818),
    ("IT", "Italy", "Milan", "IT", 45.4642, 9.1900),
    ("LT", "Lithuania", "Vilnius", "LT", 54.6872, 25.2797),
    ("LU", "Luxembourg", "Luxembourg", "LU", 49.6116, 6.1319),
    ("LV", "Latvia", "Riga", "LV", 56.9496, 24.1052),
    ("NL", "Netherlands", "Amsterdam", "NL", 52.3676, 4.9041),
    ("NO", "Norway", "Oslo", "NO", 59.9139, 10.7522),
    ("PL", "Poland", "Poznan", "PL", 52.4064, 16.9252),
    ("PT", "Portugal", "Lisbon", "PT", 38.7223, -9.1393),
    ("RO", "Romania", "Bucharest", "RO", 44.4268, 26.1025),
    ("RS", "Serbia", "Belgrade", "RS", 44.7866, 20.4489),
    ("SE", "Sweden", "Stockholm", "SE", 59.3293, 18.0686),
    ("SI", "Slovenia", "Ljubljana", "SI", 46.0569, 14.5058),
    ("SK", "Slovakia", "Bratislava", "SK", 48.1486, 17.1077),
    ("UK", "United Kingdom", "London", "GB", 51.5074, -0.1278),
]

LINKS = [
    ("IE", "UK"), ("UK", "NL"), ("UK", "FR"), ("UK", "BE"), ("UK", "PT"),
    ("NL", "BE"), ("NL", "DE"), ("NL", "DK"), ("BE", "LU"), ("BE", "FR"),
    ("LU", "DE"), ("LU", "FR"), ("FR", "CH"), ("FR", "ES"), ("FR", "DE"),
    ("ES", "PT"), ("ES", "IT"), ("CH", "IT"), ("CH", "DE"), ("CH", "AT"),
    ("DE", "DK"), ("DE", "CZ"), ("DE", "AT"), ("DE", "PL"), ("DK", "SE"),
    ("DK", "NO"), ("SE", "NO"), ("SE", "FI"), ("FI", "EE"), ("EE", "LV"),
    ("LV", "LT"), ("LT", "PL"), ("PL", "CZ"), ("CZ", "SK"), ("CZ", "AT"),
    ("SK", "AT"), ("SK", "HU"), ("AT", "HU"), ("AT", "SI"), ("AT", "IT"),
    ("SI", "HR"), ("SI", "IT"), ("HR", "HU"), ("HR", "RS"), ("HU", "RS"),
    ("HU", "RO"), ("RS", "BG"), ("RO", "BG"), ("BG", "GR"), ("IT", "GR"),
    ("GR", "IL"), ("IT", "IL"),
]

# Troncales de 100 Gbps; el resto se asigna 40 o 10 Gbps con la semilla.
CORE_100G = {
    ("UK", "NL"), ("UK", "FR"), ("NL", "DE"), ("FR", "DE"), ("DE", "CH"),
    ("CH", "IT"), ("DE", "AT"), ("AT", "IT"), ("FR", "CH"), ("DE", "CZ"),
}

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

DEMANDS = [
    ("UK", "GR", 60), ("DE", "ES", 45), ("FR", "PL", 30), ("NL", "IT", 70),
    ("SE", "AT", 25), ("IE", "HU", 15), ("PT", "FI", 10), ("IL", "DE", 35),
]

SIGNATURES = {
    "mcode1.txt": "DEADBEEFCAFE",
    "mcode2.txt": "0F1E2D3C4B5A",
    "mcode3.txt": "C0FFEE00BAD5EED",
}


def haversine(a, b):
    lat1, lon1, lat2, lon2 = map(math.radians, (a[4], a[5], b[4], b[5]))
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


def main():
    rng = random.Random(SEED)
    by_id = {node[0]: node for node in NODES}

    write_csv("nodes.csv", ["id", "name", "city", "country", "lat", "lon"], NODES)

    edges, capacities = [], []
    for u, v in LINKS:
        distance = haversine(by_id[u], by_id[v])
        edges.append((u, v, round(distance, 1), round(distance)))
        core = (u, v) in CORE_100G or (v, u) in CORE_100G
        capacities.append((u, v, 100 if core else rng.choice([10, 40, 40])))
    write_csv("edges.csv", ["u", "v", "distance_km", "cost"], edges)
    write_csv("capacities.csv", ["u", "v", "capacity_gbps"], capacities)
    write_csv("demands.csv", ["source", "target", "demand_gbps"], DEMANDS)
    write_csv("countries.csv", ["country", "lat", "lon"], COUNTRIES)

    domain = {"lon_min": -25, "lat_min": 30, "lon_max": 45, "lat_max": 72}
    scenarios = {
        "note": "Escenarios sinteticos de simulacion. No provienen de SNDlib.",
        "scenarios": [
            {
                "id": "baseline",
                "name": "Linea base",
                "description": "Red completa con capacidades de 10, 40 y 100 Gbps.",
                "flow": {"source": "UK", "sink": "GR"},
                "tsp": {"start": "AT", "nodes": "all"},
                "voronoi": {"domain": domain},
                "capacity_overrides": [],
                "removed_links": [],
            },
            {
                "id": "small-tsp",
                "name": "Ruta corta (TSP exacto)",
                "description": "Mantenimiento de 10 centrales de Europa central: Held-Karp exacto.",
                "flow": {"source": "DE", "sink": "HU"},
                "tsp": {"start": "AT", "nodes": ["AT", "DE", "CH", "IT", "SI", "HR", "HU", "SK", "CZ", "PL"]},
                "voronoi": {"domain": domain},
                "capacity_overrides": [],
                "removed_links": [],
            },
            {
                "id": "capacity-upgrade",
                "name": "Mejora de capacidad",
                "description": "Los enlaces hacia Grecia pasan a 100 Gbps.",
                "flow": {"source": "UK", "sink": "GR"},
                "tsp": {"start": "AT", "nodes": "all"},
                "voronoi": {"domain": domain},
                "capacity_overrides": [
                    {"u": "IT", "v": "GR", "capacity_gbps": 100},
                    {"u": "BG", "v": "GR", "capacity_gbps": 100},
                    {"u": "GR", "v": "IL", "capacity_gbps": 100},
                ],
                "removed_links": [],
            },
            {
                "id": "blackout-bg-gr",
                "name": "Blackout BG-GR",
                "description": "Falla el enlace Sofia-Atenas (en el MST y en el corte minimo): recalculo de MST, flujo y corte.",
                "flow": {"source": "UK", "sink": "GR"},
                "tsp": {"start": "AT", "nodes": "all"},
                "voronoi": {"domain": domain},
                "capacity_overrides": [],
                "removed_links": [{"u": "BG", "v": "GR"}],
            },
        ],
    }
    with open(OUT / "scenarios.json", "w", encoding="utf-8") as handle:
        json.dump(scenarios, handle, indent=2, ensure_ascii=False)
        handle.write("\n")

    t1, t2 = make_transmissions(rng)
    (OUT / "transmission1.txt").write_text(wrap(t1), encoding="utf-8")
    (OUT / "transmission2.txt").write_text(wrap(t2), encoding="utf-8")
    for name, signature in SIGNATURES.items():
        (OUT / name).write_text(signature + "\n", encoding="utf-8")
    write_csv("mapping.csv", ["transmission", "link_u", "link_v", "scenario", "label"], [
        ("transmission1.txt", "AT", "CH", "Austria - Suiza", "benign"),
        ("transmission2.txt", "AT", "DE", "Austria - Alemania", "ddos"),
    ])
    print(f"Datos sinteticos escritos en {OUT}")


if __name__ == "__main__":
    main()
