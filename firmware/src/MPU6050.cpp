/*
 * MPU6050.cpp
 * 
 * MPU-6050 IMU driver implementation.
 * 
 * Key algorithm: Complementary Filter
 * ==================================
 * The gyroscope provides accurate short-term angle changes but drifts over time.
 * The accelerometer provides a stable long-term angle reference but is noisy.
 * The complementary filter fuses both:
 *   angle = 0.99 * (angle + gyro_rate * dt) + 0.01 * accel_angle
 * 
 * This gives us the best of both: short-term accuracy from the gyro and 
 * long-term stability from the accelerometer, with a time constant of ~1 second.
 */

#include "MPU6050.h"
#include "Wire.h"

// Global IMU state
accel_t_gyro_union accel_t_gyro;
float x_gyro_value;           // Gyro rate in deg/s
float x_gyro_offset = 0.0f;  // Gyro bias (determined during calibration)
float accel_angle;            // Angle from accelerometer (degrees)
float angle;                  // Fused angle output (degrees)

// Byte swap helper
uint8_t swap;
#define SWAP(x, y) swap = x; x = y; y = swap

// ============================================================================
// MPU6050_getAngle — Complementary filter for angle estimation
// ============================================================================
// Called at ~100Hz. Returns the estimated tilt angle in degrees.
// Positive angle = tilting forward (toward servo/head side)
// Negative angle = tilting backward
float MPU6050_getAngle(float dt) {
  // Calculate angle from accelerometer using atan2(y_accel, z_accel)
  // This gives the angle of the gravity vector relative to the Z axis
  accel_angle = atan2f((float)accel_t_gyro.value.y_accel,
                       (float)accel_t_gyro.value.z_accel) * RAD2GRAD;

  // Convert raw gyro to deg/s (at ±500°/s range, sensitivity = 65.5 LSB/(°/s))
  x_gyro_value = (accel_t_gyro.value.x_gyro - x_gyro_offset) / 65.5f;

  // Complementary filter: 99% gyro integration + 1% accelerometer
  // Time constant ≈ 0.99 * sample_period / (1 - 0.99) ≈ 1 second at 100Hz
  angle = 0.99f * (angle + x_gyro_value * dt) + 0.01f * accel_angle;

  // Slow gyro bias correction
  // The gyro offset should converge to the true bias over ~20 seconds.
  // We clamp corrections to prevent large jumps from vibration/movement.
  int16_t correction = constrain(accel_t_gyro.value.x_gyro,
                                 (int16_t)(x_gyro_offset - 10),
                                 (int16_t)(x_gyro_offset + 10));
  x_gyro_offset = x_gyro_offset * 0.9995f + correction * 0.0005f;

  return angle;
}

// ============================================================================
// MPU6050_calibrate — Determine gyro bias at startup
// ============================================================================
// Takes 100 readings over ~2.5 seconds to find the average gyro offset.
// THE ROBOT MUST BE COMPLETELY STILL DURING THIS PROCESS.
// If the standard deviation is too high (movement detected), it retries.
void MPU6050_calibrate() {
  int i;
  long value = 0;
  float dev;
  int16_t values[100];
  bool gyro_cal_ok = false;

  delay(500);  // Let the sensor settle

  while (!gyro_cal_ok) {
    Serial.println("Gyro calibration... DONT MOVE!");
    value = 0;

    // Take 100 samples
    for (i = 0; i < 100; i++) {
      MPU6050_read_3axis();
      values[i] = accel_t_gyro.value.x_gyro;
      value += accel_t_gyro.value.x_gyro;
      delay(25);  // 25ms between samples → 2.5 seconds total
    }

    // Calculate mean
    value = value / 100;

    // Calculate standard deviation to verify the sensor was still
    dev = 0;
    for (i = 0; i < 100; i++)
      dev += (float)(values[i] - value) * (float)(values[i] - value);
    dev = sqrt(dev / 100.0f);

    Serial.print("Gyro offset: ");
    Serial.print(value);
    Serial.print("  stddev: ");
    Serial.println(dev);

    if (dev < 50.0f) {
      gyro_cal_ok = true;
      Serial.println("Calibration OK!");
    } else {
      Serial.println("Too much movement! Retrying... DONT MOVE!");
    }
  }

  x_gyro_offset = value;

  // Initialize angle from accelerometer (first reading)
  angle = atan2f((float)accel_t_gyro.value.y_accel,
                 (float)accel_t_gyro.value.z_accel) * RAD2GRAD;
}

