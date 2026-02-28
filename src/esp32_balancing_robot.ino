/*
 * esp32_balancing_robot.ino
 * 
 * ESP32 Self-Balancing Robot — Complete Firmware
 * 
 * Based on B-Robot EVO2 by JJROBOTS (https://www.jjrobots.com/)
 * Rewritten for ESP32 with embedded web interface and WebSocket control.
 * 
 * Architecture Overview:
 * =====================
 * 
 * The robot uses a cascade control system running at 100Hz:
 * 
 *   [User Joystick] → throttle, steering
 *          ↓
 *   [Speed PI Controller] (outer loop, every 4th cycle = 25Hz)
 *     Input: estimated_speed, throttle
 *     Output: target_angle (how much the robot should lean)
 *          ↓
 *   [Stability PD Controller] (inner loop, 100Hz)
 *     Input: actual_angle, target_angle
 *     Output: motor_acceleration (accumulated into control_output)
 *          ↓
 *   [Motor Speed] = control_output ± steering
 *          ↓
 *   [Stepper Motors] via hardware timer ISRs
 * 
 * Communication:
 * - ESP32 creates a WiFi Access Point (default: "BalanceBot")
 * - Serves a web interface at http://192.168.4.1
 * - WebSocket at ws://192.168.4.1/ws for real-time bidirectional control
 * - Telemetry sent to client at ~10Hz
 * 
 * Hardware:
 * - ESP32 DevKit
 * - 2x NEMA17 stepper motors
 * - 2x A4988 stepper drivers (1/8 microstepping)
 * - MPU6050 IMU (I2C)
 * - MG-90S servo
 * - 2x 18650 Li-ion batteries (7.2V)
 */

#include <Wire.h>
#include <WiFi.h>
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "Control.h"
#include "MPU6050.h"
#include "Motors.h"
#include "Timers.h"
#include "defines.h"
#include "globals.h"
#include "WebInterface.h"

// ============================================================================
// WEB SERVER & WEBSOCKET
// ============================================================================
AsyncWebServer server(WEBSERVER_PORT);
AsyncWebSocket ws("/ws");

// Telemetry timing
unsigned long lastTelemetryMs = 0;
const unsigned long TELEMETRY_INTERVAL_MS = 100;  // 10Hz telemetry updates

// Track if motors are currently enabled (for telemetry)
bool motorsActive = false;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void initMPU6050();
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len);
void processWebSocketMessage(uint8_t *data, size_t len);
void sendTelemetry();
void processControl();
void setupWiFiAP();
void setupWebServer();
void handleAutoRaiseup();

// Auto raise-up state machine
enum RaiseupState { RAISEUP_WAIT, RAISEUP_PUSH, RAISEUP_RETRACT, RAISEUP_DONE };
RaiseupState raiseupState = RAISEUP_WAIT;

// ============================================================================
// MORSE CODE PLAYER — Non-blocking "UTN"
// ============================================================================
// Morse timing: dit=120ms, dah=360ms, inter-element=120ms, inter-letter=360ms
// U: ..−   T: −   N: −.
// Sequence as alternating ON/OFF durations (ms):
// U: ON120 OFF120 ON120 OFF120 ON360 OFF360
// T: ON360 OFF360
// N: ON360 OFF120 ON120 OFF0(end)
const uint16_t morseUTN[] = {
  120, 120,  // U: dit, gap
  120, 120,  // U: dit, gap
  360, 360,  // U: dah, letter-gap
  360, 360,  // T: dah, letter-gap
  360, 120,  // N: dah, gap
  120, 0     // N: dit, END
};
const uint8_t morseLen = sizeof(morseUTN) / sizeof(morseUTN[0]);
volatile bool morseActive = false;
uint8_t morseStep = 0;
unsigned long morseTimer = 0;
void tickMorse();

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("========================================");
  Serial.println("  ESP32 Self-Balancing Robot");
  Serial.println("  Firmware v2.0 (WebSocket Control)");
  Serial.println("========================================");

  // --- GPIO Setup ---
  pinMode(PIN_ENABLE_MOTORS, OUTPUT);
  digitalWrite(PIN_ENABLE_MOTORS, HIGH);  // Motors disabled at start

  pinMode(PIN_MOTOR1_DIR, OUTPUT);
  pinMode(PIN_MOTOR1_STEP, OUTPUT);
  pinMode(PIN_MOTOR2_DIR, OUTPUT);
  pinMode(PIN_MOTOR2_STEP, OUTPUT);
  pinMode(PIN_SERVO, OUTPUT);

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  pinMode(PIN_WIFI_LED, OUTPUT);
  digitalWrite(PIN_WIFI_LED, LOW);

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  // --- Servo Setup (LEDC PWM) ---
#ifdef ESP32_CORE_V3
  ledcAttach(PIN_SERVO, 50, 16);   // 50Hz, 16-bit resolution
