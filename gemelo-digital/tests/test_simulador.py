"""Control del firmware replicado y simulación en lazo cerrado."""

import numpy as np
import pytest

from gemelo.firmware import (
    MAX_ACCEL,
    ControlFirmware,
    pasos_por_segundo_reales,
    throttle_desde_joystick,
)
from gemelo.modelo import GananciasPID
from gemelo.simulador import (
    ConfigSimulacion,
    empujon_inicial,
    escalon_joystick,
    escenarios_por_defecto,
    perturbacion,
    simular,
)

ABSURDAS = [
    GananciasPID(kp=0.02, kd=0.0),  # casi sin control de estabilidad
    GananciasPID(kp=-0.32, kd=-0.05),  # signo al revés
    GananciasPID(kp=0.32, kd=0.05, kp_throttle=0.0, ki_throttle=0.0),  # sin lazo de velocidad: se escapa
]


# --- Firmware --------------------------------------------------------------
def test_pi_corre_en_todos_los_ciclos_como_el_firmware():
    c = ControlFirmware(GananciasPID())
    corridas = [c.ciclo_control(0.0, 0.0).corrio_pi for _ in range(20)]
    assert all(corridas) and c.llamadas_pi == 20


def test_pi_cada_4_ciclos():
    c = ControlFirmware(GananciasPID(), periodo_pi_ciclos=4)
    estados = [c.ciclo_control(0.0, 2.0, joystick=1.0) for _ in range(20)]
    assert [i for i, e in enumerate(estados) if e.corrio_pi] == [0, 4, 8, 12, 16]
    assert c.llamadas_pi == 5
    # entre llamadas el ángulo objetivo no cambia
    objetivos = [e.target_angle for e in estados]
    for i in range(20):
        if i % 4:
            assert objetivos[i] == objetivos[i - 1]


def test_pi_cada_4_en_lazo_cerrado():
    r = simular(escenario=empujon_inicial(5.0, 1.0), config=ConfigSimulacion(periodo_pi_ciclos=4))
    assert np.flatnonzero(r.pi_corrio).tolist() == list(range(0, len(r.t), 4))


def test_rampa_de_aceleracion_max_accel():
    c = ControlFirmware(GananciasPID())
    assert c.set_motor_speed(500) is True and c.speed_m == MAX_ACCEL
    c.set_motor_speed(500)
    assert c.speed_m == 2 * MAX_ACCEL
    assert c.set_motor_speed(2 * MAX_ACCEL + 5) is False and c.speed_m == 2 * MAX_ACCEL + 5


def test_pd_acumula_y_satura():
    c = ControlFirmware(GananciasPID())
    for _ in range(200):  # inclinado y quieto: la salida se integra hasta MAX_CONTROL_OUTPUT
        e = c.ciclo_control(0.0, 30.0)
    assert e.control_output == -500


def test_corte_de_motores_a_74_grados():
    c = ControlFirmware(GananciasPID(), angulo_inicial_deg=73.0)
    e = c.ciclo_control(200.0, 80.0)  # cruza 74°
    assert not e.motores_activos and c.control_output == 0 and c.pid_error_sum == 0


def test_ganancias_de_levantado_por_encima_de_56():
    c = ControlFirmware(GananciasPID(kp=0.5), angulo_inicial_deg=60.0)
    assert c.kp == pytest.approx(0.1) and c.ki_thr == 0
    c.angle = 10.0
    c.ciclo_control(0.0, 10.0)
    assert c.kp == pytest.approx(0.5)


def test_joystick_como_el_firmware():
    assert throttle_desde_joystick(0.05) == 0  # zona muerta
    assert throttle_desde_joystick(1.0) == int(300 * 0.45)
    assert throttle_desde_joystick(-1.0) == -int(300 * 0.45)
    x = (0.5 - 0.06) / 0.94
    assert throttle_desde_joystick(0.5) == int(x**3 * 135)


def test_frecuencia_real_del_timer():
    assert pasos_por_segundo_reales(0) == 0
    assert pasos_por_segundo_reales(4) == pytest.approx(100.0)  # 2e6/100 es exacto
    assert pasos_por_segundo_reales(-4) == pytest.approx(-100.0)
    # período entero: 2e6 // 7525 = 265 → 7547.2 pasos/s en vez de 7525
    assert pasos_por_segundo_reales(301) == pytest.approx(2e6 / 265)


# --- Lazo cerrado ----------------------------------------------------------
@pytest.mark.parametrize("escenario", escenarios_por_defecto(), ids=lambda e: e.nombre)
def test_ganancias_por_defecto_no_se_caen(escenario):
    r = simular(GananciasPID(), escenario)
    assert not r.cayo
    # Con los parámetros provisorios el gemelo predice un ciclo límite (ver README), pero acotado
    assert np.max(np.abs(r.theta_deg)) < 30


@pytest.mark.parametrize("ruido", [False, True])
def test_ganancias_por_defecto_estables_con_ruido(ruido):
    cfg = ConfigSimulacion(ruido_gyro_dps=0.3 if ruido else 0, ruido_accel_deg=1.0 if ruido else 0, semilla=3)
    r = simular(GananciasPID(), empujon_inicial(5.0, 20.0), config=cfg)
    assert not r.cayo
    # la oscilación no crece: el último tramo no supera al primero en más de un 50 %
    n = len(r.t) // 4
    assert np.ptp(r.theta_deg[-n:]) < 1.5 * np.ptp(r.theta_deg[:n]) + 5


def test_ganancias_amortiguadas_se_establecen():
    r = simular(GananciasPID(0.45, 0.08, 0.08, 0.05), empujon_inicial(10.0, 5.0))
    assert not r.cayo
    assert np.max(np.abs(r.theta_deg[-100:])) < 1.0


@pytest.mark.parametrize("g", ABSURDAS, ids=["sin_kp", "signo_invertido", "sin_pi"])
def test_ganancias_absurdas_se_caen(g):
    r = simular(g, empujon_inicial(5.0, 10.0))
    assert r.cayo and r.t_caida_s < 10


def test_signo_de_motor_invertido_se_cae():
    r = simular(GananciasPID(), empujon_inicial(5.0, 5.0), config=ConfigSimulacion(signo_motor=+1))
    assert r.cayo


def test_joystick_adelante_avanza_e_inclina_adelante():
    r = simular(GananciasPID(0.45, 0.08, 0.08, 0.05), escalon_joystick(1.0, duracion_s=6.0))
    assert not r.cayo
    assert np.mean(r.v_m_s[-100:]) > 0.3  # throttle 135 ≈ 0.6 m/s
    t_ini = r.t < 1.5
    assert np.max(r.theta_deg[t_ini & (r.t > 1.0)]) > 0  # se inclina para acelerar


def test_empuje_desvia_en_su_sentido():
    r = simular(GananciasPID(), perturbacion(3.0, 0.1, t_s=0.5, duracion_s=1.0))
    assert r.theta_deg[np.searchsorted(r.t, 0.65)] > 1.0


def test_telemetria_simulada_con_formato_A():
    r = simular(escenario=empujon_inicial(5.0, 1.0))
    mensajes = list(r.mensajes_telemetria())
    assert len(mensajes) == 10  # 10 Hz
    from gemelo.telemetria import Telemetria, parsear

    assert all(isinstance(parsear(m), Telemetria) for _, m in mensajes)
    assert parsear(mensajes[0][1]).angulo_deg == pytest.approx(5.0, abs=0.1)
