// Visor 3D del robot balanceador (gemelo digital).
// Genera una página HTML autónoma: el cuerpo se inclina sobre el eje de las ruedas según el
// ángulo de la telemetría `A:` y las ruedas giran según la velocidad de los motores.
//
//   node build.mjs            -> iiot/node-red/gemelo.html (la sirve Node-RED en /gemelo)
//   node build.mjs --dark     -> tema oscuro (gemelo-oscuro.html)
//   node build.mjs --audit    -> prueba la geometría (lento: correr en segundo plano)
//
// Medidas en mm tomadas de los STL de docs/hardware/3d_models/files (ruedas Ø88, paneles
// laterales 75,7 × 88, estantes de 96 × 62,7 / 73,5) y de un NEMA17 (42,3 mm, tornillos a 31 mm).
// Kit de dibujo: ./kit (Anatomy, licencia MIT).
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import * as k from "./kit/iso-kit.mjs";
import * as G from "./kit/lathe.mjs";
import * as A from "./kit/audit.mjs";
import { turning } from "./kit/turn-build.mjs";
import * as TA from "./kit/turn-audit.mjs";
import { turnScript } from "./scripts/inline-kit.mjs";
import { PERFILES, BUJE_STL, EJE_STL, RAYOS, BRAZOS } from "./piezas/robot.mjs";
import { convexPieces, areaOf as convexArea } from "./piezas/convexas.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const theme = argv.includes("--dark") ? "dark" : "light";

const WIDTH = 640;
const HEIGHT = 520;
const AZIMUTH = 28;
const ELEVATION = 30;

// ---- Verdades del firmware (esp32_balancing_robot/) ----
export const ANGULO_CAIDA = 74; // angle_ready: pasado este ángulo se apagan los motores
export const ANGULO_LEVANTADO = 56; // pasado este ángulo usa las ganancias de levantado
// Motors.cpp: pasos/s = velocidad · 25 (1/8 de micropaso), 200 · 8 = 1600 pasos por vuelta
export const GRADOS_POR_UNIDAD = (25 / 1600) * 360; // 5,625 °/s de rueda por unidad de velocidad

// ---- Geometría (mm). x: eje de las ruedas, y: adelante, z: arriba ----
// Las piezas impresas salen de los STL (piezas/robot.mjs). Del sistema del STL al del mundo
// solo cambia z: el eje de las ruedas queda a la altura del radio de la rueda.
const perfil = (name) => PERFILES.find((p) => p.name === name);
// Altura del eje = radio de la rueda (Ø88 en el STL). Para que apoye en el banco en reposo, la
// rueda se gira hasta que su vértice más lejano del centro quede abajo.
const RIM = perfil("rueda").layers[0].islands[0].outer;
const farthest = RIM.reduce((best, p) => (Math.hypot(...p) > Math.hypot(...best) ? p : best));
// A 3 decimales, como guarda el kit la página: si no, los cortes de las placas no coinciden
const AX = Math.round(Math.hypot(...farthest) * 1e3) / 1e3;
const WHEEL_TURN = -Math.PI / 2 - Math.atan2(farthest[1], farthest[0]);
const DZ = AX - EJE_STL[1];
// En el STL los paneles quedan 3,17 mm afuera de los estantes (x = ±51,17 contra ±48); se
// acercan para que los estantes, que se atornillan a su cara interior, apoyen.
const PANEL_IN = 48;
const PANEL_OUT = PANEL_IN + 3.5;
const MOTOR_H = 21.15;
const MOTOR_X = [8, 48]; // NEMA17 de 40 mm con la brida contra el panel
const BOSS = [MOTOR_X[1], MOTOR_X[1] + 2]; // resalte Ø22 del motor
// Buje: z del STL del buje convertida a x (el cuello arranca en el resalte del motor)
const hubX = (zh) => BOSS[1] + (zh - BUJE_STL.collar[0]);
const WHEEL_X = [hubX(BUJE_STL.flange[0] - 6.4), hubX(BUJE_STL.flange[0])];
const zOf = (name, index = 0) => perfil(name).layers[index];
const TRAY_Z = zOf("bandeja").s0 + DZ;
const SHELF1_Z = zOf("estante").s0 + DZ;
const DECK = zOf("estante").s1 + DZ;
const TOP = zOf("techo").s1 + DZ;

const FLOOR = { x: -112, y: -126, w: 214, d: 252, r: 8 };
const FLOOR_H = 8;
const FOOT_H = 5;
const SCALE_X = [-102, -98];
const SCALE_R = 118;