#else
  ledcSetup(6, 50, 16);            // Channel 6, 50Hz, 16-bit
  ledcAttachPin(PIN_SERVO, 6);
#endif
  delay(50);
#ifdef ESP32_CORE_V3
  ledcWrite(PIN_SERVO, SERVO_AUX_NEUTRO);
#else
  ledcWrite(6, SERVO_AUX_NEUTRO);
#endif

  // --- I2C & IMU Setup ---
  Wire.begin();
  Wire.setClock(400000);  // 400kHz I2C for faster sensor reads
  Serial.println("Initializing MPU6050...");
  initMPU6050();

  // --- WiFi Access Point ---
  setupWiFiAP();

  // --- Web Server & WebSocket ---
  setupWebServer();

  // --- Hardware Timers for Stepper Motors ---
  initTimers();

  // --- Startup Motor Dance (confirms motors work) ---
  Serial.println("Motor test...");
  digitalWrite(PIN_ENABLE_MOTORS, LOW);  // Enable motors
  for (uint8_t k = 0; k < 5; k++) {
    setMotorSpeedM1(5);
    setMotorSpeedM2(5);
#ifdef ESP32_CORE_V3
    ledcWrite(PIN_SERVO, SERVO_AUX_NEUTRO + 250);
#else
    ledcWrite(6, SERVO_AUX_NEUTRO + 250);
#endif
    delay(200);
    setMotorSpeedM1(-5);
    setMotorSpeedM2(-5);
#ifdef ESP32_CORE_V3
    ledcWrite(PIN_SERVO, SERVO_AUX_NEUTRO - 250);
#else
    ledcWrite(6, SERVO_AUX_NEUTRO - 250);
#endif
    delay(200);
  }
#ifdef ESP32_CORE_V3
  ledcWrite(PIN_SERVO, SERVO_AUX_NEUTRO);
#else
  ledcWrite(6, SERVO_AUX_NEUTRO);
