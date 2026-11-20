// Vista previa en SVG de los perfiles cortados de los STL (para revisar el corte a ojo)
//   node piezas/preview.mjs > perfiles.svg
import { PERFILES } from "./robot.mjs";
const cells = [];
let x = 0;
for (const part of PERFILES) {
  const all = part.layers.flatMap((l) => l.islands.flatMap((i) => [i.outer, ...i.holes]));
  const us = all.flat().map((p) => p[0]);
  const vs = all.flat().map((p) => p[1]);
  const [u0, u1, v0, v1] = [Math.min(...us), Math.max(...us), Math.min(...vs), Math.max(...vs)];
  const paths = part.layers.map((l, k) => l.islands.map((i) => [i.outer, ...i.holes].map((loop) => `M${loop.map((p) => `${(p[0] - u0 + x).toFixed(2)} ${(v1 - p[1]).toFixed(2)}`).join("L")}Z`).join("")).join("")).map((d, k) => `<path d="${d}" fill="hsl(${k * 70} 60% 60% / .35)" stroke="#222" stroke-width=".3" fill-rule="evenodd"/>`).join("");
  cells.push(`${paths}<text x="${x}" y="${v1 - v0 + 8}" font-size="5">${part.name}</text>`);
  x += u1 - u0 + 12;
}
console.log(`<svg xmlns="http://www.w3.org/2000/svg" viewBox="-5 -5 ${x + 10} 140" width="${(x + 10) * 4}">${cells.join("")}</svg>`);