export const P = k.fitProjection([...k.boxCorners(FLOOR, -FLOOR_H - FOOT_H, 0), [SCALE_X[0], 0, AX + SCALE_R + 4], [0, 0, TOP + 8]], WIDTH, HEIGHT, { pad: 20, azimuth: AZIMUTH, elevation: ELEVATION });
const at = (p) => k.iso(p, P);
const S = (paths, style) => k.solidSvg(paths, style);
const L = (d, style) => k.lineSvg(d, style);
const D = (points, style) => k.dotsSvg(points, style);
const frameOf = (o, a, u) => {
  const axis = G.unit3(a);
  const uu = u ? G.unit3(G.sub3(u, G.mul3(axis, G.dot3(u, axis)))) : G.frameAlong(o, axis).u;
  return { o, a: axis, u: uu, v: G.cross3(axis, uu) };
};
// Prismas a lo largo de x: polígono en (y, z) del mundo
const FX = { o: [0, 0, 0], a: [1, 0, 0], u: [0, 1, 0], v: [0, 0, 1] };
// Prismas verticales: polígono en (x, y) del mundo
const FZ = { o: [0, 0, 0], a: [0, 0, 1], u: [1, 0, 0], v: [0, 1, 0] };
// Piezas redondas sobre el eje de las ruedas
const AXLE = frameOf([0, 0, AX], [1, 0, 0], [0, 0, 1]);

export const R = A.recorder(P);
const put = (item) => R.put({ ...item, shapes: item.shapes.map((shape, index) => R.solid(Object.assign(shape, { name: shape.name ?? (index ? `${item.name}#${index + 1}` : item.name) }))) });

// ---- Ángulo en que el cuerpo toca el piso (de ahí sale el rango de la bisagra) ----
const PANEL_YZ = perfil("panel").layers[0].islands[0].outer.map(([y, z]) => [y, z + DZ]);
const OUTLINE_YZ = [...PANEL_YZ, [-25, TRAY_Z], [25, TRAY_Z]];
const lowestAt = (deg) => {
  const t = (deg * Math.PI) / 180;
  return Math.min(...OUTLINE_YZ.map(([y, z]) => AX + y * Math.sin(t) + (z - AX) * Math.cos(t)));
};
let lying = 60;
while (lowestAt(lying + 0.25) > 0.6 && lowestAt(-lying - 0.25) > 0.6) lying += 0.25;
export const TILT_MAX = Math.floor(lying);

// ---- Estáticos: banco, pies, regla y cuadrante de ángulos ----
function bench() {
  for (const [x, y] of k.corners(FLOOR, 22)) {
    const svg = S(k.cylinder(x, y, 7, -FLOOR_H - FOOT_H, 1.6, P, 28), { tone: "lo" }) + S(k.cylinder(x, y, 5.4, -FLOOR_H - FOOT_H + 1.6, FOOT_H - 1.6, P, 28), { tone: "mid" });
    put({ name: `stand.foot.${x}.${y}`, layer: "back", at: [x, y, -FLOOR_H - FOOT_H], svg, shapes: [A.cylinder(x, y, 7, -FLOOR_H - FOOT_H, 1.6, { ground: true }), A.cylinder(x, y, 5.4, -FLOOR_H - FOOT_H + 1.6, FOOT_H - 1.6)] });
  }
  const screws = k.corners(FLOOR, 8);
  const groove = L(k.planOutline(k.insetPlan(FLOOR, 5), 0, P, 8), { tone: "faint" });
  const label = { x: 56, y: -90, w: 36, d: 16, r: 1.5 };
  const labelLines = [0, 1, 2].map((i) => k.lineOnTop([label.x + 4, label.y + 4 + i * 4], [label.x + (i === 2 ? 20 : 32), label.y + 4 + i * 4], 0, P)).join("");
  const svg = `<path class="iso-halo" d="${k.haloOf(FLOOR, -FLOOR_H, FLOOR_H, P)}"/>` + S(k.slabOf(FLOOR, -FLOOR_H, FLOOR_H, P, 8, 1.4), { tone: "lo", inner: L(k.sideSeam(FLOOR, -FLOOR_H + 3, P, 8), { tone: "faint" }) }) + groove + L(screws.map(([x, y]) => k.ring(x, y, 2.2, 0, P, 16)).join(""), { tone: "lo" }) + D(screws.map(([x, y]) => at([x, y, 0])), { size: 0.5 });
  // Placa y regla grabadas en el banco (sin espesor: la rueda apoya en z = 0)
  const plate = L(k.planOutline(label, 0, P, 3), { tone: "lo" }) + L(labelLines, { tone: "faint", free: true }) + D(k.corners(label, 1.6).map(([x, y]) => at([x, y, 0])), { size: 0.4 });
  // Regla a lo largo de y: el robot avanza en esta dirección
  const strip = { x: 80, y: -88, w: 12, d: 176, r: 1 };
  const ticks = k.topTicks(strip.y + 4, strip.y + strip.d - 4, 4, 5, strip.x, 0, [2.2, 4.4], P, "y");
  const ruler = L(k.lineOnTop([strip.x, strip.y], [strip.x, strip.y + strip.d], 0, P), { tone: "lo" }) + L(ticks.minor, { tone: "lo" }) + L(ticks.major, { tone: "mid" });
  put({ name: "stand.bench", layer: "back", at: [0, 0, -FLOOR_H], bias: -50, svg: svg + plate + ruler, shapes: [A.slab(FLOOR, -FLOOR_H, FLOOR_H, { ground: true })] });
}

