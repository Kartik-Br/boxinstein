#ifndef MMA8452Q_H
#define MMA8452Q_H

#include "processor_hal.h"
#include <stdint.h>

// --- Addresses ---
#define MMA_ADDR_GND 0x1C
#define MMA_ADDR_VCC 0x1D

// --- Physics Constants ---
#define DT 0.016f
#define HAND_DAMPING 0.85f
#define HAND_SPRING 0.15f
#define HEAD_SENSITIVITY 30.0f

// --- STRUCT DEFINITIONS (Crucial for fixing your error) ---
typedef struct {
    float x, y, z;
    float vel_x, vel_y, vel_z;
    float last_ax, last_ay, last_az;
} HandState;

typedef struct {
    float x, z;
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