"""Validación del gemelo contra telemetría real exportada de InfluxDB.

Uso (desde gemelo-digital/):

    # 1) exportar la telemetría (ver datos/exportar_telemetria.flux y el README)
    python -m gemelo.validacion telemetria.csv --grafico validacion.png
    # sin robot: CSV de ejemplo generado por el propio simulador
    python -m gemelo.validacion --generar-ejemplo datos/ejemplo_telemetria.csv
    python -m gemelo.validacion datos/ejemplo_telemetria.csv --grafico validacion.png

Qué hace: busca un tramo en que el robot estuvo de pie (motores = 1), simula al gemelo
desde el mismo ángulo inicial con las mismas ganancias y superpone ángulo y velocidad.
El robot real recibe empujones y comandos que el gemelo no conoce, así que lo que conviene
comparar es el transitorio inicial (cuando se lo suelta) y las características globales:
amplitud (RMS) y frecuencia dominante de la oscilación. Si difieren mucho, hay que revisar
los parámetros físicos (MEDICION.md) o el signo del ángulo (--invertir-angulo).
"""

from __future__ import annotations

import argparse
import csv
import io
import math
import sys
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from typing import Dict, List, Optional, Sequence

import numpy as np

from . import modelo as mod
from .modelo import GananciasPID, ParametrosFisicos
from .simulador import (
    RUIDO_ACCEL_TIPICO_DEG,
    RUIDO_GYRO_TIPICO_DPS,
    ConfigSimulacion,
    Escenario,
    ResultadoSimulacion,
    simular,
)

# Consulta Flux para exportar la telemetría rápida (A:) que guarda Node-RED.
# Ajustar el rango (start/stop) al momento del ensayo.
CONSULTA_FLUX = """\
from(bucket: "robot")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "telemetria" and r.robot == "balancebot")
  |> filter(fn: (r) => r._field == "angulo" or r._field == "velocidad" or r._field == "motores")
  |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
  |> keep(columns: ["_time", "angulo", "velocidad", "motores"])
  |> sort(columns: ["_time"])
"""


@dataclass
class Telemetria:
    t: np.ndarray  # [s] desde la primera muestra
    angulo_deg: np.ndarray
    velocidad: np.ndarray  # actual_robot_speed, unidades del firmware
    motores: np.ndarray  # 1 = motores activos
    inicio: Optional[datetime] = None

    def __len__(self) -> int:
        return len(self.t)

    def recorte(self, desde_s: float, hasta_s: float) -> "Telemetria":
        m = (self.t >= desde_s) & (self.t <= hasta_s)
        return Telemetria(self.t[m] - self.t[m][0], self.angulo_deg[m], self.velocidad[m], self.motores[m], self.inicio)


# ---------------------------------------------------------------------------
# Lectura del CSV
# ---------------------------------------------------------------------------
def _leer_texto(ruta: str) -> str:
    datos = open(ruta, "rb").read()
    # PowerShell 5 redirige con ">" en UTF-16; Excel a veces agrega BOM UTF-8
    if datos.startswith((b"\xff\xfe", b"\xfe\xff")):
        return datos.decode("utf-16")
    return datos.decode("utf-8-sig")


def _parsear_tiempo(texto: str) -> datetime:
    """RFC3339 de InfluxDB (con Z y hasta 9 decimales) → datetime con zona horaria."""
    texto = texto.strip().replace("Z", "+00:00")
    if "." in texto:
        base, resto = texto.split(".", 1)
        i = next((k for k, ch in enumerate(resto) if not ch.isdigit()), len(resto))
        texto = f"{base}.{resto[:i][:6].ljust(6, '0')}{resto[i:]}"
    t = datetime.fromisoformat(texto)
    return t if t.tzinfo else t.replace(tzinfo=timezone.utc)


