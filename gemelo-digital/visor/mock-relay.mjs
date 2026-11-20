// Servidor local para ver el visor sin robot ni Docker.
// Imita a Node-RED: GET /gemelo sirve la página y ws://localhost:<puerto>/ws/gemelo emite
// mensajes A:<angulo>:<velocidad>:<motores> simulados a 10 Hz (balanceo, avance y una caída cada 30 s).
//
//   node mock-relay.mjs [puerto]      -> http://localhost:18800/gemelo
import { createServer } from "node:http";
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const PORT = Number(process.argv[2] ?? 18800);
const PAGES = { "/gemelo": "gemelo.html", "/gemelo-oscuro": "gemelo-oscuro.html" };

const ease = (x) => x * x * (3 - 2 * x);
// Ángulo en grados (+ adelante), velocidad en unidades del firmware, motores 0/1
function sample(t) {
  const phase = t % 30;
  const wobble = 2.5 * Math.sin(2.1 * t) + 1.2 * Math.sin(5.3 * t + 0.7);
  const drive = Math.sin(0.35 * t);
  if (phase < 22) return [wobble + 6 * drive, Math.round(45 * drive + 4 * wobble), 1];
  if (phase < 23) {
    const s = ease(phase - 22);
    const angle = wobble * (1 - s) + 88 * s * s;
    return [angle, 0, angle < 74 ? 1 : 0];
  }
  if (phase < 26) return [88 + 0.3 * Math.sin(9 * t), 0, 0];
  const s = ease((phase - 26) / 4);
  const angle = 88 * (1 - s) + wobble * s;
  return [angle, 0, angle < 74 ? 1 : 0];
}

const server = createServer((req, res) => {
  const page = PAGES[req.url.split("?")[0]];
  if (!page) {
    res.writeHead(302, { location: "/gemelo" });
    return res.end();
  }
  res.writeHead(200, { "content-type": "text/html; charset=utf-8" });
  res.end(readFileSync(join(HERE, "..", "..", "iiot", "node-red", page)));
});

const frame = (text) => {
  const body = Buffer.from(text);
  return Buffer.concat([Buffer.from([0x81, body.length]), body]);
};
const start = Date.now();
server.on("upgrade", (req, socket) => {
  if (req.url !== "/ws/gemelo" || !req.headers["sec-websocket-key"]) return socket.destroy();
  const accept = createHash("sha1").update(req.headers["sec-websocket-key"] + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").digest("base64");
  socket.write(`HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ${accept}\r\n\r\n`);
  const timer = setInterval(() => {
    const [angle, speed, motors] = sample((Date.now() - start) / 1000);
    socket.write(frame(`A:${angle.toFixed(1)}:${speed}:${motors}`));
  }, 100);
  const stop = () => clearInterval(timer);
  socket.on("close", stop);
  socket.on("error", stop);
  socket.on("data", () => {}); // lo que mande el navegador se descarta, como en Node-RED
});

server.listen(PORT, "127.0.0.1", () => console.log(`visor con datos simulados: http://localhost:${PORT}/gemelo`));
