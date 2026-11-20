"""Banco de pruebas de ganancias PID sobre el gemelo.

Uso (desde gemelo-digital/):

    python -m gemelo.banco                                  # candidatos de ejemplo
    python -m gemelo.banco -g 0.32,0.05,0.08,0.1 -g "mio=0.45,0.08,0.08,0.05"
    python -m gemelo.banco --ruido --grafico banco.png
    python -m gemelo.banco --modelo base_impuesta --signo-motor 1

Cada juego de ganancias se simula en los mismos escenarios (empujón inicial, escalón de
joystick y empuje externo) y se resumen las métricas. Al final se ordenan los candidatos y
se muestra el comando "P:" listo para mandar al robot por WebSocket.
"""

from __future__ import annotations

import argparse
import math
import sys
from dataclasses import dataclass
from typing import List, Optional, Sequence, Tuple

import numpy as np

from . import modelo as mod
from .firmware import MAX_CONTROL_OUTPUT
from .modelo import GananciasPID, ParametrosFisicos
from .simulador import (
    RUIDO_ACCEL_TIPICO_DEG,
    RUIDO_GYRO_TIPICO_DPS,
    ConfigSimulacion,
    Escenario,
    ResultadoSimulacion,
    escenarios_por_defecto,
    simular,
)

BANDA_ESTABLECIMIENTO_DEG = 1.0  # |θ − θ_final| < 1° de ahí en adelante
HISTERESIS_OSCILACION_DEG = 0.5
VENTANA_FINAL_S = 1.0


@dataclass
class Metricas:
    cae: bool
    t_caida_s: Optional[float]
    angulo_max_deg: float  # máximo |θ| físico
    t_establecimiento_s: Optional[float]  # medido desde el evento; None = no se establece
    oscilaciones: int  # cruces del valor final (con histéresis) después del evento
    amplitud_final_pp_deg: float  # pico a pico de θ en el último segundo
    velocidad_final_m_s: float
    motor_rms: float  # velocidad de motor, unidades del firmware (1 = 5.625 °/s)
    motor_max: int
    pct_rampa_saturada: float  # % de ciclos en que MAX_ACCEL recortó el cambio de velocidad
    pct_salida_saturada: float  # % de ciclos con |control_output| = MAX_CONTROL_OUTPUT

    @property
    def motor_max_m_s(self) -> float:
        return mod.unidades_a_m_s(self.motor_max)


def calcular_metricas(res: ResultadoSimulacion) -> Metricas:
    t, th = res.t, res.theta_deg
    angulo_max = float(np.max(np.abs(th))) if len(th) else 0.0
    motor = res.speed_m.astype(float)
    base = dict(
        angulo_max_deg=angulo_max,
        motor_rms=float(np.sqrt(np.mean(motor**2))) if len(motor) else 0.0,
        motor_max=int(np.max(np.abs(res.speed_m))) if len(motor) else 0,
        pct_rampa_saturada=100.0 * float(np.mean(res.rampa_saturada)) if len(t) else 0.0,
        pct_salida_saturada=100.0 * float(np.mean(np.abs(res.control_output) >= MAX_CONTROL_OUTPUT)) if len(t) else 0.0,
    )
    if res.cayo:
        return Metricas(
            cae=True, t_caida_s=res.t_caida_s, t_establecimiento_s=None, oscilaciones=0,
            amplitud_final_pp_deg=float("nan"), velocidad_final_m_s=float("nan"), **base,
        )

    dt = t[1] - t[0] if len(t) > 1 else mod.DT_CONTROL_S
    n_final = max(1, int(round(VENTANA_FINAL_S / dt)))
    final = th[-n_final:]
    th_final = float(np.mean(final))

    desde = res.escenario.t_evento_s
    tramo = t >= desde
    error = np.abs(th[tramo] - th_final)
    fuera = np.nonzero(error > BANDA_ESTABLECIMIENTO_DEG)[0]
    if len(fuera) == 0:
        t_est = 0.0
    elif fuera[-1] >= len(error) - n_final:
        t_est = None  # sigue saliendo de la banda en el último segundo
    else:
        t_est = float(t[tramo][fuera[-1] + 1] - desde)

    # Cruces del valor final con histéresis (cada cruce es media oscilación)
    cruces, signo = 0, 0
    for e in th[tramo] - th_final:
        if e > HISTERESIS_OSCILACION_DEG and signo <= 0:
            cruces += signo < 0
            signo = 1
        elif e < -HISTERESIS_OSCILACION_DEG and signo >= 0:
            cruces += signo > 0
            signo = -1

    return Metricas(
        cae=False,
        t_caida_s=None,
        t_establecimiento_s=t_est,
        oscilaciones=cruces // 2,
        amplitud_final_pp_deg=float(np.ptp(final)),
        velocidad_final_m_s=float(np.mean(res.v_m_s[-n_final:])),
        **base,
    )


