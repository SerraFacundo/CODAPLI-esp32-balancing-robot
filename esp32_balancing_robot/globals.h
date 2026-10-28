/*
 * globals.h
 * 
 * External declarations for all global variables shared across modules.
 * The actual variable definitions are in globals.cpp.
 */

#ifndef GLOBALS_H_
#define GLOBALS_H_

#include <Arduino.h>
#include "esp32-hal-timer.h"

// ============================================================================
// HARDWARE TIMERS (for stepper motor pulse generation)
// ============================================================================
extern hw_timer_t *timer1;
extern hw_timer_t *timer2;

// ============================================================================
// LOOP TIMING
// ============================================================================
extern uint8_t cascade_control_loop_counter;
extern uint8_t loop_counter;         // Medium loop counter (~40Hz)
extern uint8_t slow_loop_counter;    // Slow loop counter (~2Hz)
extern long    timer_old;
extern long    timer_value;
extern float   dt;                   // Delta time in seconds

// ============================================================================
// IMU / ANGLE
// ============================================================================
extern float angle_adjusted;         // Current tilt angle (with offset applied)
extern float angle_adjusted_Old;     // Previous tilt angle
extern float angle_adjusted_filtered;// Low-pass filtered angle (for calibration)
extern float angle_offset;           // Calibration offset

// ============================================================================
// PID CONTROL VARIABLES
// ============================================================================
// Active gains (may switch between normal/raiseup/user values)
extern float Kp;
extern float Kd;
extern float Kp_thr;
extern float Ki_thr;

// User-set gains (from web interface sliders)
extern float Kp_user;
extern float Kd_user;
extern float Kp_thr_user;
extern float Ki_thr_user;

// Position control gains
extern float Kp_position;
extern float Kd_position;

// PID state variables
extern float PID_errorSum;
extern float PID_errorOld;
extern float PID_errorOld2;
extern float setPointOld;

// Control outputs
extern float   target_angle;        // Output of speed controller → input to stability controller
extern int16_t throttle;            // Forward/backward command from user
extern float   steering;            // Left/right command from user
extern float   max_throttle;
extern float   max_steering;
extern float   max_target_angle;
extern float   control_output;      // Accumulated output of stability controller

// ============================================================================
// MOTOR VARIABLES
// ============================================================================
extern int16_t motor1;               // Final motor 1 command
extern int16_t motor2;               // Final motor 2 command

extern int16_t speed_M1, speed_M2;   // Current motor speeds (with acceleration limiting)
extern int8_t  dir_M1, dir_M2;       // Current motor directions (-1, 0, 1)

extern volatile int32_t steps1;      // Step counter motor 1 (modified in ISR)
extern volatile int32_t steps2;      // Step counter motor 2 (modified in ISR)
extern int32_t target_steps1;
extern int32_t target_steps2;
extern int16_t motor1_control;
extern int16_t motor2_control;

extern int16_t actual_robot_speed;
extern int16_t actual_robot_speed_Old;
extern float   estimated_speed_filtered;

// ============================================================================
// OPERATING MODE
// ============================================================================
extern bool    positionControlMode;
extern uint8_t mode;                 // 0 = Normal, 1 = Pro

// ============================================================================
// WEB CONTROL VARIABLES (from WebSocket)
// ============================================================================
extern float   ws_throttle;          // -1.0 to 1.0 from joystick Y axis
extern float   ws_steering;          // -1.0 to 1.0 from joystick X axis
extern bool    ws_servo_on;
extern bool    ws_led_on;
extern bool    ws_pro_mode;
extern bool    ws_calibrate_request;
extern bool    ws_new_pid;           // Flag: new PID values received
extern bool    ws_connected;         // At least one authenticated WebSocket client

// ============================================================================
// AUTO RAISE-UP STATE
// ============================================================================
extern bool    autoRaiseup_done;       // Already completed (or disabled)
extern bool    autoRaiseup_active;     // Currently attempting to raise
extern uint8_t autoRaiseup_attempts;   // Number of attempts made
extern unsigned long autoRaiseup_timer;

// ============================================================================
// TELEMETRY (sent to web client)
// ============================================================================
extern float   telemetry_angle;
extern float   telemetry_speed;
extern float   telemetry_battery;
extern bool    telemetry_motors_active;

#endif /* GLOBALS_H_ */