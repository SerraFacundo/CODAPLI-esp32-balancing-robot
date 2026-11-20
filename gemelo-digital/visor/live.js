const DATA = __DATA__;
const follow = null;
const stage = document.querySelector(".gm-stage");
const readout = document.querySelector("[data-readout]");
const still = matchMedia("(prefers-reduced-motion: reduce)");
const params = new URLSearchParams(location.search);
// ?invertir=1 da vuelta el signo del ángulo si el robot real se inclina al revés que el dibujo
const SIGN = params.get("invertir") === "1" ? -1 : 1;
const clampTilt = (x) => Math.max(-DATA.tiltMax, Math.min(DATA.tiltMax, x));

let tilt = 0;
let target = 0;
let roll = 0;
let rate = 0;
let motors = true;
let mode = "demo";
let lastData = 0;
let manualUntil = 0;
// Una herramienta externa (window.__isoTurn) toma el control: el visor deja de dibujar 4 s
let heldUntil = 0;
let clock = 0;
let frame = 0;
let last = 0;
let visible = false;
let cost = 0;
let skip = false;

// ---- Fuente de datos: relay de Node-RED (sin token) o el robot directo (?ws=…&token=…) ----
const wsUrl = params.get("ws") ?? (location.protocol.startsWith("http") ? `${location.protocol === "https:" ? "wss" : "ws"}://${location.host}/ws/gemelo` : null);
const token = params.get("token");
let socket = null;
let retry = 0;
const connect = () => {
  if (!wsUrl) return;
  try {
    socket = new WebSocket(wsUrl);
  } catch {
    return;
  }
  socket.onopen = () => {
    retry = 0;
    if (token) socket.send(`T:${token}`);
  };
  socket.onmessage = (event) => {
    for (const line of String(event.data).split("\n")) {
      const parts = line.trim().split(":");
      if (parts[0] !== "A" || parts.length !== 4) continue;
      const angle = Number(parts[1]);
      const speed = Number(parts[2]);
      if (!Number.isFinite(angle) || !Number.isFinite(speed)) continue;
      target = clampTilt(SIGN * angle);
      rate = speed * DATA.degPerUnit;
      motors = parts[3] === "1";
      lastData = performance.now();
      if (mode !== "manual") mode = "vivo";
      run();
    }
  };
  socket.onclose = () => {
    socket = null;
    setTimeout(connect, Math.min(8000, 500 * 2 ** retry++));
  };
};
connect();

// ---- Demo: balanceo suave con una caída cada 24 s, para cuando no llegan datos ----
const ease = (x) => x * x * (3 - 2 * x);
const demoAt = (t) => {
  const phase = t % 24;
  const wobble = 3.2 * Math.sin(1.7 * t) + 1.4 * Math.sin(4.3 * t + 1);
  if (phase < 16) return { tilt: wobble, rate: 140 * Math.sin(0.5 * t) + 25 * wobble, motors: true };
  if (phase < 17.2) {
    const s = ease((phase - 16) / 1.2);
    return { tilt: wobble * (1 - s) + DATA.tiltMax * s * s, rate: 0, motors: s * s * DATA.tiltMax < DATA.fall };
  }
  if (phase < 20) return { tilt: DATA.tiltMax, rate: 0, motors: false };
  const s = ease((phase - 20) / 4);
  return { tilt: DATA.tiltMax * (1 - s) + wobble * s, rate: 0, motors: DATA.tiltMax * (1 - s) < DATA.fall };
};

const show = () => {
  const sign = tilt >= 0 ? "+" : "−";
  const text = `θ ${sign}${Math.abs(tilt).toFixed(1)}° · rueda ${Math.round(rate)}°/s · motores ${motors ? "ON" : "OFF"} · ${mode}`;
  if (readout.textContent !== text) readout.textContent = text;
  stage.setAttribute("aria-valuenow", tilt.toFixed(0));
  stage.setAttribute("aria-valuetext", `${Math.abs(tilt).toFixed(0)} grados ${tilt >= 0 ? "adelante" : "atrás"}, motores ${motors ? "encendidos" : "apagados"}`);
  stage.toggleAttribute("data-off", !motors);
};

const controller = TURN.mount(stage, DATA.turn, { follow, onHold: () => (heldUntil = performance.now() + 4000) });
const draw = () => {
  controller.set({ tilt, roll });
  show();
};

function tick(now) {
  const dt = last ? Math.min((now - last) / 1000, 1 / 20) : 1 / 60;
  last = now;
  if (mode === "manual" && now > manualUntil) mode = lastData && now - lastData < 1500 ? "vivo" : "demo";
  if (mode === "vivo" && now - lastData > 1500) mode = "demo";
  if (mode === "demo") {
    clock += dt;
    const d = demoAt(clock);
    target = d.tilt;
    rate = d.rate;
    motors = d.motors;
  }
  if (mode === "manual") rate = 0;
  // La telemetría llega a 10 Hz: seguimiento exponencial para que el dibujo vaya a 60 fps
  tilt = still.matches ? target : tilt + (target - tilt) * (1 - Math.exp(-dt / 0.08));
  // Con movimiento reducido las ruedas no giran y solo se redibuja cuando llega un dato
  if (!still.matches) roll += rate * dt;
  // Medio cuadro en equipos lentos: si redibujar pasa de 14 ms, se dibuja un cuadro sí y otro no
  skip = cost > 14 ? !skip : cost < 9 ? false : skip;
  if (now > heldUntil && !skip) {
    const t0 = performance.now();
    draw();
    cost = cost * 0.9 + (performance.now() - t0) * 0.1;
  }
  frame = visible && !still.matches ? requestAnimationFrame(tick) : 0;
}
const run = () => {
  if (!frame && visible) {
    last = 0;
    frame = requestAnimationFrame(tick);
  }
};
new IntersectionObserver(
  (entries) => {
    visible = entries[entries.length - 1].isIntersecting;
    if (visible) run();
    else if (frame) {
      cancelAnimationFrame(frame);
      frame = 0;
    }
  },
  { rootMargin: "120px 0px" },
).observe(stage);

stage.addEventListener("keydown", (event) => {
  const step = { ArrowLeft: -3, ArrowRight: 3, ArrowDown: -3, ArrowUp: 3 }[event.key];
  if (event.key === "Home") target = 0;
  else if (step === undefined) return;
  else target = clampTilt((mode === "manual" ? target : tilt) + step);
  event.preventDefault();
  mode = "manual";
  motors = Math.abs(target) < DATA.fall;
  manualUntil = performance.now() + 4000;
  if (still.matches) tilt = target;
  run();
  if (still.matches) draw();
});
if (still.matches) draw();
