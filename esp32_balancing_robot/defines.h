/*
 * defines.h
 * 
 * Pin definitions, control constants, and ESP32 core compatibility macros.
 * 
 * IMPORTANT: If you're using ESP32 Arduino Core 3.x, the timer and LEDC APIs
 * changed significantly. This file includes compatibility macros that detect
 * the core version automatically and use the correct API.
 * 
 * MICROSTEPPING: Set to 8 for 1/8 microstepping (MS1=HIGH, MS2=HIGH, MS3=LOW)
 *                Set to 16 for 1/16 microstepping (MS1=HIGH, MS2=HIGH, MS3=HIGH)
 *                This MUST match your A4988 driver hardware configuration.
 */

#ifndef DEFINES_H_
#define DEFINES_H_

// ============================================================================
// ESP32 CORE VERSION DETECTION & COMPATIBILITY MACROS
// ============================================================================
// The ESP32 Arduino Core 3.x changed timer and LEDC APIs.
// We detect the version and provide unified macros.

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  #define ESP32_CORE_V3
#endif

// Timer compatibility: Core 2.x uses timerAlarmWrite(), Core 3.x uses timerAlarm()
#ifdef ESP32_CORE_V3
  #define TIMER_ALARM_WRITE(timer, value, autoreload) timerAlarm(timer, value, autoreload, 0)
#else
  #define TIMER_ALARM_WRITE(timer, value, autoreload) timerAlarmWrite(timer, value, autoreload)
#endif

// ============================================================================
// PIN DEFINITIONS — Match the PCBWay ESP32 Balancing Robot Shield schematic
// ============================================================================
#define PIN_ENABLE_MOTORS 12        // Shared enable for both A4988 drivers
#define PIN_MOTOR1_DIR    27        // Motor 1 direction (A4988 DIR pin)
#define PIN_MOTOR1_STEP   14        // Motor 1 step pulse (A4988 STEP pin)
#define PIN_MOTOR2_DIR    25        // Motor 2 direction (A4988 DIR pin)
#define PIN_MOTOR2_STEP   26        // Motor 2 step pulse (A4988 STEP pin)
#define PIN_SERVO         13        // Servo MG-90S signal pin
#define PIN_LED           32        // Bright LED(s)
#define PIN_WIFI_LED       2        // Onboard LED (WiFi status indicator)
#define PIN_BUZZER        33        // Active buzzer

// ============================================================================
// MICROSTEPPING CONFIGURATION
// ============================================================================
// Set this to match your A4988 driver jumper configuration:
//   8  = 1/8  microstepping (MS1=HIGH, MS2=HIGH, MS3=LOW)  — confirmed for this build
//   16 = 1/16 microstepping (MS1=HIGH, MS2=HIGH, MS3=HIGH)
#define MICROSTEPPING 8

// ============================================================================
// NORMAL MODE PARAMETERS (slower, for careful control)
// ============================================================================
#define MAX_THROTTLE       300
#define MAX_STEERING       80
#define MAX_TARGET_ANGLE   10       // Max lean angle the speed controller can request (degrees)

// ============================================================================
// PRO MODE PARAMETERS (previous Normal speeds — full control)
// ============================================================================
#define MAX_THROTTLE_PRO   550
#define MAX_STEERING_PRO   140
#define MAX_TARGET_ANGLE_PRO 14

// ============================================================================
// PID CONTROL GAINS — Default values for the stability/speed/position controllers
// ============================================================================
// These are starting points. Use the web interface sliders to tune in real-time.

// Stability PD controller (inner loop, 100Hz)
// This keeps the robot upright by controlling motor acceleration based on tilt angle.
#define KP 0.32                     // Proportional: how aggressively to correct tilt
#define KD 0.050                    // Derivative: damping to prevent oscillation

// Speed PI controller (outer loop, ~25Hz)
// This controls forward/backward speed by adjusting the target lean angle.
#define KP_THROTTLE 0.080           // Proportional: speed error → angle correction
#define KI_THROTTLE 0.1             // Integral: accumulated speed error (eliminates steady-state)

// Position PD controller (optional, for precise movements)
#define KP_POSITION 0.06
#define KD_POSITION 0.45

// Raise-up gains: Special gains used when robot is recovering from a fall (>56° tilt)
// Lower Kp prevents violent correction, higher Kd provides damping during recovery
#define KP_RAISEUP          0.1
#define KD_RAISEUP          0.16
#define KP_THROTTLE_RAISEUP 0       // No speed control during raise-up
#define KI_THROTTLE_RAISEUP 0.0

// ============================================================================
// CONTROL LIMITS
// ============================================================================
#define MAX_CONTROL_OUTPUT 500       // Maximum motor command value
#define ITERM_MAX_ERROR    30        // Max single-sample error for integral windup prevention
#define ITERM_MAX          10000     // Max accumulated integral term

