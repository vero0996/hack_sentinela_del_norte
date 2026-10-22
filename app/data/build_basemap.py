"""Construye el mapa base offline (contornos de paises de Europa) para el Digital Twin.

Fuente: world-atlas@2.0.2 countries-50m.json (TopoJSON, licencia ISC), derivado de
Natural Earth (dominio publico). Solo se usa como fondo visual.

Uso:
    curl -L https://cdn.jsdelivr.net/npm/world-atlas@2.0.2/countries-50m.json -o countries-50m.json
    python3 app/data/build_basemap.py countries-50m.json
"""
import json
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "static" / "vendor" / "europe-countries.geojson"
LON_MIN, LAT_MIN, LON_MAX, LAT_MAX = -32, 25, 50, 75


def decode_arcs(topology):
    scale = topology["transform"]["scale"]
    translate = topology["transform"]["translate"]
    arcs = []
    for arc in topology["arcs"]:
        x = y = 0
        points = []
        for dx, dy in arc:
            x += dx
            y += dy
            points.append([round(x * scale[0] + translate[0], 3), round(y * scale[1] + translate[1], 3)])
        arcs.append(points)
    return arcs


def ring(indices, arcs):
    points = []
    for index in indices:
        arc = arcs[index] if index >= 0 else list(reversed(arcs[~index]))
        points.extend(arc if not points else arc[1:])
    return points


def in_europe(rings):
    return any(LON_MIN <= lon <= LON_MAX and LAT_MIN <= lat <= LAT_MAX for r in rings for lon, lat in r)


def main():
    topology = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    arcs = decode_arcs(topology)
    features = []
    for geometry in topology["objects"]["countries"]["geometries"]:
        if geometry["type"] == "Polygon":
            polygons = [[ring(r, arcs) for r in geometry["arcs"]]]
        elif geometry["type"] == "MultiPolygon":
            polygons = [[ring(r, arcs) for r in polygon] for polygon in geometry["arcs"]]
        else:
            continue
        polygons = [polygon for polygon in polygons if in_europe(polygon)]
        if polygons:
            features.append({
                "type": "Feature",
                "properties": {"name": geometry.get("properties", {}).get("name", "")},
                "geometry": {"type": "MultiPolygon", "coordinates": polygons},
            })
    collection = {
        "type": "FeatureCollection",
        "attribution": "Natural Earth (dominio publico) via world-atlas@2.0.2 (ISC)",
        "features": features,
    }
    OUT.write_text(json.dumps(collection, separators=(",", ":")), encoding="utf-8")
    print(f"{len(features)} paises escritos en {OUT} ({OUT.stat().st_size // 1024} KB)")


if __name__ == "__main__":
    main()
