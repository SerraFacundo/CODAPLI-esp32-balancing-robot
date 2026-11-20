"""Validación contra el CSV exportado de InfluxDB."""

import numpy as np
import pytest

from gemelo import validacion


def test_ejemplo_csv_ida_y_vuelta(tmp_path):
    ruta = tmp_path / "ejemplo.csv"
    validacion.generar_csv_ejemplo(str(ruta))
    tel = validacion.leer_csv_influx(str(ruta))
    assert len(tel) > 100
    assert np.all(np.diff(tel.t) > 0)
    assert tel.motores[0] == 0 and tel.angulo_deg[0] == -80.0
    tramos = validacion.tramos_de_pie(tel)
    assert len(tramos) == 1 and tramos[0][0] == pytest.approx(1.0)


CSV_ANOTADO_VARIAS_TABLAS = """\
#group,false,false,true,true,false,false,false,false
#datatype,string,long,dateTime:RFC3339,dateTime:RFC3339,dateTime:RFC3339,double,long,long
#default,_result,,,,,,,
,result,table,_start,_stop,_time,angulo,motores,velocidad
,,0,2026-11-12T18:00:00Z,2026-11-12T19:00:00Z,2026-11-12T18:30:00.1234567Z,1.5,1,10
,,0,2026-11-12T18:00:00Z,2026-11-12T19:00:00Z,2026-11-12T18:30:00.2Z,1.0,1,12

#group,false,false,true,true,false,false,false,false
#datatype,string,long,dateTime:RFC3339,dateTime:RFC3339,dateTime:RFC3339,double,long,long
#default,_result,,,,,,,
,result,table,_start,_stop,_time,angulo,motores,velocidad
,,1,2026-11-12T18:00:00Z,2026-11-12T19:00:00Z,2026-11-12T18:30:00Z,2.0,1,8
"""


def test_lee_csv_anotado_con_varias_tablas_y_utf16(tmp_path):
    for codif in ("utf-8", "utf-16"):
        ruta = tmp_path / f"t_{codif}.csv"
        ruta.write_text(CSV_ANOTADO_VARIAS_TABLAS, encoding=codif)
        tel = validacion.leer_csv_influx(str(ruta))
        assert tel.angulo_deg.tolist() == [2.0, 1.5, 1.0]  # ordenado por tiempo
        assert tel.t == pytest.approx([0.0, 0.123456, 0.2])
        assert tel.velocidad.tolist() == [8, 10, 12]


def test_lee_csv_simple(tmp_path):
    ruta = tmp_path / "simple.csv"
    ruta.write_text("_time,angulo,velocidad,motores\n2026-11-12T18:30:00Z,1.0,0,1\n2026-11-12T18:30:00.1Z,2.0,3,1\n")
    tel = validacion.leer_csv_influx(str(ruta))
    assert tel.angulo_deg.tolist() == [1.0, 2.0]


def test_comparacion_con_el_propio_gemelo_coincide_al_inicio(tmp_path):
    ruta = tmp_path / "ejemplo.csv"
    validacion.generar_csv_ejemplo(str(ruta))
    tel = validacion.leer_csv_influx(str(ruta))
    a, b = validacion.tramos_de_pie(tel)[0]
    comp = validacion.comparar(tel.recorte(a, b))
    # el "robot" de ejemplo es parecido al gemelo: el transitorio inicial coincide bastante
    assert comp.rmse_inicial_deg < 3.0
    assert comp.frec_real_hz == pytest.approx(comp.frec_sim_hz, abs=0.2)


def test_validacion_cli(tmp_path, capsys):
    ruta = tmp_path / "ejemplo.csv"
    assert validacion.main(["--generar-ejemplo", str(ruta), str(ruta)]) == 0
    assert "RMSE" in capsys.readouterr().out
    assert validacion.main(["--consulta"]) == 0
    assert 'from(bucket: "robot")' in capsys.readouterr().out


def test_archivo_flux_igual_a_la_consulta_del_modulo():
    from pathlib import Path

    texto = (Path(__file__).parent.parent / "datos" / "exportar_telemetria.flux").read_text(encoding="utf-8")
    sin_comentarios = "".join(l + "\n" for l in texto.splitlines() if not l.startswith("//"))
    assert sin_comentarios == validacion.CONSULTA_FLUX