// ============================================================================
// MOTOR PARAMETERS
// ============================================================================
// MAX_ACCEL limits how fast the motor speed can change per control cycle.
// Too low = sluggish response (robot falls before motors catch up)
// Too high = jerky movement and possible missed steps
// Original was 14, increased to 20 for better balance response.
#define MAX_ACCEL 20                // Max speed change per cycle (recommended: 14-25)

#define ZERO_SPEED 0xFFFFF          // Timer period value representing zero speed (motor stopped)
                                    // Reduced from 0xFFFFFF to prevent potential overflow issues

// ============================================================================
// AUTO RAISE-UP CONFIGURATION
// ============================================================================
// After boot + calibration, the robot uses the servo arm to push itself upright.
// It swings the servo to shift center of gravity, then motors catch the balance.
#define AUTO_RAISEUP_ENABLED    true
#define AUTO_RAISEUP_DELAY_MS   2000    // Wait time after boot before attempting (ms)
#define AUTO_RAISEUP_PUSH_MS    600     // How long the servo pushes (ms)
#define AUTO_RAISEUP_RETRIES    3       // Max attempts before giving up

// ============================================================================
// ANGLE OFFSET
// ============================================================================
// Compensates for robot's center of gravity not being perfectly centered.
// Set to 0 initially, then use the web interface calibration to find the correct value.
#define ANGLE_OFFSET 0.0

// ============================================================================
// SERVO DEFINITIONS
// ============================================================================
#define SERVO_AUX_NEUTRO       4444     // Servo neutral position (16-bit PWM duty cycle at 50Hz)
#define SERVO_MIN_PULSEWIDTH   (SERVO_AUX_NEUTRO - 2700)
#define SERVO_MAX_PULSEWIDTH   (SERVO_AUX_NEUTRO + 2700)

// ============================================================================
// WIFI AP CONFIGURATION
// ============================================================================
#define WIFI_AP_SSID     "BalanceBot"
// La contraseña del AP ya no está acá: se define en secrets.h (SECRET_AP_PASSWORD),
// que no se sube al repositorio. Ver secrets.example.h.
#define WIFI_AP_CHANNEL  1          // En modo AP+STA el AP pasa al canal del router
#define WIFI_HOSTNAME    "balancebot"
#define WEBSERVER_PORT   80

// ============================================================================
// WEBSOCKET AUTHENTICATION
// ============================================================================
// Cada cliente debe enviar "T:<token>" (SECRET_WS_TOKEN de secrets.h) antes de
// que se acepte cualquier comando o se le envíe telemetría.
#define WS_MAX_CLIENTS         4      // Sesiones WebSocket simultáneas
#define WS_AUTH_TIMEOUT_MS     5000   // Se cierra al cliente que no se autentica en este tiempo
#define WS_AUTH_FAIL_COOLDOWN_MS 2000 // Tras un token inválido, se rechazan intentos durante este tiempo
#define WS_CLOSE_AUTH_FAILED   4001   // Código de cierre WebSocket: token inválido
#define WS_CLOSE_AUTH_TIMEOUT  4002   // Código de cierre WebSocket: no se autenticó a tiempo

// ============================================================================
// JOYSTICK SENSITIVITY
// ============================================================================
// DEADZONE: Joystick inputs below this threshold (0-1) are ignored.
// Prevents accidental movement from slight touches. Range: 0.0 to 0.15
#define JOYSTICK_DEADZONE 0.06f

// THROTTLE_SCALE: Multiplier for maximum throttle (0.0 to 1.0).
// At 0.5, the joystick at full deflection gives 50% of MAX_THROTTLE.
// Start low and increase as you get comfortable controlling the robot.
#define THROTTLE_SCALE 0.45f

// STEERING_SCALE: Multiplier for maximum steering (0.0 to 1.0).
#define STEERING_SCALE 0.50f

// JOYSTICK_CURVE: Exponent for the response curve.
// 1.0 = linear (very sensitive), 2.0 = quadratic, 3.0 = cubic (smooth center)
// Cubic (3.0) gives fine control near center: 50% joystick = only 12.5% speed
#define JOYSTICK_CURVE 3.0f

// ============================================================================
// UTILITY MACROS
// ============================================================================
#define CLR(x,y) (x &= (~(1<<y)))
#define SET(x,y) (x |= (1<<y))
#define RAD2GRAD 57.2957795f
#define GRAD2RAD 0.01745329251994329576923690768489f

// ============================================================================
// DEBUG CONFIGURATION
// ============================================================================
// Uncomment to enable verbose serial output for debugging
// WARNING: Excessive serial output can slow down the control loop!
//#define DEBUG_ANGLE          // Print angle values every cycle
//#define DEBUG_MOTORS         // Print motor commands every cycle
//#define DEBUG_PID            // Print PID internals
#define DEBUG_STARTUP          // Print startup diagnostics

#endif /* DEFINES_H_ */