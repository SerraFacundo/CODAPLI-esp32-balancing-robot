"""Simulador en lazo cerrado: planta (modelo.py) + sensor MPU6050 + control del firmware.

Secuencia de cada ciclo de 10 ms (igual que loop() cuando llega un dato del MPU6050):

  1. El sensor mide el estado físico: giróscopo (θ̇) y ángulo del acelerómetro
     (θ − atan(a/g), porque el acelerómetro también siente la aceleración de la base),
     con el filtro pasabajos interno del MPU6050 (DLPF de 10 Hz) y ruido opcional.
  2. ControlFirmware calcula la nueva velocidad de los motores.
  3. El motor cambia de velocidad de golpe → salto de θ̇ (modelo.salto_velocidad_angular).
  4. Se integra la dinámica hasta el ciclo siguiente con RK4 (motor a velocidad constante).

Convención de signos: θ_física > 0 = inclinado hacia +x. El firmware ve el mismo ángulo
y responde con motor NEGATIVO cuando θ > 0, así que para que el robot se equilibre la
velocidad positiva del motor tiene que llevar la rueda hacia −x: signo_motor = −1.
Esto no está confirmado contra el robot real; con signo_motor = +1 el lazo es inestable
(sirve para ver qué pasaría si el signo estuviera al revés).
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Iterator, List, Optional, Sequence, Tuple

import numpy as np

from . import modelo as mod
from .firmware import ANGULO_CORTE_MOTORES, ControlFirmware, pasos_por_segundo_reales
from .modelo import GananciasPID, ParametrosFisicos


@dataclass
class ConfigSimulacion:
    dt_s: float = mod.DT_CONTROL_S
    subpasos: int = 10  # pasos de RK4 por ciclo de control
    modelo: str = mod.MODELO_LAGRANGE
    signo_motor: int = -1
    periodo_pi_ciclos: int = 1  # el firmware real corre el PI en todos los ciclos
    modo_pro: bool = False
    dlpf_hz: Optional[float] = 10.0  # MPU6050_DLPF_10HZ; None = sin filtro del sensor
    ruido_gyro_dps: float = 0.0  # desvío estándar del ruido del giróscopo [°/s]
    ruido_accel_deg: float = 0.0  # desvío estándar del ruido del ángulo del acelerómetro [°]
    semilla: Optional[int] = None


# Ruido "realista" para el banco: vibración de los paso a paso incluida (estimado, ajustable)
RUIDO_GYRO_TIPICO_DPS = 0.3
RUIDO_ACCEL_TIPICO_DEG = 1.0


@dataclass
class Escenario:
    """Condiciones de una simulación.

    joystick: lista de (t_inicio_s, t_fin_s, valor en [-1, 1]).
    empujes:  lista de (t_inicio_s, duración_s, fuerza_N) horizontales en el centro de masa.
    """

    nombre: str
    duracion_s: float = 5.0
    angulo_inicial_deg: float = 0.0
    velocidad_angular_inicial_dps: float = 0.0
    joystick: List[Tuple[float, float, float]] = field(default_factory=list)
    empujes: List[Tuple[float, float, float]] = field(default_factory=list)
    t_evento_s: float = 0.0  # desde cuándo se miden las métricas de respuesta

    def joystick_en(self, t: float) -> float:
        for t0, t1, valor in self.joystick:
            if t0 <= t < t1:
                return valor
        return 0.0

    def fuerza_en(self, t: float) -> float:
        return sum(f for t0, dur, f in self.empujes if t0 <= t < t0 + dur)


def empujon_inicial(grados: float, duracion_s: float = 5.0) -> Escenario:
    """El robot se suelta inclinado `grados` desde el reposo."""
    return Escenario(f"empujón {grados:g}°", duracion_s, angulo_inicial_deg=grados)


def escalon_joystick(valor: float, t_inicio_s: float = 1.0, duracion_s: float = 6.0) -> Escenario:
    """Escalón de joystick: en t_inicio se lleva a `valor` y se mantiene hasta el final."""
    return Escenario(
        f"joystick 0 → {valor:g} en t={t_inicio_s:g} s",
        duracion_s,
        joystick=[(t_inicio_s, math.inf, valor)],
        t_evento_s=t_inicio_s,
    )


def perturbacion(fuerza_n: float, duracion_pulso_s: float = 0.1, t_s: float = 1.0, duracion_s: float = 5.0) -> Escenario:
    """Empujón horizontal de `fuerza_n` durante `duracion_pulso_s` aplicado en el CM."""
    return Escenario(
        f"empuje {fuerza_n:g} N × {duracion_pulso_s * 1000:g} ms",
        duracion_s,
        empujes=[(t_s, duracion_pulso_s, fuerza_n)],
        t_evento_s=t_s,
    )


@dataclass
class ResultadoSimulacion:
    escenario: Escenario
    ganancias: GananciasPID
    t: np.ndarray  # [s] instante de cada ciclo de control
    theta_deg: np.ndarray  # ángulo físico real
    angulo_firmware_deg: np.ndarray  # angle_adjusted (lo que estima el filtro)
    omega_dps: np.ndarray
    x_m: np.ndarray
    v_m_s: np.ndarray
    speed_m: np.ndarray  # velocidad de motor en unidades del firmware
    actual_robot_speed: np.ndarray  # lo que manda la telemetría A:
    control_output: np.ndarray
    target_angle: np.ndarray
    throttle: np.ndarray
    motores_activos: np.ndarray
    rampa_saturada: np.ndarray
    pi_corrio: np.ndarray
    cayo: bool
    t_caida_s: Optional[float]

    def mensajes_telemetria(self, cada_ciclos: int = 10) -> Iterator[Tuple[float, str]]:
        """(t, "A:<angulo>:<velocidad>:<motores>") a 10 Hz, como sendTelemetry()."""
        for i in range(0, len(self.t), cada_ciclos):
            yield self.t[i], (
                f"A:{self.angulo_firmware_deg[i]:.1f}:{int(self.actual_robot_speed[i])}:"
                f"{int(self.motores_activos[i])}"
            )


class SensorMPU6050:
    """Giróscopo + ángulo del acelerómetro con DLPF de primer orden y ruido gaussiano."""

    def __init__(self, config: ConfigSimulacion, theta0_deg: float):
        self.cfg = config
        self.rng = np.random.default_rng(config.semilla)
        if config.dlpf_hz:
            self.alfa = 1.0 - math.exp(-2 * math.pi * config.dlpf_hz * config.dt_s)
        else:
            self.alfa = 1.0
        self.gyro = 0.0
        self.accel = theta0_deg

    def medir(self, theta_rad: float, omega_rad_s: float, aceleracion_m_s2: float, g: float) -> Tuple[float, float]:
        gyro_real = math.degrees(omega_rad_s)
        accel_real = math.degrees(theta_rad - math.atan2(aceleracion_m_s2, g))
        self.gyro += self.alfa * (gyro_real - self.gyro)
        self.accel += self.alfa * (accel_real - self.accel)
        gyro, accel = self.gyro, self.accel
        if self.cfg.ruido_gyro_dps:
            gyro += self.rng.normal(0.0, self.cfg.ruido_gyro_dps)
        if self.cfg.ruido_accel_deg:
            accel += self.rng.normal(0.0, self.cfg.ruido_accel_deg)
        return gyro, accel


def simular(
    ganancias: Optional[GananciasPID] = None,
    escenario: Optional[Escenario] = None,
    parametros: Optional[ParametrosFisicos] = None,
    config: Optional[ConfigSimulacion] = None,
) -> ResultadoSimulacion:
    ganancias = ganancias or GananciasPID()
    escenario = escenario or empujon_inicial(5.0)
    parametros = parametros or ParametrosFisicos()
    cfg = config or ConfigSimulacion()
    c = parametros.coeficientes()
    dt, h = cfg.dt_s, cfg.dt_s / cfg.subpasos
    rad_s_por_paso = mod.RAD_POR_PASO

    theta = math.radians(escenario.angulo_inicial_deg)
    omega = math.radians(escenario.velocidad_angular_inicial_dps)
    x, u = 0.0, 0.0  # posición del eje [m] y velocidad del motor relativa al cuerpo [rad/s]
    v_ant = mod.velocidad_lineal(c, omega, u, cfg.modelo)

    sensor = SensorMPU6050(cfg, escenario.angulo_inicial_deg)
    control = ControlFirmware(
        ganancias,
        angulo_inicial_deg=escenario.angulo_inicial_deg,
        modo_pro=cfg.modo_pro,
        periodo_pi_ciclos=cfg.periodo_pi_ciclos,
    )

    def derivadas(th: float, om: float, f: float) -> Tuple[float, float]:
        return om, mod.aceleracion_angular(c, th, om, 0.0, f, cfg.modelo)

    n = int(round(escenario.duracion_s / dt))
    reg = {k: [] for k in (
        "t", "theta", "ang_fw", "omega", "x", "v", "speed_m", "ars", "co", "ta", "thr", "mot", "rampa", "pi")}
    cayo, t_caida = False, None

    for k in range(n):
        t = k * dt
        # 1. Sensor (la aceleración de la base se toma como el promedio del último ciclo)
        v = mod.velocidad_lineal(c, omega, u, cfg.modelo)
        gyro, accel = sensor.medir(theta, omega, (v - v_ant) / dt, c.g)
        v_ant = v

        # 2. Control del firmware
        e = control.ciclo_control(gyro, accel, escenario.joystick_en(t), dt)

        reg["t"].append(t)
        reg["theta"].append(math.degrees(theta))
        reg["ang_fw"].append(e.angle_adjusted)
        reg["omega"].append(math.degrees(omega))
        reg["x"].append(x)
        reg["v"].append(v)
        reg["speed_m"].append(e.speed_m)
        reg["ars"].append(e.actual_robot_speed)
        reg["co"].append(e.control_output)
        reg["ta"].append(e.target_angle)
        reg["thr"].append(e.throttle)
        reg["mot"].append(e.motores_activos)
        reg["rampa"].append(e.rampa_saturada)
        reg["pi"].append(e.corrio_pi)

        if not e.motores_activos:
            # El firmware cortó los motores (|ángulo| >= 74°): caída
            cayo, t_caida = True, t
            break

        # 3. Nueva velocidad del motor → salto de θ̇
        u_nueva = cfg.signo_motor * pasos_por_segundo_reales(e.speed_m) * rad_s_por_paso
        omega += mod.salto_velocidad_angular(c, theta, u_nueva - u, cfg.modelo)
        u = u_nueva

        # 4. Integración RK4 con el motor a velocidad constante hasta el próximo ciclo
        for j in range(cfg.subpasos):
            ts = t + j * h
            f1 = escenario.fuerza_en(ts)
            f2 = escenario.fuerza_en(ts + h / 2)
            k1 = derivadas(theta, omega, f1)
            k2 = derivadas(theta + h / 2 * k1[0], omega + h / 2 * k1[1], f2)
            k3 = derivadas(theta + h / 2 * k2[0], omega + h / 2 * k2[1], f2)
            k4 = derivadas(theta + h * k3[0], omega + h * k3[1], escenario.fuerza_en(ts + h))
            # la posición avanza con la velocidad lineal (u constante en el subpaso)
            x += h / 6 * sum(
                w * mod.velocidad_lineal(c, kk[0], u, cfg.modelo)
                for w, kk in ((1, k1), (2, k2), (2, k3), (1, k4))
            )
            theta += h / 6 * (k1[0] + 2 * k2[0] + 2 * k3[0] + k4[0])
            omega += h / 6 * (k1[1] + 2 * k2[1] + 2 * k3[1] + k4[1])

        if abs(theta) > math.radians(90):
            # el cuerpo tocó el piso antes de que el filtro llegara a 74° (no debería pasar)
            cayo, t_caida = True, t + dt
            break

    a = {k: np.asarray(vs) for k, vs in reg.items()}
    return ResultadoSimulacion(
        escenario=escenario,
        ganancias=ganancias,
        t=a["t"],
        theta_deg=a["theta"],
        angulo_firmware_deg=a["ang_fw"],
        omega_dps=a["omega"],
        x_m=a["x"],
        v_m_s=a["v"],
        speed_m=a["speed_m"],
        actual_robot_speed=a["ars"],
        control_output=a["co"],
        target_angle=a["ta"],
        throttle=a["thr"],
        motores_activos=a["mot"].astype(bool),
        rampa_saturada=a["rampa"].astype(bool),
        pi_corrio=a["pi"].astype(bool),
        cayo=cayo,
        t_caida_s=t_caida,
    )


def escenarios_por_defecto() -> Sequence[Escenario]:
    return (
        empujon_inicial(5.0),
        empujon_inicial(15.0),
        escalon_joystick(0.6),
        perturbacion(3.0, 0.1),
    )


__all__ = [
    "ANGULO_CORTE_MOTORES",
    "ConfigSimulacion",
    "Escenario",
    "ResultadoSimulacion",
    "RUIDO_ACCEL_TIPICO_DEG",
    "RUIDO_GYRO_TIPICO_DPS",
    "empujon_inicial",
    "escalon_joystick",
    "escenarios_por_defecto",
    "perturbacion",
    "simular",
]
