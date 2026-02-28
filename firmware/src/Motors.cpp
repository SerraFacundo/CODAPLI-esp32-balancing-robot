/*
 * Motors.cpp
 * 
 * Stepper motor speed control for A4988 drivers.
 * 
 * How it works:
 * =============
 * Each motor has a hardware timer that fires an ISR at a variable rate.
 * The ISR generates a single step pulse on the A4988 STEP pin.
 * By changing the timer period, we control the step frequency = motor speed.
 * 
 * Speed units: The input "tspeed" is an abstract control value.
 * It gets multiplied by a factor (25 for 1/8 microstepping, 50 for 1/16)
 * to convert to actual steps/second, then to a timer period.
 * 
 * Timer frequency: 2 MHz (80 MHz / 40 prescaler)
 * Timer period = 2,000,000 / speed_in_steps_per_second
 * 
 * Acceleration limiting:
 * The actual motor speed (speed_M1/M2) ramps toward the target speed (tspeed)
 * at a maximum rate of MAX_ACCEL per control cycle. This prevents:
 *   1. Missed steps from sudden speed changes
 *   2. Mechanical shock
 *   3. Excessive current spikes
 */

#include <Arduino.h>
#include "defines.h"
#include "globals.h"

// ============================================================================
// setMotorSpeedM1 — Set Motor 1 speed with acceleration limiting
// ============================================================================
// tspeed: Target speed (positive = forward, negative = reverse, 0 = stop)
void setMotorSpeedM1(int16_t tspeed) {
  long timer_period;
  int16_t speed;

  // Acceleration limiter: ramp toward target speed
  if ((speed_M1 - tspeed) > MAX_ACCEL)
    speed_M1 -= MAX_ACCEL;
  else if ((speed_M1 - tspeed) < -MAX_ACCEL)
    speed_M1 += MAX_ACCEL;
  else
    speed_M1 = tspeed;

  // Convert control speed to actual step rate
#if MICROSTEPPING == 16
  speed = speed_M1 * 50;   // 1/16 microstepping: more steps per revolution
#else
  speed = speed_M1 * 25;   // 1/8 microstepping: fewer steps per revolution
#endif

  // Convert step rate to timer period
  if (speed == 0) {
    timer_period = ZERO_SPEED;
    dir_M1 = 0;
  } else if (speed > 0) {
    timer_period = 2000000L / speed;
    dir_M1 = 1;
    digitalWrite(PIN_MOTOR1_DIR, HIGH);
  } else {
    timer_period = 2000000L / (-speed);
    dir_M1 = -1;
    digitalWrite(PIN_MOTOR1_DIR, LOW);
  }

  // Clamp to minimum speed (maximum period) to prevent timer overflow
  if (timer_period > ZERO_SPEED)
    timer_period = ZERO_SPEED;

  // Update the timer alarm period
  TIMER_ALARM_WRITE(timer1, timer_period, true);
}

// ============================================================================
// setMotorSpeedM2 — Set Motor 2 speed with acceleration limiting
// ============================================================================
// Note: Motor 2 direction pins are inverted relative to Motor 1
// because the motors face opposite directions on the chassis.
void setMotorSpeedM2(int16_t tspeed) {
  long timer_period;
  int16_t speed;

  // Acceleration limiter
  if ((speed_M2 - tspeed) > MAX_ACCEL)
    speed_M2 -= MAX_ACCEL;
  else if ((speed_M2 - tspeed) < -MAX_ACCEL)
    speed_M2 += MAX_ACCEL;
  else
    speed_M2 = tspeed;

#if MICROSTEPPING == 16
  speed = speed_M2 * 50;
#else
  speed = speed_M2 * 25;
#endif

  if (speed == 0) {
    timer_period = ZERO_SPEED;
    dir_M2 = 0;
  } else if (speed > 0) {
    timer_period = 2000000L / speed;
    dir_M2 = 1;
    // Motor 2 direction is inverted (motors face opposite directions)
    digitalWrite(PIN_MOTOR2_DIR, LOW);
  } else {
    timer_period = 2000000L / (-speed);
    dir_M2 = -1;
    digitalWrite(PIN_MOTOR2_DIR, HIGH);
  }

  if (timer_period > ZERO_SPEED)
    timer_period = ZERO_SPEED;

  TIMER_ALARM_WRITE(timer2, timer_period, true);
}
