"""Cálculos para medir los parámetros físicos del robot real (ver MEDICION.md).

    python -m gemelo.medicion --masa-total 0.70 --masa-rueda 0.05 --d 0.08 --tiempo 13.4 --oscilaciones 20
"""

from __future__ import annotations

import argparse
import math
import sys
from typing import Optional, Sequence


def periodo(tiempo_total_s: float, oscilaciones: int) -> float:
    """Período medio a partir del tiempo de N oscilaciones completas (ida y vuelta)."""
    return tiempo_total_s / oscilaciones


def inercia_pivote(masa_kg: float, d_m: float, periodo_s: float, g: float = 9.81) -> float:
    """Péndulo físico: I_pivote = m·g·d·T² / (4π²)  (oscilaciones chicas, < 10°)."""
    return masa_kg * g * d_m * periodo_s**2 / (4 * math.pi**2)


def inercia_centro_masa(masa_kg: float, d_m: float, periodo_s: float, g: float = 9.81) -> float:
    """Steiner: I_c = I_pivote − m·d²."""
    return inercia_pivote(masa_kg, d_m, periodo_s, g) - masa_kg * d_m**2


def main(argv: Optional[Sequence[str]] = None) -> int:
    ap = argparse.ArgumentParser(prog="python -m gemelo.medicion", description="Parámetros físicos a partir de mediciones")
    ap.add_argument("--masa-total", type=float, required=True, help="robot completo [kg]")
    ap.add_argument("--masa-rueda", type=float, required=True, help="una rueda [kg]")
    ap.add_argument("--d", type=float, required=True, help="distancia pivote–centro de masa [m]")
    ap.add_argument("--tiempo", type=float, required=True, help="tiempo de N oscilaciones [s]")
    ap.add_argument("--oscilaciones", type=int, required=True, help="N oscilaciones completas")
    args = ap.parse_args(argv)

    m_cuerpo = args.masa_total - 2 * args.masa_rueda
    m_pendulo = m_cuerpo  # el péndulo se mide sin ruedas (MEDICION.md)
    T = periodo(args.tiempo, args.oscilaciones)
    i_piv = inercia_pivote(m_pendulo, args.d, T)
    i_c = inercia_centro_masa(m_pendulo, args.d, T)
    print(f"masa_cuerpo_kg       = {m_cuerpo:.4f}")
    print(f"período              = {T:.4f} s")
    print(f"I_pivote             = {i_piv:.6f} kg·m²")
    print(f"inercia_cuerpo_kg_m2 = {i_c:.6f} kg·m²  (Steiner: I_pivote − m·d²)")
    if i_c <= 0:
        print("¡I_c ≤ 0! Revisá d y el período (d mal medido o oscilaciones demasiado grandes).")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
