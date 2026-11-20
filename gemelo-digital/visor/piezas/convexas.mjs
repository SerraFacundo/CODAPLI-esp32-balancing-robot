// Partición de un polígono con agujeros en polígonos convexos, para dibujarlo con el kit
// (que solo acepta piezas convexas). Triangulación por orejas con puentes a los agujeros y
// después unión de triángulos vecinos mientras la unión siga convexa (Hertel-Mehlhorn).
//   node piezas/convexas.mjs   -> autoprueba
import { fileURLToPath } from "node:url";

const cross = (o, a, b) => (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
export const areaOf = (poly) => {
  let a = 0;
  for (let i = 0; i < poly.length; i++) {
    const p = poly[i];
    const q = poly[(i + 1) % poly.length];
    a += p[0] * q[1] - q[0] * p[1];
  }
  return a / 2;
};
const ccw = (poly) => (areaOf(poly) >= 0 ? poly.slice() : poly.slice().reverse());
const cw = (poly) => (areaOf(poly) <= 0 ? poly.slice() : poly.slice().reverse());

// Une cada agujero al contorno con un puente (doble arista): el segmento más corto entre un
// vértice del agujero y uno del anillo actual que no corte ninguna arista y entre por el
// ángulo interior del vértice del anillo.
const TAU = 2 * Math.PI;
const angle = (d) => (Math.atan2(d[1], d[0]) + TAU) % TAU;
function crosses(p, q, a, b) {
  const d1 = cross(p, q, a);
  const d2 = cross(p, q, b);
  const d3 = cross(a, b, p);
  const d4 = cross(a, b, q);
  return ((d1 > 1e-12 && d2 < -1e-12) || (d1 < -1e-12 && d2 > 1e-12)) && ((d3 > 1e-12 && d4 < -1e-12) || (d3 < -1e-12 && d4 > 1e-12));
}
const same = (p, q) => p[0] === q[0] && p[1] === q[1];
function bridgeHoles(outer, holes) {
  let ring = ccw(outer);
  const pending = holes.map(cw).sort((a, b) => Math.max(...b.map((p) => p[0])) - Math.max(...a.map((p) => p[0])));
  while (pending.length) {
    const hole = pending.shift();
    const edges = [];
    for (const loop of [ring, hole, ...pending]) for (let i = 0; i < loop.length; i++) edges.push([loop[i], loop[(i + 1) % loop.length]]);
    let best = null;
    for (let hi = 0; hi < hole.length; hi++) {
      const m = hole[hi];
      for (let ri = 0; ri < ring.length; ri++) {
        const v = ring[ri];
        const len = Math.hypot(m[0] - v[0], m[1] - v[1]);
        if (best && len >= best.len) continue;
        const prev = ring[(ri + ring.length - 1) % ring.length];
        const next = ring[(ri + 1) % ring.length];
        const toNext = angle([next[0] - v[0], next[1] - v[1]]);
        const sweep = (angle([prev[0] - v[0], prev[1] - v[1]]) - toNext + TAU) % TAU;
        const dir = (angle([m[0] - v[0], m[1] - v[1]]) - toNext + TAU) % TAU;
        if (!(dir > 1e-9 && dir < sweep - 1e-9)) continue;
        if (edges.some(([a, b]) => !same(a, v) && !same(b, v) && !same(a, m) && !same(b, m) && crosses(v, m, a, b))) continue;
        best = { len, hi, ri };
      }
    }
    if (!best) throw new Error("convexas: no se pudo unir un agujero");
    const rotated = [...hole.slice(best.hi), ...hole.slice(0, best.hi), hole[best.hi]];
    ring = [...ring.slice(0, best.ri + 1), ...rotated, ring[best.ri], ...ring.slice(best.ri + 1)];
  }
  return ring;
}

function triangulate(ring) {
  const idx = ring.map((_, i) => i);
  const tris = [];
  const inTri = (p, a, b, c) => cross(a, b, p) >= -1e-12 && cross(b, c, p) >= -1e-12 && cross(c, a, p) >= -1e-12;
  // Corta siempre la oreja más "gorda" (mayor ángulo mínimo): evita abanicos de triángulos
  // finos, que después se unen peor y le complican el orden de dibujo al kit
  const fatness = (a, b, c) => {
    const ang = (p, q, r) => {
      const u = [q[0] - p[0], q[1] - p[1]];
      const v = [r[0] - p[0], r[1] - p[1]];
      return Math.acos(Math.max(-1, Math.min(1, (u[0] * v[0] + u[1] * v[1]) / (Math.hypot(...u) * Math.hypot(...v) || 1))));
    };
    return Math.min(ang(a, b, c), ang(b, c, a), ang(c, a, b));
  };
  let guard = 0;
  while (idx.length > 3 && guard++ < 100000) {
    let clipped = false;
    let best = -1;
    let bestFat = -1;
    for (let k = 0; k < idx.length; k++) {
      const ia = idx[(k + idx.length - 1) % idx.length];
      const ib = idx[k];
      const ic = idx[(k + 1) % idx.length];
      const a = ring[ia];
      const b = ring[ib];
      const c = ring[ic];
      if (cross(a, b, c) <= 1e-12) continue;
      let ear = true;
      for (const j of idx) {
        if (j === ia || j === ib || j === ic) continue;
        const p = ring[j];
        // Los vértices duplicados por los puentes cuentan solo si no coinciden con a, b o c
        if ((p[0] === a[0] && p[1] === a[1]) || (p[0] === b[0] && p[1] === b[1]) || (p[0] === c[0] && p[1] === c[1])) continue;
        if (inTri(p, a, b, c)) {
          ear = false;
          break;
        }
      }
      if (!ear) continue;
      const fat = fatness(a, b, c);
      if (fat > bestFat) [best, bestFat] = [k, fat];
    }
    if (best >= 0) {
      const k = best;
      tris.push([ring[idx[(k + idx.length - 1) % idx.length]], ring[idx[k]], ring[idx[(k + 1) % idx.length]]]);
      idx.splice(k, 1);
      clipped = true;
    }
    if (!clipped) throw new Error(`convexas: no hay oreja con ${idx.length} vértices`);
  }
  if (idx.length === 3) tris.push(idx.map((i) => ring[i]));
  return tris.filter((t) => Math.abs(areaOf(t)) > 1e-9);
}

const keyOf = (p) => `${Math.round(p[0] * 1e6)},${Math.round(p[1] * 1e6)}`;
const isConvex = (poly) => {
  for (let i = 0; i < poly.length; i++) if (cross(poly[i], poly[(i + 1) % poly.length], poly[(i + 2) % poly.length]) < -1e-9) return false;
  return true;
};
// Quita vértices colineales
const tidy = (poly) => poly.filter((p, i) => Math.abs(cross(poly[(i + poly.length - 1) % poly.length], p, poly[(i + 1) % poly.length])) > 1e-9);

function mergeConvex(tris) {
  let polys = tris.map(ccw);
  let merged = true;
  while (merged) {
    merged = false;
    const edges = new Map();
    polys.forEach((poly, pi) =>
      poly.forEach((p, i) => {
        const q = poly[(i + 1) % poly.length];
        edges.set(`${keyOf(p)}|${keyOf(q)}`, [pi, i]);
      }),
    );
    // Primero las uniones que dejan las piezas más grandes
    const candidates = [];
    polys.forEach((poly, pi) =>
      poly.forEach((p, i) => {
        const q = poly[(i + 1) % poly.length];
        const other = edges.get(`${keyOf(q)}|${keyOf(p)}`);
        if (other && other[0] > pi) candidates.push([Math.abs(areaOf(poly)) + Math.abs(areaOf(polys[other[0]])), pi, i, ...other]);
      }),
    );
    candidates.sort((a, b) => b[0] - a[0]);
    const dead = new Set();
    const touched = new Set();
    for (const [, pa, ia, pb, ib] of candidates) {
      // Una pieza ya unida en esta pasada cambió sus índices: espera a la pasada siguiente
      if (touched.has(pa) || touched.has(pb)) continue;
      const A = polys[pa];
      const B = polys[pb];
      // A: ... A[ia] -> A[ia+1] ...; B tiene la arista al revés
      const joined = [];
      for (let k = 1; k <= A.length; k++) joined.push(A[(ia + k) % A.length]);
      for (let k = 2; k < B.length; k++) joined.push(B[(ib + k) % B.length]);
      const poly = tidy(joined);
      if (!isConvex(poly)) continue;
      polys[pa] = poly;
      dead.add(pb);
      touched.add(pa).add(pb);
      merged = true;
    }
    polys = polys.filter((_, i) => !dead.has(i));
  }
  return polys.map(tidy);
}

export const _bridge = (o, h) => bridgeHoles(o, h);
export const _tri = (r) => triangulate(r);
// Devuelve piezas convexas antihorarias, y para cada una qué aristas son internas (compartidas)
export function convexPieces(outer, holes = []) {
  // La triangulación por orejas depende del vértice de arranque: se prueban varios y se queda
  // con la partición de menos piezas
  const ring = bridgeHoles(outer, holes);
  let pieces = null;
  for (let start = 0; start < Math.min(ring.length, 4); start++) {
    const shift = Math.floor((start * ring.length) / Math.min(ring.length, 4));
    let found;
    try {
      found = mergeConvex(triangulate([...ring.slice(shift), ...ring.slice(0, shift)]));
    } catch {
      continue;
    }
    if (!pieces || found.length < pieces.length) pieces = found;
  }
  if (!pieces) throw new Error("convexas: no se pudo triangular");
  const count = new Map();
  for (const poly of pieces) poly.forEach((p, i) => {
    const q = poly[(i + 1) % poly.length];
    const k = [keyOf(p), keyOf(q)].sort().join("|");
    count.set(k, (count.get(k) ?? 0) + 1);
  });
  return pieces.map((polygon) => ({
    polygon,
    internal: polygon.map((p, i) => count.get([keyOf(p), keyOf(polygon[(i + 1) % polygon.length])].sort().join("|")) > 1),
  }));
}

// Autoprueba: el área total coincide y todas las piezas son convexas
if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const circle = (cx, cy, r, n) => Array.from({ length: n }, (_, i) => [cx + r * Math.cos((i / n) * 2 * Math.PI), cy + r * Math.sin((i / n) * 2 * Math.PI)]);
  const cases = [
    { name: "cuadrado", outer: [[0, 0], [10, 0], [10, 10], [0, 10]], holes: [] },
    { name: "L", outer: [[0, 0], [10, 0], [10, 4], [4, 4], [4, 10], [0, 10]], holes: [] },
    { name: "aro", outer: circle(0, 0, 44, 48), holes: [circle(0, 0, 5, 12)] },
    { name: "rueda", outer: circle(0, 0, 44, 48), holes: [circle(0, 0, 5, 10), circle(0, 26, 15, 14), circle(22.5, -13, 15, 14), circle(-22.5, -13, 15, 14)] },
    { name: "placa", outer: [[-38, 0], [38, 0], [38, 88], [-38, 88]], holes: [circle(-14, 46, 9, 12), circle(14, 46, 9, 12), circle(0, 70, 11, 14), circle(-10, 20, 6, 10), circle(10, 20, 6, 10)] },
  ];
  for (const c of cases) {
    const pieces = convexPieces(c.outer, c.holes);
    const want = Math.abs(areaOf(c.outer)) - c.holes.reduce((s, h) => s + Math.abs(areaOf(h)), 0);
    const got = pieces.reduce((s, p) => s + areaOf(p.polygon), 0);
    const convex = pieces.every((p) => isConvex(p.polygon) && areaOf(p.polygon) > 0);
    console.assert(Math.abs(want - got) < 1e-6 * Math.max(1, want) && convex, `${c.name}: área ${got} ≠ ${want} o pieza no convexa`);
    console.log(`${c.name}: ${pieces.length} piezas · área ${got.toFixed(3)} / ${want.toFixed(3)} · convexas ${convex}`);
  }
}
