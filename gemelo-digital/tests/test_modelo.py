"""Dinámica y unidades del modelo (sin control)."""

import math

import pytest

from gemelo import modelo as mod
from gemelo.medicion import inercia_centro_masa, inercia_pivote
from gemelo.modelo import GananciasPID, ParametrosFisicos


def test_conversion_de_unidades_del_firmware():
    # 200 pasos × 1/8 micropaso = 1600 pasos por vuelta; 1 unidad = 25 pasos/s
    assert mod.PASOS_POR_VUELTA == 1600
    assert mod.GRADOS_S_POR_UNIDAD == pytest.approx(5.625)
    assert mod.unidades_a_rad_s(1) == pytest.approx(math.radians(5.625))
    # 64 unidades = 1600 pasos/s = 1 vuelta por segundo
    assert mod.unidades_a_rad_s(64) == pytest.approx(2 * math.pi)
    p = ParametrosFisicos(radio_rueda_m=0.045)
    assert mod.unidades_a_m_s(64, p) == pytest.approx(2 * math.pi * 0.045)


def test_coeficientes_en_si():
    p = ParametrosFisicos(masa_cuerpo_kg=0.6, masa_rueda_kg=0.05, radio_rueda_m=0.05, altura_cm_m=0.1,
                          inercia_cuerpo_kg_m2=0.002)
    c = p.coeficientes()
    assert c.B == pytest.approx(0.6 * 0.05 * 0.1)
    assert c.C == pytest.approx(0.002 + 0.6 * 0.01)  # Steiner
    assert c.A == pytest.approx(0.7 * 0.0025 + 2 * 0.5 * 0.05 * 0.0025)
    assert c.mgl == pytest.approx(0.6 * 9.81 * 0.1)


@pytest.mark.parametrize("modelo", mod.MODELOS)
def test_conservacion_de_energia_con_motor_quieto(modelo):
    """Sin motor ni fuerzas externas la energía se conserva (vale para ángulos grandes)."""
    c = ParametrosFisicos().coeficientes()
    th, om = math.radians(170), 0.0  # colgando cerca de abajo: oscila sin caer
    e0 = mod.energia_libre(c, th, om, modelo)
    h = 1e-3
    for _ in range(3000):
        f = lambda t, w: (w, mod.aceleracion_angular(c, t, w, modelo=modelo))
        k1 = f(th, om)
        k2 = f(th + h / 2 * k1[0], om + h / 2 * k1[1])
        k3 = f(th + h / 2 * k2[0], om + h / 2 * k2[1])
        k4 = f(th + h * k3[0], om + h * k3[1])
        th += h / 6 * (k1[0] + 2 * k2[0] + 2 * k3[0] + k4[0])
        om += h / 6 * (k1[1] + 2 * k2[1] + 2 * k3[1] + k4[1])
    assert mod.energia_libre(c, th, om, modelo) == pytest.approx(e0, rel=1e-8)


def test_linealizacion_coincide_con_el_polo_inestable():
    p = ParametrosFisicos()
    c = p.coeficientes()
    th = 1e-4
    lam = mod.polo_inestable(p)
    assert mod.aceleracion_angular(c, th, 0.0) == pytest.approx(lam**2 * th, rel=1e-6)
    lam_base = mod.polo_inestable(p, mod.MODELO_BASE_IMPUESTA)
    assert mod.aceleracion_angular(c, th, 0.0, modelo=mod.MODELO_BASE_IMPUESTA) == pytest.approx(lam_base**2 * th, rel=1e-6)
    # el modelo con reacción del motor es "más lento" que la base impuesta
    assert lam < lam_base


def test_modelo_base_impuesta_es_la_ecuacion_del_enunciado():
    p = ParametrosFisicos()
    c = p.coeficientes()
    m, l, r, g, ic = p.masa_cuerpo_kg, p.altura_cm_m, p.radio_rueda_m, p.g, p.inercia_cuerpo_kg_m2
    th, u_punto = 0.2, 30.0
    a = r * u_punto
    esperado = (m * g * l * math.sin(th) - m * l * a * math.cos(th)) / (ic + m * l**2)
    assert mod.aceleracion_angular(c, th, 0.0, u_punto, modelo=mod.MODELO_BASE_IMPUESTA) == pytest.approx(esperado)


@pytest.mark.parametrize("modelo", mod.MODELOS)
def test_salto_equivale_a_integrar_una_aceleracion_corta(modelo):
    """El salto de θ̇ por un cambio brusco del motor es el límite de una rampa muy corta."""
    c = ParametrosFisicos().coeficientes()
    du, tau = 2.0, 1e-6
    integrado = mod.aceleracion_angular(c, 0.0, 0.0, du / tau, modelo=modelo) * tau
    assert mod.salto_velocidad_angular(c, 0.0, du, modelo) == pytest.approx(integrado, rel=1e-3)
    # acelerar las ruedas hacia +x tira el cuerpo hacia atrás
    assert mod.salto_velocidad_angular(c, 0.0, du, modelo) < 0


def test_inercia_por_pendulo_fisico_y_steiner():
    # Varilla de 0.2 m colgada de un extremo: I_pivote = m·L²/3, T = 2π·sqrt(2L/(3g))
    m, L, g = 0.5, 0.2, 9.81
    T = 2 * math.pi * math.sqrt(2 * L / (3 * g))
    assert inercia_pivote(m, L / 2, T, g) == pytest.approx(m * L**2 / 3)
    assert inercia_centro_masa(m, L / 2, T, g) == pytest.approx(m * L**2 / 12)


def test_ganancias_desde_texto_y_comando():
    g = GananciasPID.desde_texto("0.45, 0.08, 0.08, 0.05")
    assert g.comando_ws() == "P:0.45:0.08:0.08:0.05"
    assert GananciasPID.desde_texto(g.comando_ws()) == g
    # sin colas de punto flotante en el comando
    assert GananciasPID(kp=0.1 + 0.2).comando_ws().startswith("P:0.3:")
    with pytest.raises(ValueError):
        GananciasPID.desde_texto("1,2,3")