// Cuadrante vertical detrás de la rueda izquierda: marca ±56° (levantado) y ±74° (caída).
// Es una pieza viva fija (sin grupo) en la capa de la aguja: el kit no puede probar el orden
// entre una placa estática vertical y una aguja que gira apoyada en ella.
const SCALE_SPAN = 82;
function quadrant(layer) {
  const arc = [];
  for (let i = 0; i <= 41; i++) {
    const phi = ((-SCALE_SPAN + (i * 2 * SCALE_SPAN) / 41) * Math.PI) / 180;
    arc.push([SCALE_R * Math.sin(phi), AX + SCALE_R * Math.cos(phi)]);
  }
  const BASE_H = 4;
  const poly = [[arc[0][0], BASE_H], ...arc, [arc[arc.length - 1][0], BASE_H]];
  const plate = T.prism({ name: "stand.quadrant", layer, F: FX, polygon: poly, s0: SCALE_X[0], s1: SCALE_X[1], tone: "mid", crease: "lo", bevel: 0.6 });
  // Marcas en el plano de la cara (u = y, v = z, origen en el eje)
  const xy = (deg, r) => {
    const t = (deg * Math.PI) / 180;
    return `${(r * Math.sin(t)).toFixed(3)} ${(r * Math.cos(t)).toFixed(3)}`;
  };
  const tick = (deg, r0, r1) => `M${xy(deg, r0)}L${xy(deg, r1)}`;
  const edge = SCALE_R - 3;
  let minor = "";
  let major = "";
  for (let deg = -80; deg <= 80; deg += 2) {
    if (deg % 10 === 0) major += tick(deg, edge, edge - 7);
    else minor += tick(deg, edge, edge - 4);
  }
  const marks = [-ANGULO_CAIDA, ANGULO_CAIDA, -ANGULO_LEVANTADO, ANGULO_LEVANTADO].map((deg) => tick(deg, edge + 1.6, edge - 12)).join("");
  const rim = Array.from({ length: 81 }, (_, i) => `${i ? "L" : "M"}${xy(-80 + i * 2, edge)}`).join("");
  const svg = `<path class="iso-line" data-tone="faint" d="${rim}"/><path class="iso-line" data-tone="lo" d="${minor}"/><path class="iso-line" data-tone="mid" d="${major}${marks}"/><path class="iso-line" data-tone="hi" d="${tick(0, edge + 1.6, edge - 14)}"/>`;
  T.plane({ part: plate, o: [SCALE_X[1], 0, AX], u: [0, 1, 0], v: [0, 0, 1], normal: [1, 0, 0], svg });
  // Zócalo del cuadrante, atornillado al banco
  const base = { x: SCALE_X[0] - 4, y: arc[0][0] - 6, w: 12, d: -2 * arc[0][0] + 12, r: 1.4 };
  const screws = [[base.x + 2.4, base.y + 3], [base.x + 2.4, base.y + base.d - 3], [base.x + base.w - 2.4, base.y + 3], [base.x + base.w - 2.4, base.y + base.d - 3]];
  T.prism({ name: "stand.quadrant.base", layer, plan: base, z: 0, h: BASE_H, steps: 3, tone: "lo", bevel: 0.4, details: { dots: screws.map(([x, y]) => ({ at: [BASE_H, x, y], normal: [0, 0, 1], size: 0.45 })) } });
}

// ---- Partes vivas ----
export const T = turning(P, { recorder: R });
const bodyGroup = T.group("body", { hinge: { point: [0, 0, AX], axis: [1, 0, 0], range: [-TILT_MAX, TILT_MAX] } });
const wheels = T.group("wheel", { parent: bodyGroup, hinge: { point: [0, 0, AX], axis: [1, 0, 0], range: [0, 360] } });
// Aguja del cuadrante: gira sobre el mismo eje, con el ángulo del cuerpo
const needle = T.group("needle", { hinge: { point: [0, 0, AX], axis: [1, 0, 0], range: [-TILT_MAX, TILT_MAX] } });
const robot = T.layer("robot");
const dial = T.layer("needle");
T.stack(["back", "needle", "robot"]);

// tilt: inclinación hacia adelante (+y) en grados; roll: vuelta de la rueda hacia adelante en grados.
// La bisagra de la rueda es relativa al cuerpo y se lleva a [0, 360).
export const follow = (values) => {
  const bodyDeg = -(values.tilt ?? 0);
  const wheelWorld = -(values.roll ?? 0);
  const rel = wheelWorld - bodyDeg;
  return { body: bodyDeg, needle: bodyDeg, wheel: ((rel % 360) + 360) % 360 };
};
export const poses = () => {
  const out = [];
  // La rueda respecto del cuerpo depende solo de su giro: inclinaciones con la rueda quieta y
  // una vuelta fina de rueda con el cuerpo vertical
  for (let tilt = -TILT_MAX; tilt <= TILT_MAX; tilt += 2) for (let roll = 0; roll < 360; roll += 30) out.push(follow({ tilt, roll }));
  for (let roll = 0; roll < 360; roll += 1.5) out.push(follow({ tilt: 0, roll }));
  return out;
};

