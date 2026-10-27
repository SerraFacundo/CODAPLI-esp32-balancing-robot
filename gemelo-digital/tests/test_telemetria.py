from gemelo.modelo import GananciasPID
from gemelo.telemetria import Telemetria, parsear


def test_parsea_mensaje_valido():
    assert parsear("A:-3.5:120:1") == Telemetria(-3.5, 120, True)


def test_ignora_mensajes_invalidos():
    assert parsear("J:0.1:0.2") is None
    assert parsear("A:abc:1:1") is None
    assert parsear("A:1.0:2") is None


def test_comando_pid_por_defecto():
    assert GananciasPID().comando_ws() == "P:0.32:0.05:0.08:0.1"
