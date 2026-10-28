/*
 * Motors.h
 * 
 * Stepper motor speed control via A4988 drivers.
 * Motors are driven by hardware timer interrupts that generate step pulses.
 * These functions set the timer period (= motor speed) with acceleration limiting.
 */

#ifndef MOTORS_H_
#define MOTORS_H_

#include <Arduino.h>

void setMotorSpeedM1(int16_t tspeed);
void setMotorSpeedM2(int16_t tspeed);

#endif /* MOTORS_H_ */
