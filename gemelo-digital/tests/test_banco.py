"""Banco de pruebas de ganancias."""

import pytest

from gemelo import banco
from gemelo.modelo import GananciasPID
from gemelo.simulador import empujon_inicial, perturbacion


def test_banco_ordena_y_detecta_caidas():
    res = banco.evaluar(
        [("absurda", GananciasPID(0.02, 0.0, 0.08, 0.1)), ("buena", GananciasPID(0.45, 0.08, 0.08, 0.05))],
        [empujon_inicial(5.0, 4.0), perturbacion(2.0, 0.1, duracion_s=4.0)],
    )
    assert [r.nombre for r in res] == ["buena", "absurda"]
    assert res[0].caidas == 0 and res[1].caidas == 2
    m = res[0].metricas[0]
    assert m.t_establecimiento_s is not None and m.angulo_max_deg == pytest.approx(5.0, abs=0.2)
    assert m.motor_max > 0 and 0 <= m.pct_rampa_saturada <= 100


def test_banco_cli_muestra_comando(capsys):
    assert banco.main(["-g", "mia=0.45,0.08,0.08,0.05", "--empujon", "5"]) == 0
    salida = capsys.readouterr().out
    assert "P:0.45:0.08:0.08:0.05" in salida and "firmware" in salida