const B = { group: bodyGroup, layer: robot };
const box = (name, plan, z, h, extra = {}) => T.prism({ name, ...B, plan, z, h, steps: extra.steps ?? 2, tone: "mid", ...extra });
const alongX = (name, s0, s1, polygon, extra = {}) => T.prism({ name, ...B, F: FX, polygon, s0, s1, tone: "mid", ...extra });
const roundX = (name, s0, s1, r, extra = {}) => T.round({ name, ...B, F: AXLE, s0, s1, r, tone: "mid", ...extra });

// Placa impresa: cada isla del perfil se parte en piezas convexas (el kit solo dibuja piezas
// convexas). Las aristas compartidas entre piezas se marcan como cortes para que no se dibujen.
const pointOf = (F, [u, v]) => G.add3(F.o, G.add3(G.mul3(F.u, u), G.mul3(F.v, v)));
const insideConvex = (poly, [x, y]) => poly.every((p, i) => {
  const q = poly[(i + 1) % poly.length];
  return (q[0] - p[0]) * (y - p[1]) - (q[1] - p[1]) * (x - p[0]) >= -1e-9;
});
let plateCount = 0;
// Recorta un polígono convexo al semiplano u·k ≤ c (Sutherland-Hodgman)
const clipHalf = (poly, axis, c, sign) => {
  const out = [];
  const side = (p) => sign * p[axis] - c;
  for (let i = 0; i < poly.length; i++) {
    const a = poly[i];
    const b = poly[(i + 1) % poly.length];
    if (side(a) <= 0) out.push(a);
    if ((side(a) < 0 && side(b) > 0) || (side(a) > 0 && side(b) < 0)) {
      const t = side(a) / (side(a) - side(b));
      out.push([a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t]);
    }
  }
  return out;
};
function plate(name, { group, layer, F, s0, s1, islands, tone = "hi", material, lit, map = (p) => p, maxU = Infinity }) {
  // Sin pliegues: en una arista interna el pliegue quedaba a medias bajo la pieza vecina
  const crease = "none";
  const parts = [];
  islands.forEach((island, ii) => {
    const outer = island.outer.map(map);
    const holes = island.holes.map((h) => h.map(map));
    // maxU recorta la pieza a |u| ≤ maxU: el piso de la bandeja no se mete bajo los paneles
    const clipped = convexPieces(outer, holes).map((piece) => {
      if (!(maxU < Infinity)) return piece;
      const polygon = clipHalf(clipHalf(piece.polygon, 0, maxU, 1), 0, maxU, -1);
      // Las aristas sobre el recorte son borde; las internas se recalculan por coincidencia
      return { polygon, internal: null };
    }).filter((piece) => piece.polygon.length >= 3 && Math.abs(convexArea(piece.polygon)) > 0.05);
    if (maxU < Infinity) {
      const key = (p) => `${Math.round(p[0] * 1e3)},${Math.round(p[1] * 1e3)}`;
      const count = new Map();
      const edgeKey = (poly, i) => [key(poly[i]), key(poly[(i + 1) % poly.length])].sort().join("|");
      for (const piece of clipped) piece.polygon.forEach((_, i) => count.set(edgeKey(piece.polygon, i), (count.get(edgeKey(piece.polygon, i)) ?? 0) + 1));
      for (const piece of clipped) piece.internal = piece.polygon.map((_, i) => count.get(edgeKey(piece.polygon, i)) > 1);
    }
    for (const [pi, piece] of clipped.entries()) {
      // El kit guarda la página con 3 decimales y compara los cortes con tolerancia 0,001: los
      // vértices se redondean antes, y la normal del corte (sin normalizar, perpendicular exacta
      // a la arista) sale de esos mismos valores, así que el redondeo no la mueve.
      const polygon = piece.polygon.map(([u, v]) => [Math.round(u * 1e3) / 1e3, Math.round(v * 1e3) / 1e3]);
      const part = T.prism({ name: `${name}.${ii}.${pi}`, group, layer, F, polygon, s0, s1, tone, crease, material, lit, round: 0 });
      const origin = part.group ? part.group.origin : [0, 0, 0];
      for (let i = 0; i < polygon.length; i++) {
        if (!piece.internal[i]) continue;
        const p = pointOf(F, polygon[i]);
        const q = pointOf(F, polygon[(i + 1) % polygon.length]);
        const n = G.cross3(F.a, G.sub3(q, p)).map((c) => Math.round(c * 1e3) / 1e3);
        part.spec.cuts = [...(part.spec.cuts ?? []), { n, d: G.dot3(n, p) - G.dot3(n, origin) }];
        // Al final: el kit toma los primeros planos de cada pieza, y el eje (F.a) va primero
        part.hints.push(G.unit3(n));
      }
      parts.push({ part, polygon });
      plateCount++;
    }
  });
  return parts;
}
// Agujeros de tornillo grabados como anillos, en el plano de la cara de la pieza que los contiene
const ringPath = (cx, cy, r, steps = 40) => Array.from({ length: steps }, (_, j) => `${j ? "L" : "M"}${(cx + r * Math.cos((j / steps) * Math.PI * 2)).toFixed(3)} ${(cy + r * Math.sin((j / steps) * Math.PI * 2)).toFixed(3)}`).join("") + "Z";
function engrave(pieces, loops, { o, u, v, normal, map = (p) => p }) {
  for (const loop of loops.map((l) => l.map(map))) {
    const c = loop.reduce((m, p) => [m[0] + p[0] / loop.length, m[1] + p[1] / loop.length], [0, 0]);
    const r = Math.sqrt(Math.abs(convexArea(loop)) / Math.PI);
    const host = pieces.find(({ polygon }) => insideConvex(polygon, c)) ?? pieces[0];
    // El plano está en coordenadas (u, v) de la placa, con origen o
    T.plane({ part: host.part, o, u, v, normal, svg: `<path class="iso-line" data-tone="lo" d="${ringPath(c[0] - G.dot3(o, u), c[1] - G.dot3(o, v), r, 20)}"/>` });
  }
}

