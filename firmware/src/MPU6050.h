/*
 * MPU6050.h
 * 
 * MPU-6050 IMU driver for the balancing robot.
 * Provides complementary filter fusion of accelerometer and gyroscope data
 * to produce a stable tilt angle estimate.
 * 
 * Based on the B-Robot EVO2 by JJROBOTS.
 * Reference: "MPU-6000 and MPU-6050 Register Map and Descriptions" (RM-MPU-6000A.pdf)
 */

#ifndef MPU6050_H_
#define MPU6050_H_

#include <Arduino.h>

// ============================================================================
// CONVERSION CONSTANTS
// ============================================================================
#define RAD2GRAD 57.2957795f
#define GRAD2RAD 0.01745329251994329576923690768489f

// ============================================================================
// MPU6050 I2C ADDRESS
// ============================================================================
// Default address is 0x68 (AD0 pin LOW). If AD0 is HIGH, address is 0x69.
#define MPU6050_I2C_ADDRESS 0x68

// ============================================================================
// MPU6050 REGISTER MAP (relevant registers only)
// ============================================================================
#define MPU6050_SMPLRT_DIV    0x19   // Sample Rate Divider
#define MPU6050_CONFIG        0x1A   // Configuration (DLPF)
#define MPU6050_GYRO_CONFIG   0x1B   // Gyroscope Configuration
#define MPU6050_ACCEL_CONFIG  0x1C   // Accelerometer Configuration
#define MPU6050_INT_ENABLE    0x38   // Interrupt Enable
#define MPU6050_INT_STATUS    0x3A   // Interrupt Status
#define MPU6050_ACCEL_XOUT_H  0x3B   // Accelerometer X-axis High Byte
#define MPU6050_GYRO_XOUT_H   0x43   // Gyroscope X-axis High Byte
#define MPU6050_PWR_MGMT_1    0x6B   // Power Management 1
#define MPU6050_WHO_AM_I      0x75   // Who Am I (device ID)

// ============================================================================
// BIT DEFINITIONS
// ============================================================================
#define MPU6050_D0 0
#define MPU6050_D1 1
#define MPU6050_D2 2
#define MPU6050_D3 3
#define MPU6050_D4 4
#define MPU6050_D5 5
#define MPU6050_D6 6
#define MPU6050_D7 7

// Power Management 1 bits
#define MPU6050_DEVICE_RESET MPU6050_D7
#define MPU6050_SLEEP        MPU6050_D6

// DLPF Configuration values
#define MPU6050_DLPF_10HZ   (bit(MPU6050_D2) | bit(MPU6050_D0))  // ~10Hz bandwidth

// Gyroscope Full Scale Range
#define MPU6050_FS_SEL_500   bit(MPU6050_D3)  // ±500°/s → 65.5 LSB/°/s

// Accelerometer Full Scale Range
#define MPU6050_AFS_SEL_2G   0  // ±2g

// Interrupt Enable
#define MPU6050_DATA_RDY_EN  MPU6050_D0

// ============================================================================
// DATA UNION — Maps raw I2C bytes to named sensor values
// ============================================================================
// The MPU6050 sends data in big-endian (High byte first), but ESP32 is little-endian.
// The "reg" struct holds raw bytes which get swapped in code to produce correct "value" readings.
typedef union accel_t_gyro_union {
  struct {
    uint8_t x_accel_h;
    uint8_t x_accel_l;
    uint8_t y_accel_h;
    uint8_t y_accel_l;
    uint8_t z_accel_h;
    uint8_t z_accel_l;
    uint8_t t_h;
    uint8_t t_l;
    uint8_t x_gyro_h;
    uint8_t x_gyro_l;
    uint8_t y_gyro_h;
    uint8_t y_gyro_l;
    uint8_t z_gyro_h;
    uint8_t z_gyro_l;
  } reg;
  struct {
    int16_t x_accel;
    int16_t y_accel;
    int16_t z_accel;
    int16_t temperature;
    int16_t x_gyro;
    int16_t y_gyro;
    int16_t z_gyro;
  } value;
} accel_t_gyro_union;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================
void  MPU6050_setup();               // Initialize MPU6050 registers
void  MPU6050_calibrate();           // Calibrate gyro offset (robot must be still!)
float MPU6050_getAngle(float dt);    // Get filtered angle using complementary filter
bool  MPU6050_newData();             // Check if new sensor data is available
void  MPU6050_read_3axis();          // Read all 3 axes of accel + gyro
int   MPU6050_read(int start, uint8_t *buffer, int size);   // Low-level I2C read
int   MPU6050_write(int start, const uint8_t *pData, int size); // Low-level I2C write
int   MPU6050_write_reg(int reg, uint8_t data);             // Write single register

#endif /* MPU6050_H_ */
