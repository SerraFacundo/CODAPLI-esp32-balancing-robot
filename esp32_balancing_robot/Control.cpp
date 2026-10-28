/*
 * Control.cpp
 * 
 * PID controller implementations.
 * 
 * The control architecture is a cascade of three controllers:
 * 
 * USER INPUT (throttle)
 *       ↓
 * [Speed PI Controller] ← estimated_speed
 *       ↓ target_angle
 * [Stability PD Controller] ← IMU angle
 *       ↓ motor_acceleration
 * [Accumulator (integration)]
 *       ↓ motor_speed
 * [Motors]
 * 
 * The stability controller output is ACCUMULATED (integrated) in the main loop,
 * so it effectively controls motor acceleration, not speed directly.
 * This is key: the PD controller acts as a PID when its output is integrated.
 */

#include <Arduino.h>
#include "defines.h"
#include "globals.h"

// ============================================================================
// stabilityPDControl — Inner loop PD controller for balance
// ============================================================================
// This runs at 100Hz (every IMU sample).
//
// DT:       Time step in seconds
// input:    Current robot angle (from IMU)
// setPoint: Target angle (from speed controller)
// Kp:       Proportional gain — how hard to correct angle error
// Kd:       Derivative gain — damping to prevent oscillation
//
// Returns:  Motor acceleration command (added to control_output in main loop)
//
// The derivative term is split into two parts:
//   1. d(input)/dt — reacts to how fast the angle is changing (sensor-based)
//   2. d(setPoint)/dt — reacts to setpoint changes (makes response snappier)
// This "derivative on measurement" technique prevents derivative kick
// when the setpoint changes suddenly.
float stabilityPDControl(float DT, float input, float setPoint, float Kp, float Kd) {
  float error;
  float output;

  error = setPoint - input;

  // Limit setpoint rate of change to prevent derivative kick
  float Kd_setPoint = constrain((setPoint - setPointOld), -8.0f, 8.0f);

  // PD output: P term + D term (split derivative)
  output = Kp * error + (Kd * Kd_setPoint - Kd * (input - PID_errorOld)) / DT;

  // Store for next iteration
  PID_errorOld = input;     // For derivative: uses only input (not setpoint)
  setPointOld = setPoint;

  return output;
}

// ============================================================================
// speedPIControl — Outer loop PI controller for speed
// ============================================================================
// This runs at ~25Hz (every 4th IMU sample via cascade counter).
//
// DT:       Time step in seconds
// input:    Estimated robot speed (from stepper speeds + angle rate)
// setPoint: Desired speed (from user throttle input)
// Kp:       Proportional gain — immediate speed error correction
// Ki:       Integral gain — eliminates steady-state speed error
//
// Returns:  Target angle for the stability controller
//
// The integral term has anti-windup protection:
//   1. Each error sample is clamped to ±ITERM_MAX_ERROR
//   2. The accumulated sum is clamped to ±ITERM_MAX
// This prevents integral windup when the robot is fallen over or
// the motors are saturated.
float speedPIControl(float DT, int16_t input, int16_t setPoint, float Kp, float Ki) {
  int16_t error;
  float output;

  error = setPoint - input;

  // Anti-windup: clamp single error contribution
  PID_errorSum += constrain(error, -ITERM_MAX_ERROR, ITERM_MAX_ERROR);
  // Anti-windup: clamp total accumulated error
  PID_errorSum = constrain(PID_errorSum, -ITERM_MAX, ITERM_MAX);

  // PI output
  output = Kp * error + Ki * PID_errorSum * DT;

  return output;
}

// ============================================================================
// positionPDControl — Position controller for precise movements
// ============================================================================
// Used when the robot needs to move to a specific step count
// (e.g., "move forward 100 steps").
//
// actualPos:   Current step count
// setPointPos: Target step count
// Kpp:         Position proportional gain
// Kdp:         Position derivative gain (uses motor speed as derivative)
// speedM:      Current motor speed (acts as derivative of position)
//
// Returns:     Speed command for this motor
float positionPDControl(long actualPos, long setPointPos, float Kpp, float Kdp, int16_t speedM) {
  float output;
  float P;

  // Proportional term with output limiting
  P = constrain(Kpp * float(setPointPos - actualPos), -115.0f, 115.0f);

  // PD output: position error + velocity damping
  output = P + Kdp * float(speedM);

  return output;
}