// Paneles laterales: perfil exacto del STL, con los agujeros de alivianado calados
const panelLayer = perfil("panel").layers[0];
const toPanel = ([y, z]) => [y, z + DZ];
for (const side of [-1, 1]) {
  const tag = side > 0 ? "+" : "-";
  const pieces = plate(`body.panel${tag}`, { ...B, F: FX, s0: side > 0 ? PANEL_IN : -PANEL_OUT, s1: side > 0 ? PANEL_OUT : -PANEL_IN, islands: panelLayer.islands, tone: "hi", crease: "lo", map: toPanel });
  engrave(pieces, panelLayer.engraved, { o: [side * PANEL_OUT, 0, 0], u: [0, 1, 0], v: [0, 0, 1], normal: [side, 0, 0], map: toPanel });
}

// Motores NEMA17 (42,3 mm), eje coaxial con las ruedas. No están en los STL: son comprados.
const motorPoly = (() => {
  const h = MOTOR_H;
  const c = 4.2;
  return [[-h + c, AX - h], [h - c, AX - h], [h, AX - h + c], [h, AX + h - c], [h - c, AX + h], [-h + c, AX + h], [-h, AX + h - c], [-h, AX - h + c]];
})();
for (const side of [-1, 1]) {
  const tag = side > 0 ? "+" : "-";
  const [a, b] = side > 0 ? MOTOR_X : [-MOTOR_X[1], -MOTOR_X[0]];
  const motor = alongX(`body.motor${tag}`, a, b, motorPoly, { tone: "hi", crease: "lo", material: "gunmetal", bevel: 0.5, details: { seams: [a + 7, b - 7] } });
  const [f0, f1] = side > 0 ? BOSS : [-BOSS[1], -BOSS[0]];
  roundX(`body.boss${tag}`, f0, f1, 11, { material: "steel" });
  const lx = side > 0 ? a + 12 : a + 10;
  const sticker = `<rect x="0" y="-6" width="18" height="12" rx="0.8" class="iso-line" data-tone="faint" fill="none"/>`;
  const lines = [[-3, 14], [0, 11], [3, 8]].map(([y, w]) => `<path class="iso-line" data-tone="faint" data-free="" d="M2.5 ${y}L${2.5 + w} ${y}"/>`).join("");
  T.plane({ part: motor, o: [lx, MOTOR_H, AX], u: [1, 0, 0], v: [0, 0, -1], normal: [0, 1, 0], svg: sticker + lines });
  // Conector JST en la cara delantera del motor, cerca del extremo interior
  const cx = side > 0 ? MOTOR_X[0] + 2 : -MOTOR_X[0] - 12;
  box(`body.motor${tag}.jst`, { x: cx, y: MOTOR_H, w: 10, d: 4.6, r: 0.6 }, AX - 5, 10, { tone: "hi", bevel: 0.3, details: { seams: [AX + 2.5] } });
}

// Bandeja de motores, estante de electrónica y estante superior: perfiles exactos de los STL
const plates = {};
for (const [name, tag] of [["bandeja", "body.tray"], ["estante", "body.shelf"], ["techo", "body.top"]])
  for (const [index, layer] of perfil(name).layers.entries()) {
    const pieces = (plates[`${tag}${index}`] = plate(`${tag}${index}`, { ...B, F: FZ, s0: layer.s0 + DZ, s1: layer.s1 + DZ, islands: layer.islands, tone: "hi", maxU: PANEL_IN }));
    if (layer.engraved.length) engrave(pieces, layer.engraved, { o: [0, 0, layer.s1 + DZ], u: [1, 0, 0], v: [0, 1, 0], normal: [0, 0, 1] });
  }

