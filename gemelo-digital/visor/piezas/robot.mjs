// Perfiles de las piezas impresas, cortados de los STL de docs/hardware/3d_models/files.
// Coordenadas del STL (mm). Los paneles, los estantes y la bandeja ya vienen ensamblados en un
// mismo sistema: el eje de los motores está en y = 0, z = 21,15 (los tornillos superiores del
// NEMA17 del panel están en z = 36,5 = 21,15 + 15,5, y el motor apoya en el piso de la bandeja, z = 0).
// La rueda y el buje vienen cada uno en su propio sistema y se ubican en build.mjs.
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { loadStl, profileOf, sliceStl, areaOf } from "./stl.mjs";

const DIR = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..", "docs", "hardware", "3d_models", "files");
export const EJE_STL = [0, 21.15]; // (y, z) del eje de las ruedas en el sistema del STL

// axis: eje del espesor (0 = x, 2 = z). Cada capa es un corte a la altura h, extruido de s0 a s1.
// tol: tolerancia de simplificación del contorno (mm). Los agujeros de menos de minHole mm² se
// graban como anillos en vez de calarse (son los de tornillo; calarlos multiplica las piezas).
const SPEC = [
  { name: "panel", file: "Side_Panel_[V11.stl", axis: 0, tol: 0.5, layers: [{ h: -52.92, s0: -54.67, s1: -51.17 }] },
  { name: "rueda", file: "Wheel_[V10.stl", axis: 2, tol: 0.5, layers: [{ h: 3.2, s0: 0, s1: 6.4 }] },
  { name: "estante", file: "Electronics_Shelf_[V10.stl", axis: 2, tol: 0.7, layers: [{ h: 63.5, s0: 60.97, s1: 66.13 }, { h: 67.1, s0: 66.13, s1: 68.13 }] },
  { name: "techo", file: "Top_Shelf_[V10.stl", axis: 2, tol: 0.7, layers: [{ h: 116.9, s0: 114, s1: 120 }] },
  { name: "bandeja", file: "Motor_Shelf_[V10.stl", axis: 2, tol: 0.7, layers: [{ h: -2, s0: -4, s1: 0 }, { h: 3.5, s0: 0, s1: 7 }, { h: 8.5, s0: 7, s1: 10 }] },
];
const MIN_HOLE = 12;

export const PERFILES = SPEC.map((spec) => {
  const tris = loadStl(join(DIR, spec.file));
  const layers = spec.layers.map((layer) => {
    const islands = profileOf(tris, spec.axis, layer.h, { tol: spec.tol, minArea: 1 });
    const engraved = [];
    for (const island of islands) {
      const keep = [];
      for (const hole of island.holes) (Math.abs(areaOf(hole)) < MIN_HOLE ? engraved : keep).push(hole);
      island.holes = keep;
    }
    return { ...layer, islands, engraved };
  });
  return { ...spec, layers };
});

// Buje (Hub_[V10_ENLARGED.stl): cuello que va al eje del motor, brida en "Y" que se atornilla a
// la cara exterior de la rueda y un escalón. Coordenadas centradas en el eje del cuello.
export const BUJE_STL = (() => {
  const tris = loadStl(join(DIR, "Hub_[V10_ENLARGED.stl"));
  const at = (h) => profileOf(tris, 2, h, { tol: 0.5, minArea: 1 });
  const collar = at(15);
  const c = collar[0].outer.reduce((m, p, _, all) => [m[0] + p[0] / all.length, m[1] + p[1] / all.length], [0, 0]);
  const centred = (islands) => islands.map((i) => ({ outer: i.outer.map((p) => [p[0] - c[0], p[1] - c[1]]), holes: [] }));
  return {
    collar: [9.56, 21.5],
    flange: [21.5, 24.56],
    cap: [24.56, 25.5],
    layers: { collar: centred(collar), flange: centred(at(23)), cap: centred(at(25)) },
  };
})();

// Ángulo (rad) de los rayos de la rueda: a mitad de camino entre las aberturas
export const RAYOS = (() => {
  const wheel = PERFILES.find((p) => p.name === "rueda").layers[0].islands[0];
  const centre = (l) => l.reduce((m, p) => [m[0] + p[0] / l.length, m[1] + p[1] / l.length], [0, 0]);
  const openings = wheel.holes.filter((h) => Math.abs(areaOf(h)) > 200).map((h) => Math.atan2(centre(h)[1], centre(h)[0])).sort((a, b) => a - b);
  return openings.map((a, i) => {
    const b = openings[(i + 1) % openings.length] + (i + 1 === openings.length ? 2 * Math.PI : 0);
    return (a + b) / 2;
  });
})();
// Ángulo (rad) de los brazos de la brida del buje: los puntos más lejanos del centro
export const BRAZOS = (() => {
  const flange = BUJE_STL.layers.flange[0].outer;
  const far = flange.map((p) => [Math.hypot(p[0], p[1]), Math.atan2(p[1], p[0])]).sort((a, b) => b[0] - a[0]);
  return far[0][1];
})();
