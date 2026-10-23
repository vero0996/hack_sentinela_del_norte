// SENTINEL Digital Twin - interfaz. Todos los resultados vienen del nucleo C++ via /api/run.
"use strict";

const EARTH_RADIUS_KM = 6371;
const $ = (selector) => document.querySelector(selector);
const $$ = (selector) => Array.from(document.querySelectorAll(selector));

const ALGORITHMS = {
  mst: [["kruskal", "Kruskal + DSU"], ["prim", "Prim + heap"]],
  tsp: [["exact", "Held-Karp (exacto)"], ["heuristic", "Vecino cercano + 2-opt"]],
  flow: [["dinic", "Dinic"], ["edmonds-karp", "Edmonds-Karp"]],
  voronoi: [["pruned", "Semiplanos con poda"], ["brute", "Semiplanos sin poda"]],
  matching: [["kmp", "Knuth-Morris-Pratt"], ["z", "Algoritmo Z"]],
  palindrome: [["manacher", "Manacher"], ["expand", "Expansión desde centro"]],
  common: [["automaton", "Autómata de sufijos"], ["dp", "Programación dinámica"]],
};

const LAB_DEFAULTS = {
  mst: { size: 2000, scale: [500, 1000, 2000, 4000, 8000], hint: "Aleatoria: camino + 5n aristas, pesos 1..1000." },
  tsp: { size: 12, scale: [6, 8, 10, 12, 14, 16], hint: "Held-Karp es O(n²·2ⁿ): admite hasta 20 nodos (el escenario completo tiene 30 y la técnica exacta lo rechaza)." },
  flow: { size: 300, scale: [50, 100, 200, 400, 800], hint: "Aleatoria: camino + 4n arcos dirigidos, capacidades 1..10000, de 0 a n-1." },
  voronoi: { size: 300, scale: [50, 100, 200, 400, 800], hint: "Aleatoria: n sitios distintos en [0, 100000]²." },
  matching: { size: 500000, scale: [10000, 50000, 100000, 500000, 1000000], hint: "Aleatoria: dos transmisiones hex de longitud n con firmas insertadas." },
  palindrome: { size: 200000, scale: [10000, 50000, 100000, 200000, 400000], hint: "Aleatoria: hex con un palíndromo de n/2 insertado. Con instancia 'escenario' usa las transmisiones." },
  common: { size: 2000, scale: [500, 1000, 2000, 4000], hint: "Aleatoria: dos cadenas de n con un fragmento común de n/8. La DP es O(n·m)." },
};

const state = {
  scenarios: [],
  scenarioId: null,
  customScenario: null,
  disabled: new Set(),
  data: null,
  transmissions: null,
  activeOverlay: null,
  lastBaseline: null,
};

// ------------------------------------------------------------------ API

async function getJson(path, options) {
  const response = await fetch(path, options);
  const body = await response.json();
  if (!response.ok || body.error) {
    throw new Error(body.error || `HTTP ${response.status}`);
  }
  return body;
}

async function runCore(mode, input, label) {
  const started = performance.now();
  let body;
  try {
    const response = await fetch("/api/run", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ mode, input }),
    });
    body = await response.json();
  } catch (error) {
    logCall(mode, label, null, error.message);
    throw error;
  }
  const result = body.result || {};
  const error = body.error || result.error;
  logCall(mode, label, result, error, performance.now() - started);
  if (error) {
    throw new Error(error);
  }
  return { ...result, wall_ms: body.wall_ms };
}

function logCall(mode, label, result, error, totalMs) {
  const log = $("#call-log");
  const line = document.createElement("div");
  const time = new Date().toLocaleTimeString();
  const algorithm = label ? ` · ${label}` : "";
  if (error) {
    line.className = "err";
    line.textContent = `${time}  ./sentinel  mode=${mode}${algorithm}  ERROR: ${error}`;
  } else {
    line.textContent = `${time}  ./sentinel  mode=${mode}${algorithm}  algoritmo ${fmt(result.elapsed_ms, 3)} ms · total ${fmt(totalMs, 1)} ms`;
  }
  log.prepend(line);
}

function showBanner(message) {
  const banner = $("#banner");
  banner.textContent = message;
  banner.classList.toggle("show", Boolean(message));
}

function fmt(value, digits = 0) {
  if (value === undefined || value === null || Number.isNaN(value)) {
    return "—";
  }
  return Number(value).toLocaleString("es-MX", { minimumFractionDigits: digits, maximumFractionDigits: digits });
}

function escapeHtml(text) {
  return String(text).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
}

// ------------------------------------------------------------------ mapa

const map = L.map("map", { zoomControl: true, attributionControl: true, zoomSnap: 0.25, minZoom: 3, maxZoom: 9 }).setView([50, 14], 4);
map.attributionControl.addAttribution("Red: SNDlib geant · contornos: Natural Earth (dominio público)");

// Mapa base offline: contornos de paises (no requiere internet ni llaves de API).
fetch("vendor/europe-countries.geojson")
  .then((response) => response.json())
  .then((geojson) => {
    L.geoJSON(geojson, {
      interactive: false,
      style: { color: "#2a3a4c", weight: 1, fillColor: "#16212d", fillOpacity: 1 },
    }).addTo(map).bringToBack();
  })
  .catch(() => showBanner("No se pudo cargar el mapa base; la red se muestra sin contornos."));
const baseLayer = L.layerGroup().addTo(map);
const overlayLayer = L.layerGroup().addTo(map);
const highlightLayer = L.layerGroup().addTo(map);
const linkLines = new Map();

function capacityStyle(capacity) {
  if (capacity >= 100) {
    return { color: getCss("--cap100"), weight: 4 };
  }
  if (capacity >= 40) {
    return { color: getCss("--cap40"), weight: 3 };
  }
  return { color: getCss("--cap10"), weight: 2 };
}

function getCss(name) {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}