// Batería plana entre los motores y el estante, con interruptor en el frente
const BAT = { x: -36, y: -17, w: 72, d: 34, r: 1.6 };
const BAT_Z = AX + MOTOR_H;
const BAT_H = SHELF1_Z - BAT_Z - 0.6;
const battery = box("body.battery", BAT, BAT_Z, BAT_H, { tone: "mid", material: "gunmetal", bevel: 0.5, details: { seams: [BAT_Z + BAT_H - 3] } });
T.plane({ part: battery, o: [-6, BAT.y + BAT.d, BAT_Z + BAT_H / 2], u: [1, 0, 0], v: [0, 0, -1], normal: [0, 1, 0], svg: `<rect x="0" y="-5" width="38" height="10" rx="0.8" class="iso-line" data-tone="lo" fill="none"/><path class="iso-line" data-tone="lo" data-free="" d="M3 -1.5L24 -1.5M3 1.8L18 1.8"/><path class="iso-line" data-tone="mid" d="M30 0L35 0M32.5 -2.5L32.5 2.5"/>` });
box("body.switch", { x: -30, y: BAT.y + BAT.d, w: 9, d: 2.6, r: 0.5 }, BAT_Z + 3, 7, { tone: "hi", bevel: 0.25 });
box("body.switch.lever", { x: -27.5, y: BAT.y + BAT.d + 2.6, w: 4, d: 1.8, r: 0.4 }, BAT_Z + 5, 3, { tone: "hi", material: "chrome" });

// ESP32 sobre zócalos, en el centro del estante
const ESP = { x: -24, y: -10, w: 48, d: 26, r: 1.2 };
for (const [i, y] of [[0, -9], [1, 13]]) box(`body.esp.header${i}`, { x: -22, y, w: 44, d: 2.5, r: 0.4 }, DECK, 8.5, { tone: "lo" });
const espPcb = box("body.esp.pcb", ESP, DECK + 8.5, 1.6, { tone: "mid", material: "gunmetal", details: { dots: [[-21.5, -7.5], [21.5, -7.5], [-21.5, 13.5], [21.5, 13.5]].map(([x, y]) => ({ at: [DECK + 10.1, x, y], normal: [0, 0, 1], size: 0.5 })) } });
box("body.esp.module", { x: -2, y: -6.5, w: 19, d: 19, r: 0.6 }, DECK + 10.1, 3.1, { tone: "hi", material: "steel", bevel: 0.3, details: { seams: [DECK + 10.8] } });
T.plane({ part: espPcb, o: [18.5, -6, DECK + 10.1], u: [1, 0, 0], v: [0, 1, 0], normal: [0, 0, 1], svg: `<path class="iso-line" data-tone="hi" d="M0 1L4 1L4 4L1 4L1 7L4 7L4 10L1 10L1 13L4 13L4 16"/>` });
box("body.esp.led", { x: -9, y: 9.5, w: 2, d: 1.4, r: 0.3 }, DECK + 10.1, 0.8, { tone: "hi" });
box("body.esp.usb", { x: -26, y: -0.5, w: 6, d: 7, r: 0.6 }, DECK + 10.1, 2.8, { tone: "hi", material: "chrome" });
box("body.esp.btn0", { x: -15, y: -7, w: 3, d: 3, r: 0.4 }, DECK + 10.1, 1.2, { tone: "lo" });
box("body.esp.btn1", { x: -15, y: 10, w: 3, d: 3, r: 0.4 }, DECK + 10.1, 1.2, { tone: "lo" });

// Drivers DRV8825 con disipador, en la parte de atrás del estante
for (const side of [-1, 1]) {
  const tag = side > 0 ? "+" : "-";
  const x0 = side > 0 ? 24 : -40;
  const y0 = -36;
  box(`body.drv${tag}.socket`, { x: x0 + 1, y: y0 + 1, w: 14, d: 18, r: 0.5 }, DECK, 7, { tone: "lo" });
  box(`body.drv${tag}.pcb`, { x: x0, y: y0, w: 16, d: 20, r: 0.5 }, DECK + 7, 1.6, { tone: "mid", material: "copper" });
  T.round({ name: `body.drv${tag}.trim`, ...B, F: frameOf([x0 + 13, y0 + 18.2, 0], [0, 0, 1], [1, 0, 0]), s0: DECK + 8.6, s1: DECK + 9.6, r: 1.3, tone: "mid", material: "brass", details: { bolts: [{ s: DECK + 9.6, r: 0, count: 1, size: 0.35 }] } });
  box(`body.drv${tag}.sink`, { x: x0 + 3, y: y0 + 4, w: 10, d: 12, r: 0.4 }, DECK + 8.6, 1.6, { tone: "hi", material: "steel" });
  for (let f = 0; f < 4; f++) box(`body.drv${tag}.fin${f}`, { x: x0 + 3.6 + f * 2.6, y: y0 + 4.4, w: 1.2, d: 11.2, r: 0.2 }, DECK + 10.2, 4.4, { tone: "hi", material: "steel" });
}

