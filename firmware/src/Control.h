/*
 * Control.h
 * 
 * PID controller functions for the balancing robot.
 * Three controllers work in cascade:
 * 
 *   1. Speed PI Controller (outer loop, ~25Hz):
 *      Desired speed → target lean angle
 * 
 *   2. Stability PD Controller (inner loop, 100Hz):
 *      Target lean angle → motor acceleration
 * 
 *   3. Position PD Controller (optional):
 *      Target position → motor speed commands
 */

#ifndef CONTROL_H_
#define CONTROL_H_

// Stability PD Controller: Keeps the robot upright
// Input: current angle + target angle → Output: motor acceleration
float stabilityPDControl(float DT, float input, float setPoint, float Kp, float Kd);

// Speed PI Controller: Controls forward/backward speed
// Input: estimated speed + desired speed → Output: target lean angle
float speedPIControl(float DT, int16_t input, int16_t setPoint, float Kp, float Ki);

// Position PD Controller: Drives to a target step count
// Input: actual steps + target steps → Output: motor speed command
float positionPDControl(long actualPos, long setPointPos, float Kpp, float Kdp, int16_t speedM);

#endif /* CONTROL_H_ */