function nodeLatLng(index) {
  const node = state.data.nodes[index];
  return [node.lat, node.lon];
}

function unproject([x, y]) {
  const lat0 = state.data.voronoi.lat0;
  const lat = (y / EARTH_RADIUS_KM) * (180 / Math.PI);
  const lon = (x / (EARTH_RADIUS_KM * Math.cos((lat0 * Math.PI) / 180))) * (180 / Math.PI);
  return [lat, lon];
}

function projectLatLng(lat, lon) {
  const lat0 = state.data.voronoi.lat0;
  const x = EARTH_RADIUS_KM * ((lon * Math.PI) / 180) * Math.cos((lat0 * Math.PI) / 180);
  const y = EARTH_RADIUS_KM * ((lat * Math.PI) / 180);
  return [x, y];
}

function drawBase() {
  baseLayer.clearLayers();
  linkLines.clear();
  for (const edge of state.data.edges) {
    const line = L.polyline([nodeLatLng(edge.ui), nodeLatLng(edge.vi)], { ...capacityStyle(edge.capacity), opacity: 0.55 })
      .bindTooltip(`${edge.u} – ${edge.v}<br>${fmt(edge.distance_km, 1)} km · ${edge.capacity} Gbps`);
    line.addTo(baseLayer);
    linkLines.set(edge.key, line);
  }
  for (const edge of state.data.removed_edges) {
    L.polyline([nodeLatLng(edge.ui), nodeLatLng(edge.vi)], { color: getCss("--danger"), weight: 2, dashArray: "6 6", opacity: 0.9 })
      .bindTooltip(`${edge.u} – ${edge.v} fuera de servicio`)
      .addTo(baseLayer);
  }
  for (const node of state.data.nodes) {
    L.circleMarker([node.lat, node.lon], { radius: 5, color: "#e3ebf3", weight: 1, fillColor: "#0b1016", fillOpacity: 1 })
      .bindTooltip(`<b>${node.id}</b> · ${escapeHtml(node.city)}, ${escapeHtml(node.name)}`)
      .addTo(baseLayer);
  }
}

function setResult(html) {
  $("#explorer-result").innerHTML = `<h2>Resultado</h2>${html}`;
}

function timingHtml(result) {
  return `<div class="metric"><span>Tiempo del algoritmo (C++)</span><b>${fmt(result.elapsed_ms, 3)} ms</b></div>
    <div class="metric"><span>Tiempo total (proceso + JSON)</span><b>${fmt(result.wall_ms, 1)} ms</b></div>`;
}

function setOverlayButton(name) {
  $$("[data-overlay]").forEach((button) => button.classList.toggle("active", button.dataset.overlay === name));
}

async function showOverlay(name) {
  state.activeOverlay = name;
  setOverlayButton(name);
  overlayLayer.clearLayers();
  highlightLayer.clearLayers();
  setResult(`<p class="muted">Ejecutando ${name} en el núcleo C++…</p>`);
  try {
    if (name === "mst") {
      await overlayMst();
    } else if (name === "tsp") {
      await overlayTsp();
    } else if (name === "flow") {
      await overlayFlow();
    } else if (name === "voronoi") {
      await overlayVoronoi();
    }
  } catch (error) {
    setResult(`<p><span class="badge danger">Error</span> ${escapeHtml(error.message)}</p>`);
  }
}

function mstInput() {
  return {
    nodes: state.data.nodes.map((node) => node.index),
    edges: state.data.edges.map((edge) => ({ u: edge.ui, v: edge.vi, weight: edge.cost })),
  };
}

async function overlayMst() {
  const result = await runCore("mst", mstInput(), "kruskal");
  for (const [u, v] of result.edges) {
    L.polyline([nodeLatLng(u), nodeLatLng(v)], { color: getCss("--ok"), weight: 5, opacity: 0.95 }).addTo(overlayLayer);
  }
  const total = state.data.edges.reduce((sum, edge) => sum + edge.cost, 0);
  setResult(`<div class="big">${fmt(result.cost)} km</div>
    <p class="muted">Kruskal + DSU conecta las ${state.data.nodes.length} centrales con ${result.edges.length} enlaces
    (${fmt((100 * result.cost) / total, 1)} % del costo total de la red).</p>${timingHtml(result)}`);
}

async function overlayTsp() {
  const tsp = state.data.tsp;
  if (!tsp.reachable) {
    throw new Error("Hay centrales inalcanzables con los enlaces activos: no existe ruta de mantenimiento.");
  }
  const result = await runCore("tsp", { distances: tsp.matrix }, "auto");
  const order = result.route.map((position) => tsp.indices[position]);
  for (let i = 0; i + 1 < order.length; ++i) {
    const leg = tsp.legs[`${order[i]}-${order[i + 1]}`] || [order[i], order[i + 1]];
    L.polyline(leg.map(nodeLatLng), { color: getCss("--warn"), weight: 3, opacity: 0.9 }).addTo(overlayLayer);
  }
  order.slice(0, -1).forEach((nodeIndex, step) => {
    L.marker(nodeLatLng(nodeIndex), {
      icon: L.divIcon({ className: "", html: `<div style="background:#ffb547;color:#04121c;border-radius:9px;padding:0 5px;font:600 10px sans-serif">${step + 1}</div>`, iconSize: [20, 14] }),
    }).addTo(overlayLayer);
  });
  const names = order.map((index) => state.data.nodes[index].id).join(" → ");
  setResult(`<div class="big">${fmt(result.cost)} km</div>
    <p><span class="badge ${result.exact ? "ok" : "warn"}">${result.exact ? "Óptimo (Held-Karp)" : "Aproximado (vecino cercano + 2-opt)"}</span>
    ${tsp.ids.length} centrales desde ${escapeHtml(tsp.start)}.</p>
    <p class="muted small mono">${names}</p>
    <p class="muted small">Cada tramo usa el camino mínimo en la red (puede pasar por centrales intermedias).</p>${timingHtml(result)}`);
}

