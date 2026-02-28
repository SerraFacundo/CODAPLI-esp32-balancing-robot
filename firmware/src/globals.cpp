/*
 * globals.cpp
 * 
 * Actual definitions of all global variables.
 * Default values are set here; they may be overridden during setup() or runtime.
 */

#include <Arduino.h>
#include "defines.h"
#include "esp32-hal-timer.h"

// ============================================================================
// HARDWARE TIMERS
// ============================================================================
hw_timer_t *timer1 = NULL;
hw_timer_t *timer2 = NULL;

// ============================================================================
// LOOP TIMING
// ============================================================================
uint8_t cascade_control_loop_counter = 0;
uint8_t loop_counter       = 0;
uint8_t slow_loop_counter  = 0;
long    timer_old          = 0;
long    timer_value        = 0;
float   dt                 = 0.0f;

// ============================================================================
// IMU / ANGLE
// ============================================================================
float angle_adjusted          = 0.0f;
float angle_adjusted_Old      = 0.0f;
float angle_adjusted_filtered = 0.0f;
float angle_offset            = ANGLE_OFFSET;

// ============================================================================
// PID CONTROL VARIABLES
// ============================================================================
// Active gains (start with defaults, switch dynamically)
float Kp     = KP;
float Kd     = KD;
float Kp_thr = KP_THROTTLE;
float Ki_thr = KI_THROTTLE;

// User-adjustable gains (from web interface, start with defaults)
float Kp_user     = KP;
float Kd_user     = KD;
float Kp_thr_user = KP_THROTTLE;
float Ki_thr_user = KI_THROTTLE;

// Position control gains
float Kp_position = KP_POSITION;
float Kd_position = KD_POSITION;

// PID internal state
float PID_errorSum  = 0.0f;
float PID_errorOld  = 0.0f;
float PID_errorOld2 = 0.0f;
float setPointOld   = 0.0f;

// Control outputs
float   target_angle    = 0.0f;
int16_t throttle        = 0;
float   steering        = 0.0f;
float   max_throttle    = MAX_THROTTLE;
float   max_steering    = MAX_STEERING;
float   max_target_angle = MAX_TARGET_ANGLE;
float   control_output  = 0.0f;

// ============================================================================
// MOTOR VARIABLES
// ============================================================================
int16_t motor1 = 0;
int16_t motor2 = 0;

int16_t speed_M1 = 0, speed_M2 = 0;
int8_t  dir_M1   = 0, dir_M2   = 0;

volatile int32_t steps1 = 0;
volatile int32_t steps2 = 0;
int32_t target_steps1   = 0;
int32_t target_steps2   = 0;
int16_t motor1_control  = 0;
int16_t motor2_control  = 0;

int16_t actual_robot_speed     = 0;
int16_t actual_robot_speed_Old = 0;
float   estimated_speed_filtered = 0.0f;

// ============================================================================
// OPERATING MODE
// ============================================================================
bool    positionControlMode = false;
uint8_t mode = 0;  // 0 = Normal, 1 = Pro

// ============================================================================
// WEB CONTROL VARIABLES
// ============================================================================
float   ws_throttle          = 0.0f;
float   ws_steering          = 0.0f;
bool    ws_servo_on          = false;
bool    ws_led_on            = false;
bool    ws_pro_mode          = false;
bool    ws_calibrate_request = false;
bool    ws_new_pid           = false;
bool    ws_connected         = false;

// ============================================================================
// AUTO RAISE-UP STATE
// ============================================================================
bool    autoRaiseup_done     = !AUTO_RAISEUP_ENABLED;  // If disabled, mark as done
bool    autoRaiseup_active   = false;
uint8_t autoRaiseup_attempts = 0;
unsigned long autoRaiseup_timer = 0;

// ============================================================================
// TELEMETRY
// ============================================================================
float   telemetry_angle         = 0.0f;
float   telemetry_speed         = 0.0f;
float   telemetry_battery       = 0.0f;
bool    telemetry_motors_active = false;