#ifndef MMA8452Q_H
#define MMA8452Q_H

#include "processor_hal.h"
#include <stdint.h>

// --- Addresses ---
#define MMA_ADDR_GND 0x1C
#define MMA_ADDR_VCC 0x1D

// --- Physics Constants ---
#define DT 0.033f

// HAND TUNING (Smooth, fluid gestures)
#define HAND_DAMPING 0.95f     // 95% velocity retained (smooth glide)
#define HAND_SPRING  0.03f     // 3% pull to center per frame (loose rubber band)
#define HAND_SENSITIVITY 150.0f // MASSIVE boost to translate tiny Gs to usable coordinates

// HEAD TUNING (Tight, responsive camera control)
#define HEAD_DAMPING 0.85f     // 85% velocity retained (stops faster)
#define HEAD_SPRING  0.08f     // 8% pull to center (snaps back to center quickly)
#define HEAD_SENSITIVITY 200.0f // Huge boost to catch subtle neck movements

// SENSOR NOISE FILTER
#define DEADZONE 0.02f         // Lowered from 0.05. Allows slow movements to register

// --- STRUCT DEFINITIONS (Crucial for fixing your error) ---
typedef struct {
    float x, y, z;
    float vel_x, vel_y, vel_z;
    float last_ax, last_ay, last_az;
} HandState;

typedef struct {
    float x, z;             // Current rotation/position
    float vel_x, vel_z;     // Velocity
    float last_ax, last_az; // For calculating delta (mx, mz)
} HeadState;

// --- Function Prototypes ---
void mma8452q_init(I2C_TypeDef* i2c, uint8_t dev_addr);
void mma8452q_get_accel(I2C_TypeDef* i2c, uint8_t dev_addr, float *accel_data);
uint8_t mma8452q_reg_read(I2C_TypeDef* i2c, uint8_t dev_addr, uint8_t reg_address);
void mma8452q_reg_write(I2C_TypeDef* i2c, uint8_t dev_addr, uint8_t reg_address, uint8_t reg_value);

// Prototypes for the physics logic
void update_hand_logic(HandState* hand, float* raw);
void update_head_logic(HeadState* h, float* raw);

#endif