async function overlayFlow() {
  const source = Number($("#flow-source").value);
  const sink = Number($("#flow-sink").value);
  const edges = [];
  for (const edge of state.data.edges) {
    edges.push({ u: edge.ui, v: edge.vi, capacity: edge.capacity });
    edges.push({ u: edge.vi, v: edge.ui, capacity: edge.capacity });
  }
  const result = await runCore("flow", { n: state.data.nodes.length, edges, source, sink }, "dinic");
  const cutLinks = [];
  state.data.edges.forEach((edge, i) => {
    const net = result.edge_flow[2 * i] - result.edge_flow[2 * i + 1];
    const crossesCut = result.min_cut[edge.ui] !== result.min_cut[edge.vi];
    if (crossesCut) {
      cutLinks.push(edge);
    }
    if (net !== 0) {
      const from = net > 0 ? edge.ui : edge.vi;
      const to = net > 0 ? edge.vi : edge.ui;
      L.polyline([nodeLatLng(from), nodeLatLng(to)], {
        color: getCss("--accent-2"), weight: 2 + (6 * Math.abs(net)) / edge.capacity, opacity: 0.95,
      }).bindTooltip(`${state.data.nodes[from].id} → ${state.data.nodes[to].id}: ${Math.abs(net)} / ${edge.capacity} Gbps`).addTo(overlayLayer);
    }
  });
  for (const edge of cutLinks) {
    L.polyline([nodeLatLng(edge.ui), nodeLatLng(edge.vi)], { color: getCss("--danger"), weight: 4, dashArray: "6 7", opacity: 1 })
      .bindTooltip(`Corte mínimo: ${edge.u}–${edge.v} (${edge.capacity} Gbps)`).addTo(overlayLayer);
  }
  const sourceId = state.data.nodes[source].id;
  const sinkId = state.data.nodes[sink].id;
  const demand = state.data.demands.find((d) => d.source === sourceId && d.target === sinkId);
  const demandHtml = demand
    ? `<div class="metric"><span>Demanda SNDlib ${sourceId} → ${sinkId}</span><b>${fmt(demand.demand_gbps, 3)} Gbps
       <span class="badge ${result.flow >= demand.demand_gbps ? "ok" : "danger"}">${result.flow >= demand.demand_gbps ? "cubierta" : "excede la capacidad"}</span></b></div>`
    : `<p class="muted small">No hay demanda registrada para este par (capacidad ≠ demanda).</p>`;
  const cutCapacity = cutLinks.reduce((sum, edge) => sum + edge.capacity, 0);
  setResult(`<div class="big">${fmt(result.flow)} Gbps</div>
    <p class="muted">Flujo máximo ${escapeHtml(state.data.nodes[source].city)} → ${escapeHtml(state.data.nodes[sink].city)} (Dinic). Enlaces bidireccionales: capacidad por sentido.</p>
    ${demandHtml}
    <div class="metric"><span>Corte mínimo (${cutLinks.length} enlaces)</span><b>${cutCapacity} Gbps</b></div>
    <p class="small mono">${cutLinks.map((edge) => `${edge.u}–${edge.v}`).join(", ")}</p>${timingHtml(result)}`);
}

function voronoiInput(queries) {
  return {
    sites: state.data.nodes.map((node) => node.xy),
    bounds: state.data.voronoi.bounds,
    queries,
  };
}

async function overlayVoronoi() {
  const countries = state.data.voronoi.countries;
  const result = await runCore("voronoi", voronoiInput(countries.map((c) => c.xy)), "pruned");
  result.cells.forEach((cell, i) => {
    if (cell.length === 0) {
      return;
    }
    const hue = (i * 137.508) % 360;
    L.polygon(cell.map(unproject), { color: `hsl(${hue} 70% 60%)`, weight: 1, fillOpacity: 0.16 })
      .bindTooltip(`Región de ${state.data.nodes[i].id} (${escapeHtml(state.data.nodes[i].city)})`)
      .addTo(overlayLayer);
  });
  const rows = countries.map((country, i) => {
    const hub = state.data.nodes[result.owners[i]];
    return `<tr><td>${escapeHtml(country.country)}</td><td>${hub.id} · ${escapeHtml(hub.city)}</td></tr>`;
  }).join("");
  const assigned = new Set(result.owners).size;
  const outside = state.data.nodes.filter((node, i) => result.cells[i].length === 0).map((node) => node.id);
  setResult(`<div class="big">${countries.length} países</div>
    <p class="muted">Asignados a ${assigned} centrales por proximidad planar (proyección equirectangular, km), regiones recortadas al dominio.
    ${outside.length ? `Sin región dentro del dominio: ${outside.join(", ")}.` : ""}</p>
    ${timingHtml(result)}
    <div class="scroll"><table><tr><th>País</th><th>Central asignada</th></tr>${rows}</table></div>`);
}

map.on("click", async (event) => {
  if (!state.data) {
    return;
  }
  const point = projectLatLng(event.latlng.lat, event.latlng.lng);
  try {
    const result = await runCore("voronoi", voronoiInput([point]), "consulta");
    const hub = state.data.nodes[result.owners[0]];
    const distance = Math.hypot(point[0] - hub.xy[0], point[1] - hub.xy[1]);
    L.popup().setLatLng(event.latlng)
      .setContent(`<b>${hub.id} · ${escapeHtml(hub.city)}</b><br>Central más cercana (${fmt(distance)} km planos)<br><span class="muted">consulta resuelta en C++</span>`)
      .openOn(map);
  } catch (error) {
    showBanner(error.message);
  }
});