#endif

  // --- Ready ---
  Serial.println("========================================");
  Serial.println("  Robot ready! Connect to WiFi:");
  Serial.print("    SSID: ");
  Serial.println(WIFI_AP_SSID);
  Serial.print("    Pass: ");
  Serial.println(WIFI_AP_PASSWORD);
  Serial.println("    URL:  http://192.168.4.1");
  Serial.println("========================================");

  // Beep to signal ready
  digitalWrite(PIN_BUZZER, HIGH);
  delay(100);
  digitalWrite(PIN_BUZZER, LOW);
  delay(50);
  digitalWrite(PIN_BUZZER, HIGH);
  delay(100);
  digitalWrite(PIN_BUZZER, LOW);

  // Initialize auto raise-up timer
  autoRaiseup_timer = millis();

  // Initialize timer for dt calculation
  timer_old = micros();
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  // Clean up disconnected WebSocket clients (prevents memory leaks)
  ws.cleanupClients();

  // Read timer
  timer_value = micros();

  // ========================================================================
  // MAIN CONTROL LOOP — Triggered by MPU6050 data ready (~100Hz)
  // ========================================================================
  if (MPU6050_newData()) {
    MPU6050_read_3axis();

    // Calculate time step
    dt = (timer_value - timer_old) * 0.000001f;  // Convert microseconds to seconds
    timer_old = timer_value;

    // Sanity check dt: should be ~10ms (100Hz). Clamp to prevent issues.
    if (dt > 0.05f) dt = 0.01f;   // If dt > 50ms, something went wrong
    if (dt < 0.001f) dt = 0.01f;  // If dt < 1ms, use default

    // Store previous angle
    angle_adjusted_Old = angle_adjusted;

    // Get current angle from IMU complementary filter
    float MPU_sensor_angle = MPU6050_getAngle(dt);
    angle_adjusted = MPU_sensor_angle + angle_offset;

    // Update filtered angle (for calibration — slow average when near upright)
    if ((MPU_sensor_angle > -15) && (MPU_sensor_angle < 15))
      angle_adjusted_filtered = angle_adjusted_filtered * 0.99f + MPU_sensor_angle * 0.01f;

    // ==== HANDLE CALIBRATION REQUEST ====
    if (ws_calibrate_request) {
      ws_calibrate_request = false;
      angle_offset = angle_adjusted_filtered;
      Serial.print("Calibration: new angle_offset = ");
      Serial.println(angle_offset);
    }

    // ==== HANDLE PID UPDATE FROM WEB ====
    if (ws_new_pid) {
      ws_new_pid = false;
      // Values already written to Kp_user, Kd_user, etc. by WebSocket handler
      Serial.printf("PID updated: Kp=%.3f Kd=%.3f Kp_t=%.3f Ki_t=%.3f\n",
                    Kp_user, Kd_user, Kp_thr_user, Ki_thr_user);
    }

    // ==== MAP WEB JOYSTICK TO THROTTLE/STEERING ====
    // Uses deadzone + cubic curve for fine control near center.
    // At 50% joystick → only ~12.5% of max speed (cubic).
    // Configurable in defines.h: JOYSTICK_DEADZONE, THROTTLE_SCALE, JOYSTICK_CURVE
    if (!positionControlMode) {
      float thr = ws_throttle;
      float str = ws_steering;

      // Apply deadzone: ignore small accidental inputs
      if (fabs(thr) < JOYSTICK_DEADZONE) thr = 0.0f;
      else thr = (thr > 0) ? (thr - JOYSTICK_DEADZONE) / (1.0f - JOYSTICK_DEADZONE)
                            : (thr + JOYSTICK_DEADZONE) / (1.0f - JOYSTICK_DEADZONE);

      if (fabs(str) < JOYSTICK_DEADZONE) str = 0.0f;
      else str = (str > 0) ? (str - JOYSTICK_DEADZONE) / (1.0f - JOYSTICK_DEADZONE)
                            : (str + JOYSTICK_DEADZONE) / (1.0f - JOYSTICK_DEADZONE);

      // Apply cubic response curve: preserves sign, gives fine control near center
      float thr_curved = thr * fabs(thr) * fabs(thr);  // x * |x|^2 preserves sign
      float str_curved = str * fabs(str) * fabs(str);

      // Scale and map to motor units
      throttle = (int16_t)(thr_curved * max_throttle * THROTTLE_SCALE);
      steering = str_curved * max_steering * STEERING_SCALE;
    }

    // ==== MODE SWITCHING ====
    if (ws_pro_mode && mode == 0) {
      max_throttle = MAX_THROTTLE_PRO;
      max_steering = MAX_STEERING_PRO;
      max_target_angle = MAX_TARGET_ANGLE_PRO;
      mode = 1;
      Serial.println("Mode: PRO");
    } else if (!ws_pro_mode && mode == 1) {
      max_throttle = MAX_THROTTLE;
      max_steering = MAX_STEERING;
      max_target_angle = MAX_TARGET_ANGLE;
      mode = 0;
      Serial.println("Mode: NORMAL");
    }

    // ==== ESTIMATE ROBOT SPEED ====
    // Robot speed = motor speed - angle_change_rate
    // This is because when the robot leans forward, the angle changes
    // and the motors turn, but the actual robot displacement is the difference.
    actual_robot_speed = (speed_M1 + speed_M2) / 2;  // Average motor speed

    int16_t angular_velocity = (int16_t)((angle_adjusted - angle_adjusted_Old) * 25.0f);
    int16_t estimated_speed = -actual_robot_speed + angular_velocity;
    estimated_speed_filtered = estimated_speed_filtered * 0.9f + (float)estimated_speed * 0.1f;

    // ==== POSITION CONTROL MODE (optional, for precise movements) ====
    if (positionControlMode) {
      motor1_control = positionPDControl(steps1, target_steps1, Kp_position, Kd_position, speed_M1);
      motor2_control = positionPDControl(steps2, target_steps2, Kp_position, Kd_position, speed_M2);

      throttle = (motor1_control + motor2_control) / 2;
      throttle = constrain(throttle, -190, 190);
      steering = motor2_control - motor1_control;
      steering = constrain(steering, -50, 50);
    }

    // ==== SPEED PI CONTROLLER (outer loop) ====
    // Converts desired speed to target lean angle
    target_angle = speedPIControl(dt, estimated_speed_filtered, throttle, Kp_thr, Ki_thr);
    target_angle = constrain(target_angle, -max_target_angle, max_target_angle);

    // ==== STABILITY PD CONTROLLER (inner loop) ====
    // Converts angle error to motor acceleration
    // Note: output is ACCUMULATED (integrated), so PD becomes effectively PID
    control_output += stabilityPDControl(dt, angle_adjusted, target_angle, Kp, Kd);
    control_output = constrain(control_output, -MAX_CONTROL_OUTPUT, MAX_CONTROL_OUTPUT);

    // ==== APPLY STEERING ====
    motor1 = control_output + steering;
    motor2 = control_output - steering;
    motor1 = constrain(motor1, -MAX_CONTROL_OUTPUT, MAX_CONTROL_OUTPUT);
    motor2 = constrain(motor2, -MAX_CONTROL_OUTPUT, MAX_CONTROL_OUTPUT);

    // ==== CHECK IF ROBOT IS UPRIGHT ====
    int angle_ready;
    if (ws_servo_on)
      angle_ready = 82;  // Wider threshold when servo arm is out (shifts center of gravity)
    else
      angle_ready = 74;

    if ((angle_adjusted < angle_ready) && (angle_adjusted > -angle_ready)) {
      // ---- ROBOT IS UPRIGHT: Enable motors and run ----
      motorsActive = true;
      digitalWrite(PIN_ENABLE_MOTORS, LOW);  // Enable motors (active LOW)
      setMotorSpeedM1(motor1);
      setMotorSpeedM2(motor2);
    } else {
      // ---- ROBOT HAS FALLEN: Disable motors and reset ----
      motorsActive = false;
      digitalWrite(PIN_ENABLE_MOTORS, HIGH);  // Disable motors
      setMotorSpeedM1(0);
      setMotorSpeedM2(0);

      // Reset control state
      PID_errorSum = 0;
      control_output = 0;

      // Switch to raise-up gains
      Kp = KP_RAISEUP;
      Kd = KD_RAISEUP;
      Kp_thr = KP_THROTTLE_RAISEUP;
      Ki_thr = KI_THROTTLE_RAISEUP;

      // Reset position control
      steps1 = 0;
      steps2 = 0;
      positionControlMode = false;
      throttle = 0;
      steering = 0;
    }

    // ==== SERVO CONTROL (skip during auto raise-up) ====
    if (autoRaiseup_done) {
      if (ws_servo_on) {
        if (angle_adjusted > -40)
#ifdef ESP32_CORE_V3
          ledcWrite(PIN_SERVO, SERVO_MAX_PULSEWIDTH);
        else
          ledcWrite(PIN_SERVO, SERVO_MIN_PULSEWIDTH);
#else
          ledcWrite(6, SERVO_MAX_PULSEWIDTH);
        else
          ledcWrite(6, SERVO_MIN_PULSEWIDTH);
#endif
      } else {
#ifdef ESP32_CORE_V3
        ledcWrite(PIN_SERVO, SERVO_AUX_NEUTRO);
#else
        ledcWrite(6, SERVO_AUX_NEUTRO);
#endif
      }
    }

    // ==== LED CONTROL ====
    digitalWrite(PIN_LED, ws_led_on ? HIGH : LOW);

    // ==== GAIN SWITCHING: Normal ↔ Raise-up ====
    if ((angle_adjusted < 56) && (angle_adjusted > -56)) {
      // Within normal operating range: use user-set gains
      Kp = Kp_user;
      Kd = Kd_user;
      Kp_thr = Kp_thr_user;
      Ki_thr = Ki_thr_user;
    } else {
      // Large angle: use conservative raise-up gains
      Kp = KP_RAISEUP;
      Kd = KD_RAISEUP;
      Kp_thr = KP_THROTTLE_RAISEUP;
      Ki_thr = KI_THROTTLE_RAISEUP;
    }

    // ==== DEBUG OUTPUT ====
#ifdef DEBUG_ANGLE
    Serial.printf("A:%.1f dt:%.4f out:%.0f m1:%d m2:%d\n",
                  angle_adjusted, dt, control_output, motor1, motor2);
#endif

  }  // end if (MPU6050_newData())

  // ========================================================================
  // AUTO RAISE-UP — Attempt to stand up after boot
  // ========================================================================
  if (!autoRaiseup_done) {
    handleAutoRaiseup();
  }

  // ========================================================================
  // MORSE CODE PLAYER — Non-blocking tick
  // ========================================================================
  if (morseActive) {
    tickMorse();
  }

  // ========================================================================
  // TELEMETRY — Send to WebSocket clients at ~10Hz
  // ========================================================================
  unsigned long now = millis();
  if (now - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs = now;
    sendTelemetry();
  }
}