def leer_csv_influx(ruta: str) -> Telemetria:
    """Lee el CSV exportado de InfluxDB (consulta CONSULTA_FLUX).

    Acepta el CSV anotado de la interfaz web / `influx query --raw` (líneas #group,
    #datatype, #default, una columna vacía al principio y varias tablas con su propio
    encabezado) y también un CSV simple con columnas _time, angulo, velocidad, motores.
    """
    filas: List[Dict[str, str]] = []
    encabezado: Optional[List[str]] = None
    for fila in csv.reader(io.StringIO(_leer_texto(ruta))):
        if not any(c.strip() for c in fila) or fila[0].startswith("#"):
            continue  # línea vacía entre tablas o anotación (#group, #datatype, #default)
        if "_time" in fila:
            encabezado = [c.strip() for c in fila]
            continue
        if encabezado is None:
            continue
        filas.append(dict(zip(encabezado, fila)))

    faltan = {"_time", "angulo"} - set(filas[0] if filas else {})
    if not filas or faltan:
        raise ValueError(f"{ruta}: no encontré las columnas _time/angulo (¿se exportó con pivot?)")

    def num(f: Dict[str, str], k: str, defecto: float) -> float:
        v = (f.get(k) or "").strip()
        return float(v) if v else defecto

    filas = [f for f in filas if (f.get("angulo") or "").strip()]
    tiempos = [_parsear_tiempo(f["_time"]) for f in filas]
    orden = np.argsort([t.timestamp() for t in tiempos], kind="stable")
    t0 = tiempos[orden[0]]
    return Telemetria(
        t=np.array([(tiempos[i] - t0).total_seconds() for i in orden]),
        angulo_deg=np.array([num(filas[i], "angulo", math.nan) for i in orden]),
        velocidad=np.array([num(filas[i], "velocidad", 0.0) for i in orden]),
        motores=np.array([int(num(filas[i], "motores", 1)) for i in orden]),
        inicio=t0,
    )


def tramos_de_pie(tel: Telemetria, minimo_s: float = 3.0, hueco_max_s: float = 0.5) -> List[tuple]:
    """(t_inicio, t_fin) de los tramos con motores activos y sin huecos de datos."""
    tramos, inicio = [], None
    for i in range(len(tel)):
        activo = tel.motores[i] == 1
        corte = i > 0 and tel.t[i] - tel.t[i - 1] > hueco_max_s
        if inicio is not None and (not activo or corte):
            fin = tel.t[i - 1]
            if fin - inicio >= minimo_s:
                tramos.append((inicio, fin))
            inicio = None
        if activo and inicio is None:
            inicio = tel.t[i]
    if inicio is not None and tel.t[-1] - inicio >= minimo_s:
        tramos.append((inicio, tel.t[-1]))
    return tramos


# ---------------------------------------------------------------------------
# Comparación
# ---------------------------------------------------------------------------
def frecuencia_dominante(t: np.ndarray, y: np.ndarray) -> float:
    """Frecuencia [Hz] del pico del espectro (sin la componente continua)."""
    if len(t) < 8:
        return math.nan
    dt = float(np.median(np.diff(t)))
    tu = np.arange(t[0], t[-1], dt)
    yu = np.interp(tu, t, y)
    yu = yu - np.mean(yu)
    espectro = np.abs(np.fft.rfft(yu * np.hanning(len(yu))))
    f = np.fft.rfftfreq(len(yu), dt)
    if len(f) < 3:
        return math.nan
    return float(f[1:][np.argmax(espectro[1:])])


@dataclass
class Comparacion:
    real: Telemetria
    sim: ResultadoSimulacion
    angulo_sim_deg: np.ndarray  # interpolado en los tiempos reales (como lo vería la telemetría)
    velocidad_sim: np.ndarray
    rmse_deg: float
    rmse_inicial_deg: float  # primeros segundos (transitorio al soltarlo)
    rms_real_deg: float
    rms_sim_deg: float
    frec_real_hz: float
    frec_sim_hz: float