// ============================================================================
// MPU6050_setup — Initialize sensor registers
// ============================================================================
void MPU6050_setup() {
  int error;
  uint8_t c;

  // Verify communication by reading WHO_AM_I register (should return 0x68)
  error = MPU6050_read(MPU6050_WHO_AM_I, &c, 1);
  Serial.print("MPU6050 WHO_AM_I: 0x");
  Serial.print(c, HEX);
  Serial.print(" (expected 0x68), error=");
  Serial.println(error);

  // Reset the sensor to default state
  MPU6050_write_reg(MPU6050_PWR_MGMT_1, bit(MPU6050_DEVICE_RESET));
  delay(125);

  // Wake up and select PLL with X-axis gyro as clock source (more stable than internal oscillator)
  MPU6050_write_reg(MPU6050_PWR_MGMT_1, 0x01);

  // Configure gyroscope: ±500°/s range
  MPU6050_write_reg(MPU6050_GYRO_CONFIG, MPU6050_FS_SEL_500);

  // Configure accelerometer: ±2g range
  MPU6050_write_reg(MPU6050_ACCEL_CONFIG, MPU6050_AFS_SEL_2G);

  // Configure Digital Low Pass Filter: 10Hz
  // This heavily filters sensor noise but adds ~30ms latency.
  // Acceptable for a balancing robot running at 100Hz.
  MPU6050_write_reg(MPU6050_CONFIG, MPU6050_DLPF_10HZ);

  // Set sample rate to 100Hz: Rate = 1000 / (1 + SMPLRT_DIV)
  // With SMPLRT_DIV=9: Rate = 1000/(1+9) = 100Hz
  MPU6050_write_reg(MPU6050_SMPLRT_DIV, 9);

  // Enable data-ready interrupt
  MPU6050_write_reg(MPU6050_INT_ENABLE, bit(MPU6050_DATA_RDY_EN));

  // Final wake-up (redundant but ensures sensor is running)
  MPU6050_write_reg(MPU6050_PWR_MGMT_1, 0x01);
}

// ============================================================================
// MPU6050_read_3axis — Read all accelerometer and gyroscope data (14 bytes)
// ============================================================================
void MPU6050_read_3axis() {
  int error;

  // Read 14 bytes starting from ACCEL_XOUT_H: 6 accel + 2 temp + 6 gyro
  error = MPU6050_read(MPU6050_ACCEL_XOUT_H, (uint8_t *)&accel_t_gyro, sizeof(accel_t_gyro));

  if (error != 0) {
    // Silently ignore errors — occasional I2C glitches are normal
    return;
  }

  // Swap bytes from big-endian (MPU6050) to little-endian (ESP32)
  SWAP(accel_t_gyro.reg.x_accel_h, accel_t_gyro.reg.x_accel_l);
  SWAP(accel_t_gyro.reg.y_accel_h, accel_t_gyro.reg.y_accel_l);
  SWAP(accel_t_gyro.reg.z_accel_h, accel_t_gyro.reg.z_accel_l);
  SWAP(accel_t_gyro.reg.t_h, accel_t_gyro.reg.t_l);
  SWAP(accel_t_gyro.reg.x_gyro_h, accel_t_gyro.reg.x_gyro_l);
  SWAP(accel_t_gyro.reg.y_gyro_h, accel_t_gyro.reg.y_gyro_l);
  SWAP(accel_t_gyro.reg.z_gyro_h, accel_t_gyro.reg.z_gyro_l);
}

// ============================================================================
// MPU6050_newData — Check if new sensor data is available
// ============================================================================
bool MPU6050_newData() {
  uint8_t status;
  int error;

  error = MPU6050_read(MPU6050_INT_STATUS, &status, 1);
  if (error != 0) return false;

  return (status & 0x01);  // Data ready bit
}

// ============================================================================
// LOW-LEVEL I2C FUNCTIONS
// ============================================================================

int MPU6050_read(int start, uint8_t *buffer, int size) {
  int i, n;

  Wire.beginTransmission(MPU6050_I2C_ADDRESS);
  n = Wire.write(start);
  if (n != 1) return -10;

  n = Wire.endTransmission(false);  // Repeated start (don't release bus)
  if (n != 0) return n;

  Wire.requestFrom(MPU6050_I2C_ADDRESS, size, true);
  i = 0;
  while (Wire.available() && i < size) {
    buffer[i++] = Wire.read();
  }
  if (i != size) return -11;

  return 0;
}

int MPU6050_write(int start, const uint8_t *pData, int size) {
  int n, error;

  Wire.beginTransmission(MPU6050_I2C_ADDRESS);
  n = Wire.write(start);
  if (n != 1) return -20;

  n = Wire.write(pData, size);
  if (n != size) return -21;

  error = Wire.endTransmission(true);
  if (error != 0) return error;

  return 0;
}

int MPU6050_write_reg(int reg, uint8_t data) {
  return MPU6050_write(reg, &data, 1);
}
