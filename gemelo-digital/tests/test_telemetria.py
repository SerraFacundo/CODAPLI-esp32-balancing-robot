from gemelo.modelo import GananciasPID
from gemelo.telemetria import Caida, Estado, Telemetria, parsear


def test_parsea_telemetria():
    assert parsear("A:-3.5:120:1") == Telemetria(-3.5, 120, True)


def test_parsea_estado_con_y_sin_bateria():
    assert parsear("E:120:3:95:7400:-61:180000:2") == Estado(120, 3, 95, 7400, -61, 180000, 2)
    sin_medicion = parsear("E:120:3:95:-1:0:180000:2")
    assert sin_medicion.bateria_mv is None and sin_medicion.rssi_dbm is None


def test_parsea_caida():
    assert parsear("F:4:-75.3:12345") == Caida(4, -75.3, 12345)


def test_ignora_mensajes_invalidos():
    assert parsear("J:0.1:0.2") is None
    assert parsear("A:abc:1:1") is None
    assert parsear("A:1.0:2") is None
    assert parsear("E:1:2:3") is None
    assert parsear("T:OK") is None
    assert parsear("") is None


def test_comando_pid_por_defecto():
    assert GananciasPID().comando_ws() == "P:0.32:0.05:0.08:0.1"