def comparar(
    real: Telemetria,
    ganancias: Optional[GananciasPID] = None,
    parametros: Optional[ParametrosFisicos] = None,
    config: Optional[ConfigSimulacion] = None,
    segundos_iniciales: float = 2.0,
) -> Comparacion:
    """Simula el gemelo desde el primer ángulo del tramo real y compara ambas series."""
    esc = Escenario("validación", duracion_s=float(real.t[-1]) + 0.1, angulo_inicial_deg=float(real.angulo_deg[0]))
    sim = simular(ganancias, esc, parametros, config)
    # La telemetría manda angle_adjusted con 1 decimal: se compara contra lo mismo
    ang = np.round(np.interp(real.t, sim.t, sim.angulo_firmware_deg), 1)
    vel = np.interp(real.t, sim.t, sim.actual_robot_speed)
    if sim.cayo:  # después de caer el gemelo no tiene datos: se deja la última muestra
        ang[real.t > sim.t[-1]] = np.nan
    ok = ~np.isnan(ang)
    ini = ok & (real.t <= segundos_iniciales)
    return Comparacion(
        real=real,
        sim=sim,
        angulo_sim_deg=ang,
        velocidad_sim=vel,
        rmse_deg=float(np.sqrt(np.mean((ang[ok] - real.angulo_deg[ok]) ** 2))),
        rmse_inicial_deg=float(np.sqrt(np.mean((ang[ini] - real.angulo_deg[ini]) ** 2))) if ini.any() else math.nan,
        rms_real_deg=float(np.sqrt(np.mean((real.angulo_deg - np.mean(real.angulo_deg)) ** 2))),
        rms_sim_deg=float(np.sqrt(np.nanmean((ang - np.nanmean(ang)) ** 2))),
        frec_real_hz=frecuencia_dominante(real.t, real.angulo_deg),
        frec_sim_hz=frecuencia_dominante(real.t[ok], ang[ok]),
    )


def graficar(comp: Comparacion, archivo: str) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, (a1, a2) = plt.subplots(2, 1, figsize=(10, 6), sharex=True)
    a1.plot(comp.real.t, comp.real.angulo_deg, ".-", ms=3, lw=1, label="real (InfluxDB)")
    a1.plot(comp.real.t, comp.angulo_sim_deg, lw=1.5, label="gemelo")
    a1.set_ylabel("ángulo [°]")
    a1.set_title(
        f"RMSE {comp.rmse_deg:.1f}° · RMS real {comp.rms_real_deg:.1f}° vs gemelo {comp.rms_sim_deg:.1f}° · "
        f"frecuencia {comp.frec_real_hz:.2f} vs {comp.frec_sim_hz:.2f} Hz",
        fontsize=9, loc="left",
    )
    a2.plot(comp.real.t, comp.real.velocidad, ".-", ms=3, lw=1, label="real")
    a2.plot(comp.real.t, comp.velocidad_sim, lw=1.5, label="gemelo")
    a2.set_ylabel("velocidad [unid. firmware]")
    a2.set_xlabel("t [s] desde el inicio del tramo")
    for a in (a1, a2):
        a.grid(alpha=0.3)
        a.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(archivo, dpi=120)
    plt.close(fig)