function highlightLink(u, v) {
  highlightLayer.clearLayers();
  const key = [u, v].sort().join("-");
  const edge = state.data.edges.find((e) => e.key === key) || state.data.removed_edges.find((e) => e.key === key);
  if (!edge) {
    showBanner(`El enlace ${u}–${v} no existe en este escenario.`);
    return;
  }
  const line = L.polyline([nodeLatLng(edge.ui), nodeLatLng(edge.vi)], { color: "#ffffff", weight: 9, opacity: 0.9 }).addTo(highlightLayer);
  map.fitBounds(line.getBounds().pad(1.5));
}

// ------------------------------------------------------------------ escenarios

async function loadScenario() {
  const disabled = Array.from(state.disabled);
  if (state.customScenario) {
    state.data = await getJson("/api/scenario/custom", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ scenario: state.customScenario, disabled }),
    });
  } else {
    const query = disabled.length ? `?disabled=${encodeURIComponent(disabled.join(","))}` : "";
    state.data = await getJson(`/api/scenario/${state.scenarioId}${query}`);
  }
  $("#synthetic-note").textContent = state.data.synthetic_note;
  fillNodeSelects();
  const firstLoad = baseLayer.getLayers().length === 0;
  drawBase();
  if (firstLoad) {
    map.fitBounds(L.latLngBounds(state.data.nodes.filter((node) => node.in_domain).map((node) => [node.lat, node.lon])).pad(0.08));
  }
  renderScenarioList();
  renderLinkList();
  if (state.activeOverlay) {
    await showOverlay(state.activeOverlay);
  } else {
    overlayLayer.clearLayers();
  }
}

function fillNodeSelects() {
  const options = state.data.nodes.map((node) => `<option value="${node.index}">${node.id} · ${escapeHtml(node.city)}</option>`).join("");
  $("#flow-source").innerHTML = options;
  $("#flow-sink").innerHTML = options;
  $("#flow-source").value = state.data.flow.source_index;
  $("#flow-sink").value = state.data.flow.sink_index;
}

function renderScenarioSelect() {
  const options = state.scenarios.map((s) => `<option value="${s.id}">${escapeHtml(s.name)}</option>`);
  if (state.customScenario) {
    options.push(`<option value="__custom">Personalizado: ${escapeHtml(state.customScenario.name || "sin nombre")}</option>`);
  }
  $("#scenario-select").innerHTML = options.join("");
  $("#scenario-select").value = state.customScenario ? "__custom" : state.scenarioId;
}

function renderScenarioList() {
  const items = state.scenarios.map((s) => `<div class="scenario-item ${!state.customScenario && s.id === state.scenarioId ? "active" : ""}" data-id="${s.id}">
      <b>${escapeHtml(s.name)}</b><p>${escapeHtml(s.description)}</p></div>`);
  if (state.customScenario) {
    items.push(`<div class="scenario-item active" data-id="__custom"><b>Personalizado: ${escapeHtml(state.customScenario.name || "")}</b>
      <p>${escapeHtml(state.customScenario.description || "Cargado desde archivo.")}</p></div>`);
  }
  $("#scenario-list").innerHTML = items.join("");
  $$("#scenario-list .scenario-item").forEach((item) => item.addEventListener("click", () => selectScenario(item.dataset.id)));
}

function renderLinkList() {
  const all = [...state.data.edges, ...state.data.removed_edges].sort((a, b) => a.key.localeCompare(b.key));
  const html = all.map((edge) => {
    const active = !state.data.removed_edges.some((removed) => removed.key === edge.key);
    const forced = !active && !state.disabled.has(edge.key);
    return `<label title="${forced ? "Eliminado por el escenario" : ""}"><input type="checkbox" data-key="${edge.key}" ${active ? "checked" : ""} ${forced ? "disabled" : ""}>
      ${edge.u}–${edge.v} <span class="muted">${edge.capacity}G</span></label>`;
  }).join("");
  // La misma lista aparece en el Network Explorer y en el Scenario Manager.
  $$(".link-list").forEach((list) => { list.innerHTML = html; });
  $("#links-reset").disabled = state.disabled.size === 0;
  $$(".link-list input").forEach((box) => box.addEventListener("change", async () => {
    if (box.checked) {
      state.disabled.delete(box.dataset.key);
    } else {
      state.disabled.add(box.dataset.key);
    }
    await safely(loadScenario);
  }));
}

$("#links-reset").addEventListener("click", async () => {
  state.disabled.clear();
  await safely(loadScenario);
});

async function selectScenario(id) {
  if (id === "__custom") {
    return;
  }
  state.customScenario = null;
  state.scenarioId = id;
  state.disabled.clear();
  renderScenarioSelect();
  await safely(loadScenario);
}

async function safely(task) {
  try {
    showBanner("");
    await task();
  } catch (error) {
    showBanner(error.message);
  }
}

const SCENARIO_TEMPLATE = {
  id: "mi-escenario",
  name: "Mi escenario",
  description: "Ejemplo: falla del enlace DE-AT y mejora IT-GR.",
  flow: { source: "UK", sink: "GR" },
  tsp: { start: "AT", nodes: ["AT", "DE", "CH", "IT", "FR", "NL", "BE", "LU"] },
  voronoi: { domain: { lon_min: -25, lat_min: 30, lon_max: 45, lat_max: 72 } },
  capacity_overrides: [{ u: "IT", v: "GR", capacity_gbps: 100 }],
  removed_links: [{ u: "DE", v: "AT" }],
};

$("#scenario-template").addEventListener("click", () => {
  const blob = new Blob([JSON.stringify(SCENARIO_TEMPLATE, null, 2)], { type: "application/json" });
  const link = document.createElement("a");
  link.href = URL.createObjectURL(blob);
  link.download = "escenario_plantilla.json";
  link.click();
  URL.revokeObjectURL(link.href);
});

$("#scenario-file").addEventListener("change", async (event) => {
  const file = event.target.files[0];
  if (!file) {
    return;
  }
  await safely(async () => {
    const parsed = JSON.parse(await file.text());
    state.customScenario = parsed.scenarios ? parsed.scenarios[0] : parsed;
    state.disabled.clear();
    renderScenarioSelect();
    await loadScenario();
  });
  event.target.value = "";
});

