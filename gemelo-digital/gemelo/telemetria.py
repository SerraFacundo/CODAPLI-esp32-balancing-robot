"""Parseo de la telemetría que envía el robot por WebSocket.

Formato actual del firmware (sendTelemetry, 10 Hz):
    A:<angulo>:<velocidad>:<motores_activos>
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class Telemetria:
    angulo_deg: float
    velocidad: int
    motores_activos: bool


def parsear(mensaje: str) -> Telemetria | None:
    """Devuelve la telemetría del mensaje, o None si no es un mensaje `A:` válido."""
    partes = mensaje.strip().split(":")
    if len(partes) != 4 or partes[0] != "A":
        return None
    try:
        return Telemetria(
            angulo_deg=float(partes[1]),
            velocidad=int(partes[2]),
            motores_activos=partes[3] == "1",
        )
    except ValueError:
        return None
