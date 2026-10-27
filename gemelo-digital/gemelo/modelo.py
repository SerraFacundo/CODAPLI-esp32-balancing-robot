"""Modelo dinámico del robot (péndulo invertido de dos ruedas).

Esqueleto: los parámetros físicos son provisorios y hay que medirlos en el robot real.
Las ganancias por defecto replican las de src/defines.h.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass
class ParametrosFisicos:
    # TODO: medir en el robot real
    masa_cuerpo_kg: float = 0.6
    masa_rueda_kg: float = 0.05
    radio_rueda_m: float = 0.045
    altura_cm_m: float = 0.08  # distancia del eje de ruedas al centro de masa
    g: float = 9.81


@dataclass
class GananciasPID:
    """Mismos nombres y valores por defecto que src/defines.h."""

    kp: float = 0.32  # KP — estabilidad (PD)
    kd: float = 0.050  # KD
    kp_throttle: float = 0.080  # KP_THROTTLE — velocidad (PI)
    ki_throttle: float = 0.1  # KI_THROTTLE

    def comando_ws(self) -> str:
        """Mensaje `P:` para enviar estas ganancias al robot por WebSocket."""
        return f"P:{self.kp}:{self.kd}:{self.kp_throttle}:{self.ki_throttle}"


FRECUENCIA_CONTROL_HZ = 100
