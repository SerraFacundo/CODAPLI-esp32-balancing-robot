/*
 * Timers.cpp
 * 
 * Hardware timer setup and ISR (Interrupt Service Routines) for stepper motors.
 * 
 * Architecture:
 * =============
 * Two hardware timers (Timer 0 and Timer 1) each drive one stepper motor.
 * Each timer fires an interrupt at a variable rate set by the motor speed functions.
 * The ISR generates a single step pulse (HIGH then LOW) on the A4988 STEP pin.
 * 
 * Timer Configuration:
 * - Base clock: 80 MHz (ESP32 APB clock)
 * - Prescaler: 40 → Timer clock = 2 MHz
 * - Count up with auto-reload
 * - Alarm value = timer period = 2,000,000 / steps_per_second
 * 
 * The ISR also maintains a step counter (steps1/steps2) used for position control.
 * The step counter direction depends on dir_M1/dir_M2.
 * 
 * ESP32 Core Compatibility:
 * - Core 2.x: timerBegin(num, divider, countUp), timerAlarmWrite(), timerAlarmEnable()
 * - Core 3.x: timerBegin(freq), timerAlarm(), no separate enable needed
 */

#include "globals.h"
#include "defines.h"
#include <Arduino.h>
#include "esp32-hal-timer.h"

// Spinlocks for ISR critical sections (prevent concurrent access on dual-core ESP32)
portMUX_TYPE muxer1 = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE muxer2 = portMUX_INITIALIZER_UNLOCKED;

// ============================================================================
// Timer 1 ISR — Motor 1 step pulse generation
// ============================================================================
// IRAM_ATTR: Places this function in IRAM for fastest execution
// This ISR must be extremely fast — just a GPIO pulse and counter update
void IRAM_ATTR timer1ISR() {
  portENTER_CRITICAL_ISR(&muxer1);

  if (dir_M1 != 0) {
    // Generate step pulse: HIGH-LOW sequence
    // The A4988 steps on the rising edge of STEP
    digitalWrite(PIN_MOTOR1_STEP, HIGH);

    // Update position counter
    if (dir_M1 > 0)
      steps1--;
    else
      steps1++;

    digitalWrite(PIN_MOTOR1_STEP, LOW);
  }

  portEXIT_CRITICAL_ISR(&muxer1);
}

// ============================================================================
// Timer 2 ISR — Motor 2 step pulse generation
// ============================================================================
void IRAM_ATTR timer2ISR() {
  portENTER_CRITICAL_ISR(&muxer2);

  if (dir_M2 != 0) {
    digitalWrite(PIN_MOTOR2_STEP, HIGH);

    if (dir_M2 > 0)
      steps2--;
    else
      steps2++;

    digitalWrite(PIN_MOTOR2_STEP, LOW);
  }

  portEXIT_CRITICAL_ISR(&muxer2);
}

// ============================================================================
// initTimers — Configure hardware timers for both motors
// ============================================================================
void initTimers() {
#ifdef ESP32_CORE_V3
  // ---- ESP32 Arduino Core 3.x API ----
  // timerBegin(frequency) — sets timer to count at given frequency
  timer1 = timerBegin(2000000);  // 2 MHz timer clock
  timerAttachInterrupt(timer1, &timer1ISR);
  timerAlarm(timer1, ZERO_SPEED, true, 0);  // Initial period = stopped

  timer2 = timerBegin(2000000);
  timerAttachInterrupt(timer2, &timer2ISR);
  timerAlarm(timer2, ZERO_SPEED, true, 0);
#else
  // ---- ESP32 Arduino Core 2.x API ----
  // timerBegin(timer_num, prescaler, count_up)
  // 80 MHz / 40 = 2 MHz timer clock
  timer1 = timerBegin(0, 40, true);
  timerAttachInterrupt(timer1, &timer1ISR, true);
  timerAlarmWrite(timer1, ZERO_SPEED, true);  // Initial period = stopped

  timer2 = timerBegin(1, 40, true);
  timerAttachInterrupt(timer2, &timer2ISR, true);
  timerAlarmWrite(timer2, ZERO_SPEED, true);

  // Must explicitly enable alarms in Core 2.x
  timerAlarmEnable(timer1);
  timerAlarmEnable(timer2);
#endif

  Serial.println("Timers initialized (2 MHz, auto-reload)");
}