// MPU6050: el sensor que mide el ángulo que dibuja este visor (la pieza resaltada)
const MPU_X = 8;
box("body.mpu.header", { x: MPU_X + 1, y: 20.5, w: 18, d: 2.5, r: 0.4 }, DECK, 6, { tone: "lo" });
box("body.mpu.post", { x: MPU_X + 8.5, y: 31, w: 3, d: 3, r: 0.6 }, DECK, 6, { tone: "lo" });
box("body.mpu", { x: MPU_X, y: 20, w: 20, d: 15, r: 0.8 }, DECK + 6, 1.6, { tone: "hi", lit: true, details: { dots: [[2, 22], [18, 22], [2, 33], [18, 33]].map(([x, y]) => ({ at: [DECK + 7.6, MPU_X + x, y], normal: [0, 0, 1], size: 0.5 })) } });
box("body.mpu.pins", { x: MPU_X + 2, y: 33.4, w: 16, d: 1.2, r: 0.2 }, DECK + 7.6, 0.8, { tone: "mid", material: "brass" });
box("body.mpu.chip", { x: MPU_X + 7.8, y: 25.3, w: 4.4, d: 4.4, r: 0.3 }, DECK + 7.6, 0.9, { tone: "lo", material: "gunmetal" });

// Leyenda grabada en el estante superior, en la pieza que contiene su centro
const topHost = plates["body.top0"].find(({ polygon }) => insideConvex(polygon, [-4, 0])) ?? plates["body.top0"][0];
T.plane({ part: topHost.part, o: [-30, 4, TOP], u: [1, 0, 0], v: [0, 1, 0], normal: [0, 0, 1], svg: `<text class="iso-face-text" data-tone="lo" font-size="9" letter-spacing="1.4">CODAPLI</text><text class="iso-face-text" data-tone="faint" font-size="5" y="9" letter-spacing="1">UTN FRLP · 2026</text>` });

// ---- Ruedas y bujes (grupo hijo del cuerpo, coaxial: gira sobre el mismo eje) ----
// La rueda del STL está en su plano (x, y): se lleva al plano (y, z) del mundo.
const W = { group: wheels, layer: robot };
const wheelLayer = perfil("rueda").layers[0];
const rotate = ([x, y], t) => [x * Math.cos(t) - y * Math.sin(t), x * Math.sin(t) + y * Math.cos(t)];
const toWheel = (p) => {
  const [x, y] = rotate(p, WHEEL_TURN);
  return [x, AX + y];
};
// La brida en "Y" del buje se gira para quedar sobre los rayos de la rueda
const turnHub = RAYOS[1] - BRAZOS + WHEEL_TURN;
const toHub = ([x, y]) => [x * Math.cos(turnHub) - y * Math.sin(turnHub), AX + x * Math.sin(turnHub) + y * Math.cos(turnHub)];
for (const side of [-1, 1]) {
  const tag = side > 0 ? "+" : "-";
  const span = (a, b) => (side > 0 ? [a, b] : [-b, -a]);
  const [w0, w1] = span(...WHEEL_X);
  plate(`wheel.rim${tag}`, { ...W, F: FX, s0: w0, s1: w1, islands: wheelLayer.islands, tone: "mid", crease: "lo", material: "rubber", map: toWheel });
  const [c0, c1] = span(hubX(BUJE_STL.collar[0]), WHEEL_X[0]);
  plate(`wheel.collar${tag}`, { ...W, F: FX, s0: c0, s1: c1, islands: BUJE_STL.layers.collar, tone: "mid", material: "steel", map: toHub });
  const [f0, f1] = span(hubX(BUJE_STL.flange[0]), hubX(BUJE_STL.flange[1]));
  plate(`wheel.flange${tag}`, { ...W, F: FX, s0: f0, s1: f1, islands: BUJE_STL.layers.flange, tone: "hi", crease: "lo", map: toHub });
  const [p0, p1] = span(hubX(BUJE_STL.cap[0]), hubX(BUJE_STL.cap[1]));
  plate(`wheel.cap${tag}`, { ...W, F: FX, s0: p0, s1: p1, islands: BUJE_STL.layers.cap, tone: "hi", crease: "lo", map: toHub });
}