@dataclass
class ResultadoCandidato:
    nombre: str
    ganancias: GananciasPID
    resultados: List[ResultadoSimulacion]
    metricas: List[Metricas]

    @property
    def caidas(self) -> int:
        return sum(m.cae for m in self.metricas)

    def puntaje(self) -> Tuple[int, int, float, float]:
        """Orden lexicográfico (menor es mejor): caídas, escenarios sin establecerse,
        suma de tiempos de establecimiento y suma de ángulos máximos."""
        sin_establecer = sum((not m.cae) and m.t_establecimiento_s is None for m in self.metricas)
        t_total = sum(
            m.t_establecimiento_s if m.t_establecimiento_s is not None else r.escenario.duracion_s
            for m, r in zip(self.metricas, self.resultados)
        )
        return (self.caidas, sin_establecer, round(t_total, 2), round(sum(m.angulo_max_deg for m in self.metricas), 1))


def evaluar(
    candidatos: Sequence[Tuple[str, GananciasPID]],
    escenarios: Optional[Sequence[Escenario]] = None,
    parametros: Optional[ParametrosFisicos] = None,
    config: Optional[ConfigSimulacion] = None,
) -> List[ResultadoCandidato]:
    """Simula cada candidato en cada escenario. Devuelve la lista ordenada del mejor al peor."""
    escenarios = list(escenarios or escenarios_por_defecto())
    salida = []
    for nombre, g in candidatos:
        res = [simular(g, e, parametros, config) for e in escenarios]
        salida.append(ResultadoCandidato(nombre, g, res, [calcular_metricas(r) for r in res]))
    return sorted(salida, key=lambda rc: rc.puntaje())


CANDIDATOS_EJEMPLO: Tuple[Tuple[str, GananciasPID], ...] = (
    ("firmware", GananciasPID()),
    ("más Kd", GananciasPID(0.32, 0.08, 0.08, 0.1)),
    ("Ki bajo", GananciasPID(0.32, 0.05, 0.08, 0.03)),
    ("Kp+Kd, Ki bajo", GananciasPID(0.45, 0.08, 0.08, 0.05)),
    ("absurda", GananciasPID(0.02, 0.0, 0.08, 0.1)),
)


# ---------------------------------------------------------------------------
# Salida por consola
# ---------------------------------------------------------------------------
def _f(valor: Optional[float], fmt: str = ".2f", nada: str = "—") -> str:
    if valor is None or (isinstance(valor, float) and math.isnan(valor)):
        return nada
    return format(valor, fmt)


def tabla(resultados: Sequence[ResultadoCandidato]) -> str:
    lineas: List[str] = []
    escenarios = [r.escenario for r in resultados[0].resultados]
    ancho = max(len(r.nombre) for r in resultados)
    for i, esc in enumerate(escenarios):
        lineas.append(f"\n▶ {esc.nombre}  ({esc.duracion_s:g} s)")
        lineas.append(
            f"  {'candidato':<{ancho}}  {'¿cae?':>8}  {'θmáx °':>7}  {'t_est s':>7}  {'osc':>3}  "
            f"{'pp fin °':>8}  {'v fin m/s':>9}  {'motor rms':>9}  {'máx':>4}  {'rampa %':>7}"
        )
        for rc in resultados:
            m = rc.metricas[i]
            cae = f"sí {m.t_caida_s:.2f}s" if m.cae else "no"
            lineas.append(
                f"  {rc.nombre:<{ancho}}  {cae:>8}  {m.angulo_max_deg:7.1f}  {_f(m.t_establecimiento_s):>7}  "
                f"{m.oscilaciones:3d}  {_f(m.amplitud_final_pp_deg, '.1f'):>8}  {_f(m.velocidad_final_m_s):>9}  "
                f"{m.motor_rms:9.0f}  {m.motor_max:4d}  {m.pct_rampa_saturada:7.1f}"
            )
    lineas.append("\n▶ Ranking (caídas, sin establecerse, Σ t_est, Σ θmáx) y comando para el robot")
    for pos, rc in enumerate(resultados, 1):
        p = rc.puntaje()
        lineas.append(
            f"  {pos}. {rc.nombre:<{ancho}}  caídas={p[0]} sin_est={p[1]} Σt={p[2]:.2f}s Σθ={p[3]:.0f}°  "
            f"→ {rc.ganancias.comando_ws()}"
        )
    mejor = resultados[0]
    if mejor.caidas == 0:
        lineas.append(f"\nMejor candidato: {mejor.nombre}. Para cargarlo, mandá por WebSocket:\n  {mejor.ganancias.comando_ws()}")
    else:
        lineas.append("\nNingún candidato pasó todos los escenarios sin caerse.")
    return "\n".join(lineas)