// ============================================================================
// IMU INITIALIZATION
// ============================================================================
void initMPU6050() {
  MPU6050_setup();
  delay(500);
  MPU6050_calibrate();
}

// ============================================================================
// WIFI ACCESS POINT SETUP
// ============================================================================
void setupWiFiAP() {
  Serial.println("Setting up WiFi Access Point...");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  digitalWrite(PIN_WIFI_LED, HIGH);  // LED on = AP active
}

// ============================================================================
// WEB SERVER SETUP
// ============================================================================
void setupWebServer() {
  // Serve the main page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", WEB_PAGE);
  });

  // Attach WebSocket handler
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  // 404 handler
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("Web server started on port 80");
}

// ============================================================================
// WEBSOCKET EVENT HANDLER
// ============================================================================
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n",
                    client->id(), client->remoteIP().toString().c_str());
      ws_connected = true;
      break;

    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      // Safety: stop the robot when client disconnects
      ws_throttle = 0;
      ws_steering = 0;
      ws_connected = (ws.count() > 0);
      break;

    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo *)arg;
      if (info->final && info->index == 0 && info->len == len) {
        // Complete message received
        if (info->opcode == WS_TEXT) {
          data[len] = 0;  // Null-terminate
          processWebSocketMessage(data, len);
        }
      }
      break;
    }

    case WS_EVT_ERROR:
      Serial.printf("WebSocket error from client #%u\n", client->id());
      break;

    case WS_EVT_PONG:
      break;
  }
}