$("#scenario-select").addEventListener("change", (event) => selectScenario(event.target.value));

$("#scenario-compare").addEventListener("click", async (event) => {
  const button = event.target;
  button.disabled = true;
  await safely(async () => {
    const rows = [];
    for (const scenario of state.scenarios) {
      const data = await getJson(`/api/scenario/${scenario.id}`);
      const edges = data.edges;
      const mst = await runCore("mst", {
        nodes: data.nodes.map((n) => n.index),
        edges: edges.map((e) => ({ u: e.ui, v: e.vi, weight: e.cost })),
      }, scenario.id).catch((e) => ({ error: e.message }));
      const tsp = data.tsp.reachable
        ? await runCore("tsp", { distances: data.tsp.matrix }, scenario.id).catch((e) => ({ error: e.message }))
        : { error: "inalcanzable" };
      const flowEdges = edges.flatMap((e) => [{ u: e.ui, v: e.vi, capacity: e.capacity }, { u: e.vi, v: e.ui, capacity: e.capacity }]);
      const flow = await runCore("flow", { n: data.nodes.length, edges: flowEdges, source: data.flow.source_index, sink: data.flow.sink_index }, scenario.id);
      const cut = edges.filter((e) => flow.min_cut[e.ui] !== flow.min_cut[e.vi]).map((e) => `${e.u}–${e.v}`);
      rows.push(`<tr><td>${escapeHtml(scenario.name)}</td><td class="num">${edges.length}</td>
        <td class="num">${mst.error ? escapeHtml(mst.error) : fmt(mst.cost)}</td>
        <td class="num">${tsp.error ? escapeHtml(tsp.error) : `${fmt(tsp.cost)} ${tsp.exact ? "" : "≈"}`}</td>
        <td class="num">${data.flow.source}→${data.flow.sink}: ${fmt(flow.flow)}</td><td class="small mono">${cut.join(", ")}</td></tr>`);
    }
    $("#scenario-table").innerHTML = `<table><tr><th>Escenario</th><th class="num">Enlaces</th><th class="num">MST (km)</th>
      <th class="num">TSP (km)</th><th class="num">Flujo máx. (Gbps)</th><th>Corte mínimo</th></tr>${rows.join("")}</table>
      <p class="muted small">≈ ruta aproximada (más de 15 centrales). Todos los valores calculados por el núcleo C++.</p>`;
  });
  button.disabled = false;
});

// ------------------------------------------------------------------ laboratorio