def graficar(resultados: Sequence[ResultadoCandidato], archivo: str) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    escenarios = [r.escenario for r in resultados[0].resultados]
    fig, ejes = plt.subplots(len(escenarios), 1, figsize=(10, 2.6 * len(escenarios)), sharex=False, squeeze=False)
    for i, esc in enumerate(escenarios):
        ax = ejes[i][0]
        for rc in resultados:
            r = rc.resultados[i]
            ax.plot(r.t, r.theta_deg, lw=1.2, label=rc.nombre + (" (cae)" if r.cayo else ""))
        ax.axhline(0, color="0.6", lw=0.6)
        ax.set_title(esc.nombre, fontsize=10, loc="left")
        ax.set_ylabel("θ [°]")
        ax.set_ylim(-40, 40)
        ax.grid(alpha=0.3)
    ejes[-1][0].set_xlabel("t [s]")
    ejes[0][0].legend(fontsize=8, ncol=3, loc="upper right")
    fig.tight_layout()
    fig.savefig(archivo, dpi=120)
    plt.close(fig)


def _parsear_candidato(texto: str, i: int) -> Tuple[str, GananciasPID]:
    if "=" in texto:
        nombre, valores = texto.split("=", 1)
    else:
        nombre, valores = f"g{i}", texto
    return nombre.strip(), GananciasPID.desde_texto(valores)


def main(argv: Optional[Sequence[str]] = None) -> int:
    ap = argparse.ArgumentParser(prog="python -m gemelo.banco", description=__doc__.split("\n\n")[0])
    ap.add_argument("-g", "--ganancias", action="append", metavar="[NOMBRE=]KP,KD,KPT,KIT",
                    help="candidato a probar (se puede repetir; también acepta 'P:kp:kd:kpt:kit')")
    ap.add_argument("--sin-firmware", action="store_true", help="no agregar las ganancias de defines.h como referencia")
    ap.add_argument("--ruido", action="store_true", help="ruido típico de sensor (gyro 0.3 °/s, accel 1°)")
    ap.add_argument("--semilla", type=int, default=1)
    ap.add_argument("--modelo", choices=mod.MODELOS, default=mod.MODELO_LAGRANGE)
    ap.add_argument("--signo-motor", type=int, choices=(-1, 1), default=-1)
    ap.add_argument("--periodo-pi", type=int, default=1, help="ciclos entre llamadas al PI (firmware: 1)")
    ap.add_argument("--empujon", type=float, action="append", metavar="GRADOS", help="escenario de empujón inicial")
    ap.add_argument("--joystick", type=float, action="append", metavar="VALOR", help="escenario de escalón de joystick")
    ap.add_argument("--empuje", type=float, action="append", metavar="NEWTON", help="escenario de empuje de 100 ms")
    ap.add_argument("--grafico", metavar="ARCHIVO.png", help="guardar θ(t) de todos los candidatos")
    args = ap.parse_args(argv)

    from .simulador import empujon_inicial, escalon_joystick, perturbacion

    if args.ganancias:
        candidatos = [_parsear_candidato(t, i) for i, t in enumerate(args.ganancias, 1)]
        if not args.sin_firmware:
            candidatos.insert(0, ("firmware", GananciasPID()))
    else:
        candidatos = list(CANDIDATOS_EJEMPLO)

    escenarios: List[Escenario] = []
    escenarios += [empujon_inicial(g) for g in args.empujon or []]
    escenarios += [escalon_joystick(v) for v in args.joystick or []]
    escenarios += [perturbacion(f) for f in args.empuje or []]
    if not escenarios:
        escenarios = list(escenarios_por_defecto())

    cfg = ConfigSimulacion(
        modelo=args.modelo,
        signo_motor=args.signo_motor,
        periodo_pi_ciclos=args.periodo_pi,
        ruido_gyro_dps=RUIDO_GYRO_TIPICO_DPS if args.ruido else 0.0,
        ruido_accel_deg=RUIDO_ACCEL_TIPICO_DEG if args.ruido else 0.0,
        semilla=args.semilla,
    )
    p = ParametrosFisicos()
    print(f"Modelo: {args.modelo} · signo_motor={args.signo_motor} · PI cada {args.periodo_pi} ciclo(s) · "
          f"ruido={'sí' if args.ruido else 'no'}")
    print(f"Parámetros (provisorios): {p}")
    print(f"Polo inestable a lazo abierto: {mod.polo_inestable(p, args.modelo):.2f} 1/s")

    resultados = evaluar(candidatos, escenarios, p, cfg)
    print(tabla(resultados))
    if args.grafico:
        graficar(resultados, args.grafico)
        print(f"\nGráfico guardado en {args.grafico}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