// ============================================================================
// PROCESS INCOMING WEBSOCKET MESSAGES
// ============================================================================
// Protocol:
//   J:<throttle>:<steering>   Joystick (-1.0 to 1.0)
//   M:<0|1>                   Mode
//   P:<kp>:<kd>:<kpt>:<kit>   PID parameters
//   C                         Calibrate
//   S:<0|1>                   Servo
//   L:<0|1>                   LED
//   B                         Buzzer
void processWebSocketMessage(uint8_t *data, size_t len) {
  char *msg = (char *)data;

  if (len < 1) return;

  switch (msg[0]) {
    case 'J': {
      // Joystick: J:<throttle>:<steering>
      float t = 0, s = 0;
      if (sscanf(msg, "J:%f:%f", &t, &s) == 2) {
        ws_throttle = constrain(t, -1.0f, 1.0f);
        ws_steering = constrain(s, -1.0f, 1.0f);
      }
      break;
    }

    case 'M': {
      // Mode: M:<0|1>
      int m = 0;
      if (sscanf(msg, "M:%d", &m) == 1) {
        ws_pro_mode = (m == 1);
      }
      break;
    }

    case 'P': {
      // PID: P:<kp>:<kd>:<kpt>:<kit>
      float kp, kd, kpt, kit;
      if (sscanf(msg, "P:%f:%f:%f:%f", &kp, &kd, &kpt, &kit) == 4) {
        Kp_user = kp;
        Kd_user = kd;
        Kp_thr_user = kpt;
        Ki_thr_user = kit;
        ws_new_pid = true;
      }
      break;
    }

    case 'C':
      // Calibrate
      ws_calibrate_request = true;
      Serial.println("Calibration requested from web");
      break;

    case 'S': {
      // Servo: S:<0|1>
      int v = 0;
      if (sscanf(msg, "S:%d", &v) == 1) {
        ws_servo_on = (v == 1);
      }
      break;
    }

    case 'L': {
      // LED: L:<0|1>
      int v = 0;
      if (sscanf(msg, "L:%d", &v) == 1) {
        ws_led_on = (v == 1);
      }
      break;
    }

    case 'B':
      // Buzzer: short beep pattern
      digitalWrite(PIN_BUZZER, HIGH);
      delay(80);
      digitalWrite(PIN_BUZZER, LOW);
      delay(40);
      digitalWrite(PIN_BUZZER, HIGH);
      delay(80);
      digitalWrite(PIN_BUZZER, LOW);
      break;

    case 'W':
      // Morse code "UTN": W command (from web UI)
      if (msg[0] == 'W' && !morseActive) {
        morseActive = true;
        morseStep = 0;
        morseTimer = millis();
        digitalWrite(PIN_BUZZER, HIGH);  // Start first element
        Serial.println("Morse: UTN");
      }
      break;
  }
}