# ---------------------------------------------------------------------------
# CSV de ejemplo (cuando todavía no hay datos del robot)
# ---------------------------------------------------------------------------
def generar_csv_ejemplo(ruta: str, semilla: int = 7) -> None:
    """Escribe un CSV con el mismo formato que exporta InfluxDB (anotado y pivoteado).

    Simula un "robot real" que NO es idéntico al gemelo por defecto (centro de masa 10 %
    más alto e inercia 30 % mayor), con ruido de sensor y algunos empujones, precedido de
    1 s acostado (motores = 0). Así la validación muestra diferencias reales que analizar.
    """
    robot = ParametrosFisicos(altura_cm_m=0.088, inercia_cuerpo_kg_m2=0.00195)
    cfg = ConfigSimulacion(ruido_gyro_dps=RUIDO_GYRO_TIPICO_DPS, ruido_accel_deg=RUIDO_ACCEL_TIPICO_DEG, semilla=semilla)
    esc = Escenario(
        "sesión de ejemplo",
        duracion_s=12.0,
        angulo_inicial_deg=8.0,
        empujes=[(4.0, 0.1, 2.0), (7.5, 0.1, -2.5)],
        joystick=[(9.0, 11.0, 0.5)],
    )
    res = simular(GananciasPID(), esc, robot, cfg)

    inicio = datetime(2026, 11, 12, 18, 30, 0, tzinfo=timezone.utc)
    filas = [(inicio + timedelta(seconds=0.1 * i), -80.0, 0, 0) for i in range(10)]  # acostado
    for t, msg in res.mensajes_telemetria():
        _, ang, vel, mot = msg.split(":")
        filas.append((inicio + timedelta(seconds=1.0 + t), float(ang), int(vel), int(mot)))

    with open(ruta, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["#group", "false", "false", "false", "false", "false", "false"])
        w.writerow(["#datatype", "string", "long", "dateTime:RFC3339", "double", "long", "long"])
        w.writerow(["#default", "_result", "", "", "", "", ""])
        w.writerow(["", "result", "table", "_time", "angulo", "motores", "velocidad"])
        for ts, ang, vel, mot in filas:
            w.writerow(["", "", 0, ts.strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z", f"{ang:.1f}", mot, vel])


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def main(argv: Optional[Sequence[str]] = None) -> int:
    ap = argparse.ArgumentParser(prog="python -m gemelo.validacion", description="Gemelo vs. telemetría real (InfluxDB)")
    ap.add_argument("csv", nargs="?", help="CSV exportado de InfluxDB")
    ap.add_argument("--generar-ejemplo", metavar="CSV", help="escribir un CSV de ejemplo generado por el simulador")
    ap.add_argument("--consulta", action="store_true", help="mostrar la consulta Flux para exportar los datos")
    ap.add_argument("-g", "--ganancias", default=None, help="ganancias cargadas en el robot (KP,KD,KPT,KIT)")
    ap.add_argument("--tramo", type=int, default=1, help="número de tramo de pie a usar (1 = el primero)")
    ap.add_argument("--desde", type=float, help="inicio del tramo [s desde la primera muestra] (manual)")
    ap.add_argument("--hasta", type=float, help="fin del tramo [s]")
    ap.add_argument("--invertir-angulo", action="store_true", help="multiplicar el ángulo real por −1")
    ap.add_argument("--ruido", action="store_true", help="simular con ruido típico de sensor")
    ap.add_argument("--modelo", choices=mod.MODELOS, default=mod.MODELO_LAGRANGE)
    ap.add_argument("--grafico", metavar="ARCHIVO.png")
    args = ap.parse_args(argv)

    if args.consulta:
        print(CONSULTA_FLUX)
        return 0
    if args.generar_ejemplo:
        generar_csv_ejemplo(args.generar_ejemplo)
        print(f"CSV de ejemplo escrito en {args.generar_ejemplo}")
        if not args.csv:
            return 0
    if not args.csv:
        ap.error("falta el CSV (o usá --generar-ejemplo / --consulta)")

    tel = leer_csv_influx(args.csv)
    if args.invertir_angulo:
        tel.angulo_deg = -tel.angulo_deg
    print(f"{len(tel)} muestras, {tel.t[-1]:.1f} s desde {tel.inicio:%Y-%m-%d %H:%M:%S} UTC")

    if args.desde is not None:
        desde, hasta = args.desde, args.hasta if args.hasta is not None else tel.t[-1]
    else:
        tramos = tramos_de_pie(tel)
        if not tramos:
            print("No hay tramos de pie de al menos 3 s (motores = 1).")
            return 1
        for i, (a, b) in enumerate(tramos, 1):
            print(f"  tramo {i}: {a:7.1f} – {b:7.1f} s ({b - a:.1f} s)")
        desde, hasta = tramos[min(args.tramo, len(tramos)) - 1]
        if args.hasta is not None:
            hasta = args.hasta
    real = tel.recorte(desde, hasta)
    print(f"Usando {desde:.1f} – {hasta:.1f} s ({len(real)} muestras)")

    cfg = ConfigSimulacion(
        modelo=args.modelo,
        ruido_gyro_dps=RUIDO_GYRO_TIPICO_DPS if args.ruido else 0.0,
        ruido_accel_deg=RUIDO_ACCEL_TIPICO_DEG if args.ruido else 0.0,
        semilla=1,
    )
    g = GananciasPID.desde_texto(args.ganancias) if args.ganancias else GananciasPID()
    comp = comparar(real, g, ParametrosFisicos(), cfg)
    print(f"Ganancias del gemelo: {g.comando_ws()}")
    print(f"RMSE ángulo:            {comp.rmse_deg:6.2f} °  (primeros 2 s: {comp.rmse_inicial_deg:.2f} °)")
    print(f"RMS del ángulo:         real {comp.rms_real_deg:5.2f} °   gemelo {comp.rms_sim_deg:5.2f} °")
    print(f"Frecuencia dominante:   real {comp.frec_real_hz:5.2f} Hz  gemelo {comp.frec_sim_hz:5.2f} Hz")
    if comp.sim.cayo:
        print(f"Atención: el gemelo se cae a los {comp.sim.t_caida_s:.2f} s y el robot real no.")
    if args.grafico:
        graficar(comp, args.grafico)
        print(f"Gráfico guardado en {args.grafico}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
