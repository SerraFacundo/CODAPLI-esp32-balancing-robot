"""Modelo dinámico del robot (péndulo invertido de dos ruedas con motores paso a paso).

Los parámetros físicos son PROVISORIOS hasta medirlos en el robot real
(ver gemelo-digital/MEDICION.md). Las ganancias por defecto replican las de
esp32_balancing_robot/defines.h.

Dos modelos de la planta
------------------------
Coordenadas: θ = inclinación del cuerpo (rad, positiva hacia +x), φ_rel = giro de la
rueda RELATIVO al cuerpo (es lo que fija el paso a paso, porque el estator va abulonado
al cuerpo) y u = dφ_rel/dt = velocidad que impone el motor. La rueda gira respecto del
piso φ = φ_rel + θ y rueda sin deslizar: x = r·φ.

* MODELO_LAGRANGE (por defecto). Con un solo grado de libertad (θ), porque u está
  impuesta por el motor, la ecuación de Lagrange queda:

      M(θ)·θ̈ = m·g·l·sinθ + B·sinθ·θ̇² − (A + B·cosθ)·u̇ + Q

      A = (m + 2·m_r)·r² + 2·I_r   (traslación del conjunto + giro de las 2 ruedas)
      B = m·r·l
      C = I_c + m·l²               (cuerpo respecto del eje de las ruedas)
      M(θ) = A + 2·B·cosθ + C
      Q = fuerza externa horizontal aplicada en el centro de masa · (r + l·cosθ)

  Incluye la reacción del motor sobre el cuerpo y la masa/inercia de las ruedas.

* MODELO_BASE_IMPUESTA. La ecuación del enunciado, con la aceleración de la base como
  entrada (como un carro que alguien empuja):

      (I_c + m·l²)·θ̈ = m·g·l·sinθ − m·l·a·cosθ + F·l·cosθ,    a = r·u̇

Linealizados (θ chico, sin fuerza externa):

    Lagrange:       θ̈ = (m·g·l·θ − (A + B)·u̇) / (A + 2B + C)
    Base impuesta:  θ̈ = m·l/(I_c + m·l²) · (g·θ − a)

Los motores cambian de velocidad de golpe al comienzo de cada ciclo de control
(el firmware reescribe el período del timer), así que u̇ es un impulso: se aplica como
un salto de θ̇ que conserva el momento generalizado (ver salto_velocidad_angular).
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Optional

# ---------------------------------------------------------------------------
# Constantes del firmware (esp32_balancing_robot/defines.h, Motors.cpp)
# ---------------------------------------------------------------------------
FRECUENCIA_CONTROL_HZ = 100
DT_CONTROL_S = 1.0 / FRECUENCIA_CONTROL_HZ

PASOS_POR_VUELTA_MOTOR = 200
MICROPASOS = 8
PASOS_POR_VUELTA = PASOS_POR_VUELTA_MOTOR * MICROPASOS  # 1600 micropasos por vuelta
PASOS_POR_UNIDAD = 25  # Motors.cpp: speed = speed_M * 25 (1/8 de micropaso)
FRECUENCIA_TIMER_HZ = 2_000_000  # timer a 2 MHz: período = 2e6 / pasos_por_segundo

# 1 unidad de velocidad del firmware = 25 pasos/s = 25/1600 vueltas/s
GRADOS_S_POR_UNIDAD = PASOS_POR_UNIDAD * 360.0 / PASOS_POR_VUELTA  # 5.625 °/s
RAD_POR_PASO = 2.0 * math.pi / PASOS_POR_VUELTA

MODELO_LAGRANGE = "lagrange"
MODELO_BASE_IMPUESTA = "base_impuesta"
MODELOS = (MODELO_LAGRANGE, MODELO_BASE_IMPUESTA)


@dataclass
class ParametrosFisicos:
    # TODO: medir en el robot real (ver MEDICION.md)
    masa_cuerpo_kg: float = 0.6  # todo menos las dos ruedas (incluye batería y motores)
    masa_rueda_kg: float = 0.05  # cada rueda
    radio_rueda_m: float = 0.045
    altura_cm_m: float = 0.08  # distancia del eje de ruedas al centro de masa del cuerpo
    # Inercia del cuerpo respecto de su centro de masa (eje paralelo al de las ruedas).
    # Provisoria: prisma de 0.6 kg y ~0.17 m de alto → m·h²/12 ≈ 0.0015 kg·m².
    inercia_cuerpo_kg_m2: float = 0.0015
    # Inercia de cada rueda respecto de su eje. None = disco macizo (½·m·r²).
    inercia_rueda_kg_m2: Optional[float] = None
    g: float = 9.81

    @property
    def inercia_rueda(self) -> float:
        if self.inercia_rueda_kg_m2 is not None:
            return self.inercia_rueda_kg_m2
        return 0.5 * self.masa_rueda_kg * self.radio_rueda_m**2

    def coeficientes(self) -> "Coeficientes":
        m, l, r = self.masa_cuerpo_kg, self.altura_cm_m, self.radio_rueda_m
        return Coeficientes(
            A=(m + 2 * self.masa_rueda_kg) * r**2 + 2 * self.inercia_rueda,
            B=m * r * l,
            C=self.inercia_cuerpo_kg_m2 + m * l**2,
            mgl=m * self.g * l,
            m=m,
            l=l,
            r=r,
            g=self.g,
        )


@dataclass(frozen=True)
class Coeficientes:
    """Coeficientes de la ecuación de movimiento (unidades SI: kg·m², N·m)."""

    A: float
    B: float
    C: float
    mgl: float
    m: float
    l: float
    r: float
    g: float


def _redondear(valor: float) -> str:
    # 6 cifras significativas evitan cosas como 0.16000000000000003 en el comando
    return f"{valor:.6g}"


@dataclass
class GananciasPID:
    """Mismos nombres y valores por defecto que esp32_balancing_robot/defines.h."""

    kp: float = 0.32  # KP — estabilidad (PD)
    kd: float = 0.050  # KD
    kp_throttle: float = 0.080  # KP_THROTTLE — velocidad (PI)
    ki_throttle: float = 0.1  # KI_THROTTLE

    def comando_ws(self) -> str:
        """Mensaje `P:` para enviar estas ganancias al robot por WebSocket."""
        valores = (self.kp, self.kd, self.kp_throttle, self.ki_throttle)
        return "P:" + ":".join(_redondear(v) for v in valores)

    @classmethod
    def desde_texto(cls, texto: str) -> "GananciasPID":
        """Acepta "kp,kd,kpt,kit" o un comando "P:kp:kd:kpt:kit"."""
        texto = texto.strip()
        if texto.upper().startswith("P:"):
            partes = texto[2:].split(":")
        else:
            partes = texto.replace(";", ",").split(",")
        if len(partes) != 4:
            raise ValueError(f"se esperaban 4 ganancias (kp, kd, kpt, kit): {texto!r}")
        return cls(*(float(p) for p in partes))


# ---------------------------------------------------------------------------
# Dinámica
# ---------------------------------------------------------------------------
def aceleracion_angular(
    c: Coeficientes,
    theta: float,
    omega: float,
    u_punto: float = 0.0,
    fuerza_n: float = 0.0,
    modelo: str = MODELO_LAGRANGE,
) -> float:
    """θ̈ [rad/s²] dado θ [rad], θ̇ [rad/s], u̇ [rad/s²] y una fuerza horizontal en el CM [N]."""
    s, co = math.sin(theta), math.cos(theta)
    if modelo == MODELO_LAGRANGE:
        masa_gen = c.A + 2 * c.B * co + c.C
        q = fuerza_n * (c.r + c.l * co)
        return (c.mgl * s + c.B * s * omega**2 - (c.A + c.B * co) * u_punto + q) / masa_gen
    if modelo == MODELO_BASE_IMPUESTA:
        a = c.r * u_punto
        return (c.mgl * s - c.m * c.l * a * co + fuerza_n * c.l * co) / c.C
    raise ValueError(f"modelo desconocido: {modelo!r} (opciones: {MODELOS})")


def salto_velocidad_angular(
    c: Coeficientes, theta: float, delta_u: float, modelo: str = MODELO_LAGRANGE
) -> float:
    """Δθ̇ [rad/s] cuando la velocidad del motor cambia de golpe en Δu [rad/s].

    Sale de integrar la ecuación de movimiento durante el instante del salto (u̇ impulsivo):
    se conserva el momento generalizado ∂L/∂θ̇ = (A + B·cosθ)·u + M(θ)·θ̇.
    """
    co = math.cos(theta)
    if modelo == MODELO_LAGRANGE:
        return -(c.A + c.B * co) * delta_u / (c.A + 2 * c.B * co + c.C)
    if modelo == MODELO_BASE_IMPUESTA:
        return -c.m * c.l * c.r * co * delta_u / c.C
    raise ValueError(f"modelo desconocido: {modelo!r} (opciones: {MODELOS})")


def velocidad_lineal(c: Coeficientes, omega: float, u: float, modelo: str = MODELO_LAGRANGE) -> float:
    """Velocidad del eje de las ruedas respecto del piso [m/s]."""
    if modelo == MODELO_LAGRANGE:
        return c.r * (u + omega)  # la rueda gira respecto del piso u + θ̇
    return c.r * u  # base impuesta: el motor fija directamente la velocidad de la base


def energia_libre(c: Coeficientes, theta: float, omega: float, modelo: str = MODELO_LAGRANGE) -> float:
    """Energía mecánica [J] con el motor quieto (u = 0); se conserva sin fuerzas externas."""
    co = math.cos(theta)
    if modelo == MODELO_LAGRANGE:
        return 0.5 * (c.A + 2 * c.B * co + c.C) * omega**2 + c.mgl * co
    return 0.5 * c.C * omega**2 + c.mgl * co


def polo_inestable(p: ParametrosFisicos, modelo: str = MODELO_LAGRANGE) -> float:
    """Polo inestable del modelo linealizado a lazo abierto [1/s] (θ crece como e^{λt})."""
    c = p.coeficientes()
    if modelo == MODELO_LAGRANGE:
        return math.sqrt(c.mgl / (c.A + 2 * c.B + c.C))
    return math.sqrt(c.mgl / c.C)


def unidades_a_rad_s(velocidad_firmware: float) -> float:
    """Velocidad de motor del firmware (speed_M) → velocidad angular de la rueda [rad/s]."""
    return velocidad_firmware * PASOS_POR_UNIDAD * RAD_POR_PASO


def unidades_a_m_s(velocidad_firmware: float, p: Optional[ParametrosFisicos] = None) -> float:
    """Velocidad de motor del firmware → velocidad lineal de la rueda [m/s]."""
    p = p or ParametrosFisicos()
    return unidades_a_rad_s(velocidad_firmware) * p.radio_rueda_m
