"""Réplica en Python del control discreto del firmware (un ciclo = una muestra del MPU6050).

Cada bloque cita la línea equivalente de esp32_balancing_robot/. Se respetan los
truncamientos a entero (int16_t) porque cambian el comportamiento cerca de cero.
Diferencias conocidas: el ESP32 calcula en float de 32 bits (acá 64 bits) y el robot
simulado no gira (steering = 0, así que motor1 = motor2).
"""

from __future__ import annotations

import math
from dataclasses import dataclass

from .modelo import FRECUENCIA_TIMER_HZ, PASOS_POR_UNIDAD, GananciasPID

# defines.h
MAX_THROTTLE = 300
MAX_TARGET_ANGLE = 10
MAX_THROTTLE_PRO = 550
MAX_TARGET_ANGLE_PRO = 14
MAX_CONTROL_OUTPUT = 500
ITERM_MAX_ERROR = 30
ITERM_MAX = 10000
MAX_ACCEL = 20
ZERO_SPEED = 0xFFFFF
JOYSTICK_DEADZONE = 0.06
THROTTLE_SCALE = 0.45
GANANCIAS_LEVANTADO = GananciasPID(kp=0.1, kd=0.16, kp_throttle=0.0, ki_throttle=0.0)

ANGULO_CORTE_MOTORES = 74  # esp32_balancing_robot.ino: const int angle_ready = 74
ANGULO_GANANCIAS_LEVANTADO = 56  # por encima se usan las ganancias de levantado


def constrain(x, bajo, alto):
    """constrain() de Arduino."""
    return bajo if x < bajo else alto if x > alto else x


def a_int(x: float) -> int:
    """Conversión float → int de C (trunca hacia cero), como la asignación a int16_t."""
    return int(x)


def throttle_desde_joystick(joystick: float, max_throttle: float = MAX_THROTTLE) -> int:
    """Joystick web [-1, 1] → throttle (zona muerta + curva cúbica + escala), como en loop()."""
    thr = constrain(joystick, -1.0, 1.0)
    if abs(thr) < JOYSTICK_DEADZONE:
        thr = 0.0
    elif thr > 0:
        thr = (thr - JOYSTICK_DEADZONE) / (1.0 - JOYSTICK_DEADZONE)
    else:
        thr = (thr + JOYSTICK_DEADZONE) / (1.0 - JOYSTICK_DEADZONE)
    thr_curvado = thr * abs(thr) * abs(thr)
    return a_int(thr_curvado * max_throttle * THROTTLE_SCALE)


