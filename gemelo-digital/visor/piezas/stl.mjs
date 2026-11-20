// Lectura de STL y corte en perfiles 2D: las piezas impresas del robot son placas extruidas,
// así que un corte a una altura dada devuelve su contorno exacto (exterior y agujeros).
import { readFileSync } from "node:fs";

export function loadStl(file) {
  const b = readFileSync(file);
  const n = b.length >= 84 ? b.readUInt32LE(80) : 0;
  const tris = [];
  if (84 + n * 50 === b.length) {
    for (let i = 0; i < n; i++) {
      const t = [];
      for (let v = 0; v < 3; v++) t.push([0, 1, 2].map((a) => b.readFloatLE(84 + i * 50 + 12 + v * 12 + a * 4)));
      tris.push(t);
    }
  } else {
    const vs = [...b.toString("utf8").matchAll(/vertex\s+(\S+)\s+(\S+)\s+(\S+)/g)].map((m) => [+m[1], +m[2], +m[3]]);
    for (let i = 0; i + 2 < vs.length; i += 3) tris.push([vs[i], vs[i + 1], vs[i + 2]]);
  }
  return tris;
}

export const areaOf = (loop) => {
  let a = 0;
  for (let i = 0; i < loop.length; i++) {
    const p = loop[i];
    const q = loop[(i + 1) % loop.length];
    a += p[0] * q[1] - q[0] * p[1];
  }
  return a / 2;
};

// Corta con el plano coord[axis] = h. Devuelve lazos cerrados en las otras dos coordenadas (u, v),
// con u, v = los dos ejes restantes en orden (x, y, z sin `axis`).
export function sliceStl(tris, axis, h) {
  const [ua, va] = [0, 1, 2].filter((a) => a !== axis);
  const segs = [];
  for (const t of tris) {
    const pts = [];
    for (let i = 0; i < 3; i++) {
      const p = t[i];
      const q = t[(i + 1) % 3];
      if ((p[axis] - h) * (q[axis] - h) < 0) {
        const s = (h - p[axis]) / (q[axis] - p[axis]);
        pts.push([p[ua] + s * (q[ua] - p[ua]), p[va] + s * (q[va] - p[va])]);
      }
    }
    if (pts.length === 2) segs.push(pts);
  }
  const key = (p) => `${Math.round(p[0] * 1e3)},${Math.round(p[1] * 1e3)}`;
  const ends = new Map();
  segs.forEach((s, i) => s.forEach((p) => (ends.get(key(p)) ?? ends.set(key(p), []).get(key(p))).push(i)));
  const used = new Uint8Array(segs.length);
  const loops = [];
  for (let i = 0; i < segs.length; i++) {
    if (used[i]) continue;
    used[i] = 1;
    const loop = [segs[i][0]];
    let end = segs[i][1];
    for (;;) {
      const next = (ends.get(key(end)) ?? []).find((j) => !used[j]);
      if (next === undefined) break;
      used[next] = 1;
      loop.push(end);
      const s = segs[next];
      end = key(s[0]) === key(end) ? s[1] : s[0];
    }
    if (loop.length >= 3 && key(end) === key(loop[0])) loops.push(loop);
  }
  return loops;
}

// Douglas-Peucker sobre un lazo cerrado
export function simplify(loop, tol) {
  const n = loop.length;
  if (n < 8) return loop.slice();
  let far = 0;
  let best = -1;
  for (let i = 1; i < n; i++) {
    const d = Math.hypot(loop[i][0] - loop[0][0], loop[i][1] - loop[0][1]);
    if (d > best) [best, far] = [d, i];
  }
  const keep = new Uint8Array(n);
  keep[0] = keep[far] = 1;
  const run = (a, b) => {
    const stack = [[a, b]];
    while (stack.length) {
      const [i, j] = stack.pop();
      const p = loop[i];
      const q = loop[j % n];
      const dx = q[0] - p[0];
      const dy = q[1] - p[1];
      const L = Math.hypot(dx, dy) || 1;
      let worst = -1;
      let at = -1;
      for (let k = i + 1; k < j; k++) {
        const r = loop[k % n];
        const d = Math.abs((r[0] - p[0]) * dy - (r[1] - p[1]) * dx) / L;
        if (d > worst) [worst, at] = [d, k];
      }
      if (worst > tol) {
        keep[at % n] = 1;
        stack.push([i, at], [at, j]);
      }
    }
  };
  run(0, far);
  run(far, n);
  return loop.filter((_, i) => keep[i]);
}

const inside = (p, loop) => {
  let hit = false;
  for (let i = 0, j = loop.length - 1; i < loop.length; j = i++) {
    const a = loop[i];
    const b = loop[j];
    if (a[1] > p[1] !== b[1] > p[1] && p[0] < ((b[0] - a[0]) * (p[1] - a[1])) / (b[1] - a[1]) + a[0]) hit = !hit;
  }
  return hit;
};

// Perfil de una placa: corta a la altura h, descarta lazos menores que minArea (mm²) y los
// simplifica. Devuelve las islas sólidas (contorno antihorario) con sus agujeros (horarios),
// según cuántos lazos envuelven a cada uno.
export function profileOf(tris, axis, h, { tol = 0.2, minArea = 0.5 } = {}) {
  const loops = sliceStl(tris, axis, h)
    .map((l) => simplify(l, tol))
    .filter((l) => l.length >= 3 && Math.abs(areaOf(l)) >= minArea);
  const depth = loops.map((l) => loops.filter((o) => o !== l && Math.abs(areaOf(o)) > Math.abs(areaOf(l)) && inside(l[0], o)).length);
  const orient = (l, ccw) => ((areaOf(l) > 0) === ccw ? l : l.slice().reverse());
  const islands = loops.map((l, i) => (depth[i] % 2 === 0 ? { outer: orient(l, true), holes: [] } : null));
  loops.forEach((l, i) => {
    if (depth[i] % 2 === 0) return;
    // El agujero pertenece a la isla más chica que lo contiene con un nivel menos
    let owner = -1;
    loops.forEach((o, j) => {
      if (depth[j] === depth[i] - 1 && inside(l[0], o) && (owner < 0 || Math.abs(areaOf(o)) < Math.abs(areaOf(loops[owner])))) owner = j;
    });
    if (owner >= 0) islands[owner].holes.push(orient(l, false));
  });
  return islands.filter(Boolean).sort((a, b) => areaOf(b.outer) - areaOf(a.outer));
}

export const circleOf = (loop) => {
  const c = loop.reduce((s, p) => [s[0] + p[0] / loop.length, s[1] + p[1] / loop.length], [0, 0]);
  return { c, r: Math.sqrt(Math.abs(areaOf(loop)) / Math.PI) };
};