function mulberry32(seed) {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function randomHex(random, length) {
  const digits = "0123456789ABCDEF";
  let text = "";
  for (let i = 0; i < length; ++i) {
    text += digits[Math.floor(random() * 16)];
  }
  return text;
}

function randomInstance(mode, n, seed) {
  const random = mulberry32(seed);
  const randint = (lo, hi) => lo + Math.floor(random() * (hi - lo + 1));
  if (mode === "mst") {
    const edges = [];
    for (let i = 0; i + 1 < n; ++i) {
      edges.push({ u: i, v: i + 1, weight: randint(1, 1000) });
    }
    for (let k = 0; k < 5 * n; ++k) {
      const u = randint(0, n - 1);
      const v = randint(0, n - 1);
      if (u !== v) {
        edges.push({ u, v, weight: randint(1, 1000) });
      }
    }
    return { nodes: Array.from({ length: n }, (_, i) => i), edges };
  }
  if (mode === "tsp") {
    const points = Array.from({ length: n }, () => [randint(0, 1000), randint(0, 1000)]);
    return { distances: points.map((a) => points.map((b) => Math.round(Math.hypot(a[0] - b[0], a[1] - b[1])))) };
  }
  if (mode === "flow") {
    const edges = [];
    const seen = new Set();
    for (let i = 0; i + 1 < n; ++i) {
      edges.push({ u: i, v: i + 1, capacity: randint(100, 10000) });
      seen.add(`${i}-${i + 1}`);
    }
    for (let k = 0; k < 4 * n; ++k) {
      const u = randint(0, n - 1);
      const v = randint(0, n - 1);
      if (u !== v && !seen.has(`${u}-${v}`)) {
        seen.add(`${u}-${v}`);
        edges.push({ u, v, capacity: randint(1, 10000) });
      }
    }
    return { n, edges, source: 0, sink: n - 1 };
  }
  if (mode === "voronoi") {
    const seen = new Set();
    const sites = [];
    while (sites.length < n) {
      const point = [randint(1, 99999), randint(1, 99999)];
      if (!seen.has(point.join(","))) {
        seen.add(point.join(","));
        sites.push(point);
      }
    }
    return { sites, bounds: [0, 0, 100000, 100000] };
  }
  if (mode === "matching") {
    const signatures = ["DEADBEEFCAFEBABE", "0123456789ABCDEF0123", "FACEFACEFACE"];
    const a = randomHex(random, n);
    const b = randomHex(random, n);
    return {
      transmissions: [a.slice(0, n / 3) + signatures[0] + a.slice(n / 3), b.slice(0, (2 * n) / 3) + signatures[1] + b.slice((2 * n) / 3)],
      patterns: signatures,
    };
  }
  if (mode === "palindrome") {
    const half = randomHex(random, Math.floor(n / 4));
    const palindrome = half + "A" + [...half].reverse().join("");
    const side = randomHex(random, Math.floor(n / 4));
    return { transmissions: [side + palindrome + side, randomHex(random, n)] };
  }
  const shared = randomHex(random, Math.max(4, Math.floor(n / 8)));
  const half = Math.floor(n / 2);
  return {
    transmissions: [randomHex(random, half) + shared + randomHex(random, half), randomHex(random, half) + shared + randomHex(random, half)],
  };
}

function scenarioInstance(mode) {
  if (mode === "mst") {
    return mstInput();
  }
  if (mode === "tsp") {
    return { distances: state.data.tsp.matrix };
  }
  if (mode === "flow") {
    const edges = state.data.edges.flatMap((e) => [{ u: e.ui, v: e.vi, capacity: e.capacity }, { u: e.vi, v: e.ui, capacity: e.capacity }]);
    return { n: state.data.nodes.length, edges, source: state.data.flow.source_index, sink: state.data.flow.sink_index };
  }
  if (mode === "voronoi") {
    return voronoiInput(state.data.voronoi.countries.map((c) => c.xy));
  }
  const { transmissions, codes } = state.transmissions;
  if (mode === "matching") {
    return { transmissions, patterns: codes };
  }
  return { transmissions };
}

function polygonArea(points) {
  let twice = 0;
  points.forEach((p, i) => {
    const q = points[(i + 1) % points.length];
    twice += p[0] * q[1] - q[0] * p[1];
  });
  return Math.abs(twice) / 2;
}

function summarize(mode, result) {
  if (mode === "mst") {
    return [["Costo total", fmt(result.cost)], ["Aristas", result.edges.length]];
  }
  if (mode === "tsp") {
    return [["Costo de la ruta", fmt(result.cost)], ["Tipo", result.exact ? "óptimo" : "aproximado"], ["Ruta (inicio)", result.route.slice(0, 10).join(" → ") + (result.route.length > 10 ? " …" : "")]];
  }
  if (mode === "flow") {
    return [["Flujo máximo", fmt(result.flow)], ["Nodos del lado origen", result.min_cut.filter((x) => x === 1).length]];
  }
  if (mode === "voronoi") {
    const areas = result.cells.map(polygonArea);
    return [["Celdas", result.cells.length], ["Área total", fmt(areas.reduce((a, b) => a + b, 0))], ["Vértices", result.cells.reduce((s, c) => s + c.length, 0)]];
  }
  if (mode === "matching") {
    return [["Posiciones", JSON.stringify(result.matches)]];
  }
  if (mode === "palindrome") {
    return result.ranges.map((r, i) => [`T${i + 1}`, `${r[0]}–${r[1]} (long. ${r[1] ? r[1] - r[0] + 1 : 0})`]);
  }
  const r = result.range;
  return [["Rango en T1", `${r[0]}–${r[1]}`], ["Longitud", r[1] ? r[1] - r[0] + 1 : 0]];
}

function agreement(mode, a, b) {
  if (mode === "tsp") {
    const gap = (100 * (b.cost - a.cost)) / Math.max(1, a.cost);
    return { ok: a.exact ? b.cost >= a.cost : true, text: `Diferencia de costo B vs A: ${fmt(gap, 2)} % ${a.exact ? "(A es óptimo: B nunca puede ser menor)" : ""}` };
  }
  if (mode === "voronoi") {
    const worst = Math.max(...a.cells.map((cell, i) => Math.abs(polygonArea(cell) - polygonArea(b.cells[i])) / Math.max(1, polygonArea(b.cells[i]))));
    return { ok: worst < 1e-9, text: `Máxima diferencia relativa de área por celda: ${worst.toExponential(2)}` };
  }
  const [key, label] = {
    mst: ["cost", "costo total"], flow: ["flow", "flujo máximo"], matching: ["matches", "conjunto de posiciones"],
    palindrome: ["ranges", "conjunto de rangos"], common: ["range", "rango"],
  }[mode];
  const same = JSON.stringify(a[key]) === JSON.stringify(b[key]);
  return { ok: same, text: same ? `Ambas técnicas producen el mismo ${label}.` : `Resultados distintos en ${label}.` };
}

function renderLabCard(selector, title, result) {
  const rows = summarize($("#lab-mode").value, result).map(([k, v]) => `<div class="metric"><span>${k}</span><b class="mono">${escapeHtml(v)}</b></div>`).join("");
  $(selector).innerHTML = `<h2>${escapeHtml(title)}</h2>${rows}${timingHtml(result)}`;
}

function labInstance() {
  const mode = $("#lab-mode").value;
  if ($("#lab-source").value === "scenario") {
    return scenarioInstance(mode);
  }
  return randomInstance(mode, Number($("#lab-size").value), Number($("#lab-seed").value));
}

function updateLabForm() {
  const mode = $("#lab-mode").value;
  const options = ALGORITHMS[mode].map(([value, label]) => `<option value="${value}">${label}</option>`).join("");
  $("#lab-algo-a").innerHTML = options;
  $("#lab-algo-b").innerHTML = options;
  $("#lab-algo-b").selectedIndex = 1;
  $("#lab-size").value = LAB_DEFAULTS[mode].size;
  $("#lab-hint").textContent = LAB_DEFAULTS[mode].hint;
}

$("#lab-mode").addEventListener("change", updateLabForm);

$("#lab-run").addEventListener("click", async (event) => {
  const button = event.target;
  button.disabled = true;
  const mode = $("#lab-mode").value;
  const algoA = $("#lab-algo-a").value;
  const algoB = $("#lab-algo-b").value;
  const labelA = $("#lab-algo-a").selectedOptions[0].textContent;
  const labelB = $("#lab-algo-b").selectedOptions[0].textContent;
  const instance = labInstance();
  const results = {};
  for (const [slot, algorithm, label] of [["a", algoA, labelA], ["b", algoB, labelB]]) {
    try {
      results[slot] = await runCore(mode, { ...instance, algorithm }, algorithm);
      renderLabCard(`#lab-${slot}`, label, results[slot]);
    } catch (error) {
      $(`#lab-${slot}`).innerHTML = `<h2>${escapeHtml(label)}</h2><p><span class="badge danger">Error</span> ${escapeHtml(error.message)}</p>`;
    }
  }
  if (results.a && results.b) {
    const check = agreement(mode, results.a, results.b);
    const ratio = results.b.elapsed_ms / Math.max(results.a.elapsed_ms, 1e-6);
    $("#lab-compare").innerHTML = `<h2>Comparación</h2>
      <p><span class="badge ${check.ok ? "ok" : "danger"}">${check.ok ? "Consistente" : "Revisar"}</span> ${escapeHtml(check.text)}</p>
      <div class="metric"><span>Tiempo B / tiempo A</span><b>${fmt(ratio, 2)}×</b></div>`;
  } else {
    $("#lab-compare").innerHTML = `<h2>Comparación</h2><p class="muted">Una de las técnicas no aceptó la instancia (ver su tarjeta).</p>`;
  }
  button.disabled = false;
});

$("#lab-scale").addEventListener("click", async (event) => {
  const button = event.target;
  button.disabled = true;
  const mode = $("#lab-mode").value;
  const sizes = LAB_DEFAULTS[mode].scale;
  const series = [$("#lab-algo-a").value, $("#lab-algo-b").value].map((algorithm) => ({ algorithm, points: [] }));
  const seed = Number($("#lab-seed").value);
  $("#lab-chart").innerHTML = `<h2>Escalabilidad</h2><p class="muted">Ejecutando ${sizes.length * 2} corridas en C++…</p>`;
  for (const n of sizes) {
    const instance = randomInstance(mode, n, seed);
    for (const line of series) {
      try {
        const result = await runCore(mode, { ...instance, algorithm: line.algorithm }, `${line.algorithm} n=${n}`);
        line.points.push([n, result.elapsed_ms]);
      } catch (error) {
        line.points.push([n, null]);
      }
    }
  }
  renderChart(mode, series);
  button.disabled = false;
});

function renderChart(mode, series) {
  const width = 760;
  const height = 280;
  const pad = { left: 64, right: 20, top: 16, bottom: 36 };
  const all = series.flatMap((s) => s.points.filter((p) => p[1] !== null));
  const maxX = Math.max(...all.map((p) => p[0]));
  const maxY = Math.max(...all.map((p) => p[1]), 1e-3);
  const sx = (x) => pad.left + (x / maxX) * (width - pad.left - pad.right);
  const sy = (y) => height - pad.bottom - (y / maxY) * (height - pad.top - pad.bottom);
  const colors = [getCss("--accent"), getCss("--warn")];
  const ticks = [0, 0.25, 0.5, 0.75, 1].map((t) => `<line x1="${pad.left}" x2="${width - pad.right}" y1="${sy(t * maxY)}" y2="${sy(t * maxY)}" stroke="#243242"/>
    <text x="${pad.left - 6}" y="${sy(t * maxY) + 4}" text-anchor="end">${fmt(t * maxY, maxY < 10 ? 2 : 0)}</text>`).join("");
  const xTicks = series[0].points.map(([x]) => `<text x="${sx(x)}" y="${height - 12}" text-anchor="middle">${fmt(x)}</text>`).join("");
  const lines = series.map((s, i) => {
    const points = s.points.filter((p) => p[1] !== null);
    const path = points.map((p, k) => `${k ? "L" : "M"}${sx(p[0])},${sy(p[1])}`).join(" ");
    const dots = points.map((p) => `<circle cx="${sx(p[0])}" cy="${sy(p[1])}" r="3.5" fill="${colors[i]}"><title>${s.algorithm} n=${p[0]}: ${fmt(p[1], 3)} ms</title></circle>`).join("");
    return `<path d="${path}" fill="none" stroke="${colors[i]}" stroke-width="2"/>${dots}`;
  }).join("");
  const legend = series.map((s, i) => {
    const label = ALGORITHMS[mode].find(([value]) => value === s.algorithm)[1];
    const failed = s.points.filter((p) => p[1] === null).map((p) => p[0]);
    return `<span style="color:${colors[i]}">● ${label}</span>${failed.length ? ` <span class="muted small">(rechazó n = ${failed.join(", ")})</span>` : ""}`;
  }).join(" &nbsp; ");
  $("#lab-chart").innerHTML = `<h2>Escalabilidad · ms del algoritmo vs n</h2><div class="chart"><svg viewBox="0 0 ${width} ${height}">${ticks}${xTicks}${lines}</svg></div><p class="small">${legend}</p>`;
}

// ------------------------------------------------------------------ ciberseguridad

function renderHex(container, text, marks) {
  const flags = new Uint8Array(text.length);
  for (const [start, end, bit] of marks) {
    for (let i = Math.max(0, start); i < Math.min(text.length, end); ++i) {
      flags[i] |= bit;
    }
  }
  let html = "";
  let i = 0;
  while (i < text.length) {
    let j = i;
    while (j < text.length && flags[j] === flags[i]) {
      ++j;
    }
    const chunk = text.slice(i, j);
    const classes = [flags[i] & 1 ? "m" : "", flags[i] & 2 ? "p" : "", flags[i] & 4 ? "c" : ""].filter(Boolean).join(" ");
    html += classes ? `<mark class="${classes}" title="posiciones ${i + 1}–${j}">${chunk}</mark>` : chunk;
    i = j;
  }
  container.innerHTML = html;
}

async function runCyber() {
  const { transmissions } = state.transmissions;
  const patterns = [1, 2, 3].map((k) => $(`#sig-${k}`).value.replace(/\s/g, "").toUpperCase());
  const matching = await runCore("matching", { transmissions, patterns }, "kmp");
  const palindrome = await runCore("palindrome", { transmissions }, "manacher");
  const common = await runCore("common", { transmissions }, "automaton");

  const lines = [];
  matching.matches.forEach((row) => row.forEach((position) => lines.push(position > 0 ? `true ${position}` : "false")));
  palindrome.ranges.forEach((r) => lines.push(`${r[0]} ${r[1]}`));
  lines.push(`${common.range[0]} ${common.range[1]}`);
  const elapsed = matching.elapsed_ms + palindrome.elapsed_ms + common.elapsed_ms;
  $("#cyber-output").innerHTML = `<h2>Salida (formato de la especificación)</h2><pre class="mono">${lines.join("\n")}</pre>
    <p class="muted small">KMP · Manacher · autómata de sufijos — ${fmt(elapsed, 3)} ms en C++.</p>`;

  const [c0, c1] = common.range;
  const shared = c1 ? transmissions[0].slice(c0 - 1, c1) : "";
  transmissions.forEach((text, t) => {
    const marks = [];
    matching.matches[t].forEach((position, p) => {
      if (position > 0) {
        marks.push([position - 1, position - 1 + patterns[p].length, 1]);
      }
    });
    const [p0, p1] = palindrome.ranges[t];
    if (p1) {
      marks.push([p0 - 1, p1, 2]);
    }
    if (shared) {
      const start = t === 0 ? c0 - 1 : text.indexOf(shared);
      marks.push([start, start + shared.length, 4]);
    }
    renderHex($(`#cyber-t${t + 1} .hex`), text, marks);
    const found = matching.matches[t].map((pos, p) => `mcode${p + 1}: ${pos > 0 ? `pos ${pos}` : "no"}`).join(" · ");
    $(`#cyber-t${t + 1} h2`).textContent = `transmission${t + 1}.txt · ${text.length} caracteres · ${found}`;
  });
  renderMapping(matching.matches);
}

function renderMapping(matches) {
  const { mapping, note } = state.transmissions;
  const rows = mapping.map((row, t) => {
    const ddos = /ddos/i.test(row.scenario);
    let status = escapeHtml(row.status);
    if (matches && matches[t]) {
      const found = matches[t].filter((position) => position > 0).length;
      status = `${found} de ${matches[t].length} firmas detectadas`;
    }
    const button = row.link_u
      ? `<button class="ghost" data-link="${escapeHtml(row.link_u)},${escapeHtml(row.link_v)}">Ver en mapa</button>`
      : `<span class="muted small">enlace no encontrado</span>`;
    return `<tr><td class="mono">${escapeHtml(row.transmission)}</td><td>${escapeHtml(row.link)}</td>
      <td><span class="badge ${ddos ? "danger" : "ok"}">${escapeHtml(row.scenario)}</span></td><td>${status}</td><td>${button}</td></tr>`;
  }).join("");
  $("#cyber-mapping").innerHTML = `<h2>Correspondencia de simulación (mapping.csv)</h2>
    <table><tr><th>Transmisión</th><th>Enlace</th><th>Escenario</th><th>Estado</th><th></th></tr>${rows}</table>
    <p class="muted small">${escapeHtml(note)} Las etiquetas dan contexto; no sustituyen los resultados.</p>`;
  $$("#cyber-mapping [data-link]").forEach((button) => button.addEventListener("click", () => {
    const [u, v] = button.dataset.link.split(",");
    activateTab("explorer");
    map.invalidateSize();
    highlightLink(u, v);
  }));
}

function resetSignatures() {
  state.transmissions.codes.forEach((code, i) => {
    $(`#sig-${i + 1}`).value = code;
  });
}

$("#cyber-run").addEventListener("click", () => safely(runCyber));
$("#cyber-reset").addEventListener("click", () => {
  resetSignatures();
  safely(runCyber);
});

// ------------------------------------------------------------------ pestañas e inicio

function activateTab(name) {
  $$(".tab").forEach((tab) => tab.classList.toggle("active", tab.dataset.tab === name));
  $$(".panel").forEach((panel) => panel.classList.toggle("active", panel.id === `tab-${name}`));
  if (name === "explorer") {
    setTimeout(() => map.invalidateSize(), 0);
  }
}

$$(".tab").forEach((tab) => tab.addEventListener("click", () => activateTab(tab.dataset.tab)));
$$("[data-overlay]").forEach((button) => button.addEventListener("click", () => showOverlay(button.dataset.overlay)));
$("#clear-overlays").addEventListener("click", () => {
  state.activeOverlay = null;
  setOverlayButton(null);
  overlayLayer.clearLayers();
  highlightLayer.clearLayers();
  setResult(`<p class="muted">Capas limpias.</p>`);
});
$("#flow-source").addEventListener("change", () => state.activeOverlay === "flow" && showOverlay("flow"));
$("#flow-sink").addEventListener("change", () => state.activeOverlay === "flow" && showOverlay("flow"));

async function init() {
  await safely(async () => {
    const list = await getJson("/api/scenarios");
    state.scenarios = list.scenarios;
    state.scenarioId = state.scenarios[0].id;
    renderScenarioSelect();
    state.transmissions = await getJson("/api/transmissions");
    resetSignatures();
    renderMapping();
    updateLabForm();
    // Enlaces directos para la demo: #tab=cyber, #scenario=blackout-gr-it&overlay=flow
    const params = new URLSearchParams(location.hash.slice(1));
    if (params.get("scenario") && state.scenarios.some((s) => s.id === params.get("scenario"))) {
      state.scenarioId = params.get("scenario");
      renderScenarioSelect();
    }
    if (params.get("overlay")) {
      state.activeOverlay = params.get("overlay");
      setOverlayButton(state.activeOverlay);
    }
    await loadScenario();
    await runCyber();
    if (params.get("tab")) {
      activateTab(params.get("tab"));
    }
    if (params.get("labmode") && ALGORITHMS[params.get("labmode")]) {
      $("#lab-mode").value = params.get("labmode");
      updateLabForm();
    }
    if (params.get("lab") === "run") {
      $("#lab-run").click();
    }
    if (params.get("lab") === "scale") {
      $("#lab-scale").click();
    }
    if (params.get("compare") === "run") {
      $("#scenario-compare").click();
    }
  });
}

init();