def pasos_por_segundo_reales(speed_m: int) -> float:
    """Frecuencia de pasos que sale realmente del timer (Motors.cpp).

    El período es entero: timer_period = 2000000 / (speed_M·25), así que a velocidades
    altas la frecuencia real difiere un poco de la pedida.
    """
    pasos = speed_m * PASOS_POR_UNIDAD
    if pasos == 0:
        return 0.0  # dir_M = 0: el ISR no genera pasos
    periodo = min(FRECUENCIA_TIMER_HZ // abs(pasos), ZERO_SPEED)
    return math.copysign(FRECUENCIA_TIMER_HZ / periodo, pasos)


@dataclass
class EstadoCiclo:
    """Valores internos del firmware al terminar un ciclo (para registrar y graficar)."""

    angle_adjusted: float
    target_angle: float
    control_output: float
    speed_m: int
    actual_robot_speed: int
    estimated_speed_filtered: float
    throttle: int
    motores_activos: bool
    rampa_saturada: bool
    corrio_pi: bool


class ControlFirmware:
    """Estado y lazo de control de esp32_balancing_robot.ino (rama sin control de posición).

    periodo_pi_ciclos: cada cuántos ciclos corre speedPIControl. El firmware actual lo llama
    en TODOS los ciclos (cascade_control_loop_counter se declara pero no se usa), así que el
    valor por defecto es 1. Con N > 1 el PI recibe DT = N·dt.
    """

    def __init__(
        self,
        ganancias: GananciasPID,
        angulo_inicial_deg: float = 0.0,
        modo_pro: bool = False,
        periodo_pi_ciclos: int = 1,
        angle_offset: float = 0.0,
    ):
        if periodo_pi_ciclos < 1:
            raise ValueError("periodo_pi_ciclos tiene que ser >= 1")
        self.ganancias_usuario = ganancias
        self.periodo_pi_ciclos = periodo_pi_ciclos
        self.angle_offset = angle_offset
        self.max_throttle = MAX_THROTTLE_PRO if modo_pro else MAX_THROTTLE
        self.max_target_angle = MAX_TARGET_ANGLE_PRO if modo_pro else MAX_TARGET_ANGLE

        # MPU6050_calibrate(): el filtro arranca con el ángulo del acelerómetro
        self.angle = angulo_inicial_deg
        self.angle_adjusted = self.angle + angle_offset
        self.angle_adjusted_old = self.angle_adjusted

        self.estimated_speed_filtered = 0.0
        self.pid_error_sum = 0.0
        self.pid_error_old = self.angle_adjusted  # el robot venía quieto en ese ángulo
        self.set_point_old = 0.0
        self.target_angle = 0.0
        self.control_output = 0.0
        self.speed_m = 0  # speed_M1 = speed_M2
        self.actual_robot_speed = 0
        self.motores_activos = abs(self.angle_adjusted) < ANGULO_CORTE_MOTORES
        self.ciclo = 0
        self.llamadas_pi = 0
        self._elegir_ganancias()

    # -- Bloques del firmware -------------------------------------------------
    def filtro_complementario(self, gyro_dps: float, accel_deg: float, dt: float) -> float:
        # MPU6050.cpp: angle = 0.99f * (angle + x_gyro_value * dt) + 0.01f * accel_angle
        self.angle = 0.99 * (self.angle + gyro_dps * dt) + 0.01 * accel_deg
        return self.angle

    def stability_pd_control(self, dt: float, entrada: float, set_point: float) -> float:
        # Control.cpp: stabilityPDControl()
        error = set_point - entrada
        kd_set_point = constrain(set_point - self.set_point_old, -8.0, 8.0)
        salida = self.kp * error + (self.kd * kd_set_point - self.kd * (entrada - self.pid_error_old)) / dt
        self.pid_error_old = entrada
        self.set_point_old = set_point
        return salida

    def speed_pi_control(self, dt: float, entrada: int, set_point: int) -> float:
        # Control.cpp: speedPIControl() — entrada y set_point son int16_t
        self.llamadas_pi += 1
        error = set_point - entrada
        self.pid_error_sum += constrain(error, -ITERM_MAX_ERROR, ITERM_MAX_ERROR)
        self.pid_error_sum = constrain(self.pid_error_sum, -ITERM_MAX, ITERM_MAX)
        return self.kp_thr * error + self.ki_thr * self.pid_error_sum * dt

    def set_motor_speed(self, tspeed: int) -> bool:
        # Motors.cpp: setMotorSpeedM1() — limita el cambio a MAX_ACCEL por ciclo
        diferencia = self.speed_m - tspeed
        if diferencia > MAX_ACCEL:
            self.speed_m -= MAX_ACCEL
            return True
        if diferencia < -MAX_ACCEL:
            self.speed_m += MAX_ACCEL
            return True
        self.speed_m = tspeed
        return False

    def _elegir_ganancias(self) -> None:
        # loop(): GAIN SWITCHING — define las ganancias del ciclo siguiente
        if -ANGULO_GANANCIAS_LEVANTADO < self.angle_adjusted < ANGULO_GANANCIAS_LEVANTADO:
            g = self.ganancias_usuario
        else:
            g = GANANCIAS_LEVANTADO
        self.kp, self.kd, self.kp_thr, self.ki_thr = g.kp, g.kd, g.kp_throttle, g.ki_throttle

    # -- Un ciclo completo de loop() -------------------------------------------
    def ciclo_control(self, gyro_dps: float, accel_deg: float, joystick: float = 0.0, dt: float = 0.01) -> EstadoCiclo:
        self.angle_adjusted_old = self.angle_adjusted
        self.angle_adjusted = self.filtro_complementario(gyro_dps, accel_deg, dt) + self.angle_offset

        throttle = throttle_desde_joystick(joystick, self.max_throttle)

        # ESTIMATE ROBOT SPEED
        self.actual_robot_speed = a_int((self.speed_m + self.speed_m) / 2)
        angular_velocity = a_int((self.angle_adjusted - self.angle_adjusted_old) * 25.0)
        estimated_speed = -self.actual_robot_speed + angular_velocity
        self.estimated_speed_filtered = self.estimated_speed_filtered * 0.9 + estimated_speed * 0.1

        # SPEED PI CONTROLLER (outer loop)
        corrio_pi = self.ciclo % self.periodo_pi_ciclos == 0
        if corrio_pi:
            ta = self.speed_pi_control(dt * self.periodo_pi_ciclos, a_int(self.estimated_speed_filtered), throttle)
            self.target_angle = constrain(ta, -self.max_target_angle, self.max_target_angle)

        # STABILITY PD CONTROLLER (inner loop) — la salida se ACUMULA
        self.control_output += self.stability_pd_control(dt, self.angle_adjusted, self.target_angle)
        self.control_output = constrain(self.control_output, -MAX_CONTROL_OUTPUT, MAX_CONTROL_OUTPUT)

        motor = constrain(a_int(self.control_output), -MAX_CONTROL_OUTPUT, MAX_CONTROL_OUTPUT)

        rampa_saturada = False
        if -ANGULO_CORTE_MOTORES < self.angle_adjusted < ANGULO_CORTE_MOTORES:
            self.motores_activos = True
            rampa_saturada = self.set_motor_speed(motor)
        else:
            # ROBOT HAS FALLEN: motores deshabilitados y reset del control
            self.motores_activos = False
            self.set_motor_speed(0)
            self.pid_error_sum = 0.0
            self.control_output = 0.0

        self._elegir_ganancias()
        self.ciclo += 1
        return EstadoCiclo(
            angle_adjusted=self.angle_adjusted,
            target_angle=self.target_angle,
            control_output=self.control_output,
            speed_m=self.speed_m,
            actual_robot_speed=self.actual_robot_speed,
            estimated_speed_filtered=self.estimated_speed_filtered,
            throttle=throttle,
            motores_activos=self.motores_activos,
            rampa_saturada=rampa_saturada,
            corrio_pi=corrio_pi,
        )