// Aguja: buje sobre la cara del cuadrante, aguja fina y contrapeso
const NEEDLE_X = [SCALE_X[1], SCALE_X[1] + 1.2];
const N = { group: needle, layer: dial };
T.round({ name: "needle.hub", ...N, F: AXLE, s0: SCALE_X[1], s1: NEEDLE_X[1] + 1.4, r: 4.2, tone: "mid", material: "brass", details: { bolts: [{ s: NEEDLE_X[1] + 1.4, r: 0, count: 1, size: 0.5 }] } });
T.prism({ name: "needle.arm", ...N, F: FX, polygon: [[-1.1, AX + 4.4], [1.1, AX + 4.4], [0.35, AX + SCALE_R - 4], [-0.35, AX + SCALE_R - 4]], s0: NEEDLE_X[0], s1: NEEDLE_X[1], tone: "hi", crease: "lo" });
T.prism({ name: "needle.tail", ...N, F: FX, polygon: [[-2.6, AX - 4.4], [2.6, AX - 4.4], [3.4, AX - 13], [-3.4, AX - 13]], s0: NEEDLE_X[0], s1: NEEDLE_X[1], tone: "mid", crease: "lo", material: "brass" });

bench();
quadrant(dial);
const verify = argv.includes("--verify");
const out = T.build({ statics: R.items, poses, verify });
A.settle(R, P);
const layerOf = (name) => R.items.filter((item) => item.layer === name).sort((a, b) => a.key - b.key).map((item) => item.svg);
if (argv.includes("--audit")) {
  A.auditOrExit(R, P);
  TA.orbitOrExit(T, R, P, {
    sweep: { at: (x) => follow({ tilt: x - TILT_MAX, roll: 0 }), from: 0, to: 2 * TILT_MAX, frame: 0.3 },
    clips: [{ name: "roll", at: (t) => follow({ tilt: 8 * Math.sin(t * 2), roll: 90 * t }), from: 0, to: 4, frame: 1 / 60 }],
    quick: argv.includes("--quick"),
    only: argv.find((a) => a.startsWith("--only="))?.slice(7).split(",") ?? null,
    log: argv.includes("--verbose") ? (text) => console.log(`  … ${text}`) : null,
  });
}

const LABEL = "Robot balanceador de dos ruedas sobre un banco: dos paneles laterales unidos por una bandeja de motores y dos estantes, dos motores paso a paso con el eje en el de las ruedas, batería, ESP32, dos drivers y el sensor MPU6050. Detrás, un cuadrante marca los ángulos de levantado y de caída.";
const backSvg = k.figureSvg({ width: WIDTH, height: HEIGHT, label: LABEL, body: layerOf("back") });
const stage = `<div class="iso-stage iso-turn gm-stage" tabindex="0" role="slider" aria-label="Robot: inclinación en vivo. Flechas izquierda y derecha para inclinarlo a mano" aria-valuemin="-${TILT_MAX}" aria-valuemax="${TILT_MAX}" aria-valuenow="0" aria-valuetext="vertical">${backSvg}${out.layerSvg("needle", { width: WIDTH, height: HEIGHT })}${out.layerSvg("robot", { width: WIDTH, height: HEIGHT })}</div>`;

const DATA = { turn: out.data, tiltMax: TILT_MAX, fall: ANGULO_CAIDA, raise: ANGULO_LEVANTADO, degPerUnit: GRADOS_POR_UNIDAD };
const CSS = out.css + "\n" + readFileSync(join(HERE, "visor.css"), "utf8");
const LIVE = readFileSync(join(HERE, "live.js"), "utf8").replace("__DATA__", JSON.stringify(DATA)).replace("const follow = null;", `const follow = ${follow.toString()};`);
const body = k.plateHtml({
  fig: "Gemelo",
  title: "Robot balanceador",
  hint: "Telemetría A: en vivo · ← → inclina a mano",
  readout: "conectando…",
  keys: [
    { mark: "raised", label: "Cuerpo: gira sobre el eje de las ruedas" },
    { mark: "lit", label: "MPU6050: mide el ángulo" },
    { mark: "raised", label: "Aguja: el mismo ángulo sobre el cuadrante" },
  ],
  caption: `El ángulo y la velocidad salen del mensaje A: del robot (10 Hz). La rueda gira ${GRADOS_POR_UNIDAD.toFixed(3)} °/s por unidad de velocidad (1/8 de micropaso). El cuadrante marca ±${ANGULO_LEVANTADO}° (ganancias de levantado) y ±${ANGULO_CAIDA}° (motores apagados).`,
  body: `<style>${CSS}</style>${stage}`,
});
const page = k.pageHtml({ title: "Gemelo digital", theme, body, script: turnScript() + (verify ? `window.__isoVerify = ${JSON.stringify(out.verify)};\n` : "") + LIVE, width: 680 }).replace('<html lang="en">', '<html lang="es">');
if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const target = argv.find((a) => a.startsWith("--out="))?.slice(6) ?? join(HERE, "..", "..", "iiot", "node-red", `gemelo${verify ? "-verify" : ""}${theme === "dark" ? "-oscuro" : ""}.html`);
  writeFileSync(target, page);
  console.log(`wrote ${target} · ${(page.length / 1024).toFixed(0)} KB · ${out.report.parts} parts · tilt ±${TILT_MAX}° · build ${out.report.seconds.toFixed(1)} s`);
}
