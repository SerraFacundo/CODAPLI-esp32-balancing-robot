"""Parseo de los mensajes que envía el robot por WebSocket.

Protocolo completo: docs/ind40/protocolo-websocket.md

    A:<angulo>:<velocidad>:<motores_activos>                                  (10 Hz)
    E:<uptime_s>:<caidas>:<de_pie_s>:<bateria_mV>:<rssi>:<heap>:<clientes>    (1 Hz)
    F:<numero>:<angulo>:<tramo_ms>                                            (al caer)
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Optional, Union


@dataclass(frozen=True)
class Telemetria:
    angulo_deg: float
    velocidad: int
    motores_activos: bool


@dataclass(frozen=True)
class Estado:
    uptime_s: int
    caidas: int
    de_pie_s: int
    bateria_mv: Optional[int]  # None si el robot no mide batería (envía -1)
    rssi_dbm: Optional[int]    # None si la STA no está conectada (envía 0)
    heap_libre: int
    clientes: int


@dataclass(frozen=True)
class Caida:
    numero: int
    angulo_deg: float
    tramo_ms: int  # cuánto tiempo estuvo de pie antes de caer


Mensaje = Union[Telemetria, Estado, Caida]


def parsear(mensaje: str) -> Optional[Mensaje]:
    """Devuelve el mensaje parseado, o None si no es un mensaje de telemetría válido."""
    partes = mensaje.strip().split(":")
    try:
        if partes[0] == "A" and len(partes) == 4:
            return Telemetria(
                angulo_deg=float(partes[1]),
                velocidad=int(partes[2]),
                motores_activos=partes[3] == "1",
            )
        if partes[0] == "E" and len(partes) == 8:
            bateria, rssi = int(partes[4]), int(partes[5])
            return Estado(
                uptime_s=int(partes[1]),
                caidas=int(partes[2]),
                de_pie_s=int(partes[3]),
                bateria_mv=bateria if bateria >= 0 else None,
                rssi_dbm=rssi if rssi != 0 else None,
                heap_libre=int(partes[6]),
                clientes=int(partes[7]),
            )
        if partes[0] == "F" and len(partes) == 4:
            return Caida(
                numero=int(partes[1]),
                angulo_deg=float(partes[2]),
                tramo_ms=int(partes[3]),
            )
    except ValueError:
        return None
    return None