// ============================================================================
// SEND TELEMETRY TO ALL CONNECTED WEBSOCKET CLIENTS
// ============================================================================
// Format: A:<angle>:<speed>:<motors_active>
void sendTelemetry() {
  if (ws.count() == 0) return;  // No clients connected

  char buf[64];
  snprintf(buf, sizeof(buf), "A:%.1f:%d:%d",
           angle_adjusted,
           actual_robot_speed,
           motorsActive ? 1 : 0);

  ws.textAll(buf);
}

// ============================================================================
// AUTO RAISE-UP — Non-blocking state machine
// ============================================================================
// After boot, the robot is lying flat. This function uses the servo arm to
// push itself upright. The sequence:
//   1. WAIT: Delay after boot to let things settle
//   2. PUSH: Swing servo arm to push robot toward upright
//   3. RETRACT: Pull servo back and check if balance caught
//   4. DONE: Either balanced or gave up after max retries
//
// The servo arm acts as a lever against the ground. The direction of push
// depends on which side the robot is lying on (positive or negative angle).
void handleAutoRaiseup() {
  unsigned long now = millis();

  switch (raiseupState) {

    case RAISEUP_WAIT:
      // Wait for the delay after boot
      if (now - autoRaiseup_timer >= AUTO_RAISEUP_DELAY_MS) {
        // Check if robot is already upright (held by user)
        if (fabs(angle_adjusted) < 40) {
          Serial.println("Auto raise-up: Robot already upright, skipping.");
          autoRaiseup_done = true;
          raiseupState = RAISEUP_DONE;
          return;
        }

        if (autoRaiseup_attempts >= AUTO_RAISEUP_RETRIES) {
          Serial.println("Auto raise-up: Max retries reached. Stand it up manually.");
          autoRaiseup_done = true;
          raiseupState = RAISEUP_DONE;
          // Triple beep to signal failure
          for (int i = 0; i < 3; i++) {
            digitalWrite(PIN_BUZZER, HIGH); delay(200);
            digitalWrite(PIN_BUZZER, LOW);  delay(100);
          }
          return;
        }

        // Start a push attempt
        autoRaiseup_attempts++;
        Serial.printf("Auto raise-up: Attempt %d/%d (angle=%.1f)\n",
                       autoRaiseup_attempts, AUTO_RAISEUP_RETRIES, angle_adjusted);

        // Push servo in the direction that lifts the robot
        // If angle > 0 (leaning forward), push with max pulse
        // If angle < 0 (leaning backward), push with min pulse
        if (angle_adjusted > 0) {
#ifdef ESP32_CORE_V3
          ledcWrite(PIN_SERVO, SERVO_MAX_PULSEWIDTH);
#else
          ledcWrite(6, SERVO_MAX_PULSEWIDTH);
#endif
        } else {
#ifdef ESP32_CORE_V3
          ledcWrite(PIN_SERVO, SERVO_MIN_PULSEWIDTH);
#else
          ledcWrite(6, SERVO_MIN_PULSEWIDTH);
#endif
        }

        autoRaiseup_timer = now;
        raiseupState = RAISEUP_PUSH;
      }
      break;

    case RAISEUP_PUSH:
      // Wait for the push duration
      if (now - autoRaiseup_timer >= AUTO_RAISEUP_PUSH_MS) {
        // Retract servo back to neutral
#ifdef ESP32_CORE_V3
        ledcWrite(PIN_SERVO, SERVO_AUX_NEUTRO);
#else
        ledcWrite(6, SERVO_AUX_NEUTRO);
#endif
        autoRaiseup_timer = now;
        raiseupState = RAISEUP_RETRACT;
      }
      break;

    case RAISEUP_RETRACT:
      // Give the robot 1 second to stabilize after the push
      if (now - autoRaiseup_timer >= 1000) {
        if (fabs(angle_adjusted) < 60) {
          // Success! The balance controller should be catching it now
          Serial.println("Auto raise-up: SUCCESS! Robot is balancing.");
          autoRaiseup_done = true;
          raiseupState = RAISEUP_DONE;
          // Victory beep
          digitalWrite(PIN_BUZZER, HIGH); delay(50);
          digitalWrite(PIN_BUZZER, LOW);  delay(30);
          digitalWrite(PIN_BUZZER, HIGH); delay(50);
          digitalWrite(PIN_BUZZER, LOW);
        } else {
          // Didn't make it — try again
          Serial.printf("Auto raise-up: Attempt %d failed (angle=%.1f), retrying...\n",
                         autoRaiseup_attempts, angle_adjusted);
          autoRaiseup_timer = now;
          raiseupState = RAISEUP_WAIT;  // Go back to wait (will check retries)
        }
      }
      break;

    case RAISEUP_DONE:
      autoRaiseup_done = true;
      break;
  }
}

// ============================================================================
// MORSE CODE TICK — Non-blocking player
// ============================================================================
// Steps through morseUTN[] array. Even indices = ON duration, odd = OFF duration.
// When an OFF duration is 0, the sequence is complete.
void tickMorse() {
  if (!morseActive) return;
  unsigned long now = millis();

  if (now - morseTimer >= morseUTN[morseStep]) {
    morseStep++;

    if (morseStep >= morseLen) {
      // Sequence complete
      digitalWrite(PIN_BUZZER, LOW);
      morseActive = false;
      return;
    }

    if (morseStep % 2 == 0) {
      // Even index = ON
      digitalWrite(PIN_BUZZER, HIGH);
    } else {
      // Odd index = OFF
      digitalWrite(PIN_BUZZER, LOW);
      // Check if this OFF duration is 0 (end marker)
      if (morseUTN[morseStep] == 0) {
        morseActive = false;
        return;
      }
    }
    morseTimer = now;
  }
}
