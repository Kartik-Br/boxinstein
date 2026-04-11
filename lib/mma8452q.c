/**
 ******************************************************************************
 * @file    s4/main.c
 * @author  Zhiyong
 * @date    26/03/2026
 * @brief   I2C example with the MMA8462Q. Reads and checks the WHO_AM_I
 *          register (Address: 0x0D) for the set value (0x2A). If the value is
 *          correct, the green LED is toggled.
 *  		REFERENCE: MMA8462 Datasheet (p15)
 * 			Uses D15 (SCL) & D14 (SDA)
 *			Uses the following macros:
 * 				 MODIFY_REG( Register, clear mask (bits to clear), set mask
 *(bits to set)) WRITE_REG( Register, value to set) READ_REG( (Register) SET_BIT
 *(Register, bit mask to set) CLEAR_BIT (Register, bit mask to clear)
 ******************************************************************************
 */

#include "mma8452q.h"
#include <math.h>

/**
 * @brief Reads a single byte from a specific register on a specific sensor
 * @param i2c The I2C peripheral (e.g., I2C1 or I2C2)
 * @param dev_addr The 7-bit device address (0x1C or 0x1D)
 * @param reg_address The register to read from
 */
uint8_t mma8452q_reg_read(I2C_TypeDef* i2c, uint8_t dev_addr, uint8_t reg_address) {

    uint8_t reg_value = 0;
    uint32_t tickstart = HAL_GetTick();
    uint8_t addr_write = (dev_addr << 1);          // Shift for I2C Write bit (0)
    uint8_t addr_read  = (dev_addr << 1) | 0x01;   // Shift for I2C Read bit (1)

    // 1. Generate START condition
    SET_BIT(i2c->CR1, I2C_CR1_START); 
    while ((READ_REG(i2c->SR1) & I2C_SR1_SB) == 0) {
        if ((HAL_GetTick() - tickstart) > 10) return 0; // Timeout
    }

    // 2. Send Device Address (Write mode)
    WRITE_REG(i2c->DR, addr_write);
    while ((READ_REG(i2c->SR1) & I2C_SR1_ADDR) == 0) {
        if ((HAL_GetTick() - tickstart) > 10) return 0;
    }
    (void)i2c->SR2; // Clear ADDR flag

    // 3. Send Register Address
    WRITE_REG(i2c->DR, reg_address);
    while ((READ_REG(i2c->SR1) & I2C_SR1_TXE) == 0);

    // 4. Generate REPEATED START
    SET_BIT(i2c->CR1, I2C_CR1_START);
    while ((READ_REG(i2c->SR1) & I2C_SR1_SB) == 0);

    // 5. Send Device Address (Read mode)
    WRITE_REG(i2c->DR, addr_read);
    while ((READ_REG(i2c->SR1) & I2C_SR1_ADDR) == 0);

    // 6. NACK and STOP (We only want 1 byte)
    CLEAR_BIT(i2c->CR1, I2C_CR1_ACK);
    (void)i2c->SR2; // Clear ADDR flag
    SET_BIT(i2c->CR1, I2C_CR1_STOP);

    // 7. Wait for data and read it
    while ((READ_REG(i2c->SR1) & I2C_SR1_RXNE) == 0);
    reg_value = READ_REG(i2c->DR);

    return reg_value;
}

/**
 * @brief Writes a single byte to a specific register on a specific sensor
 */
void mma8452q_reg_write(I2C_TypeDef* i2c, uint8_t dev_addr, uint8_t reg_address, uint8_t reg_value) {

    uint32_t timeout = 100000;
    uint8_t addr_write = (dev_addr << 1);

    // 1. START
    SET_BIT(i2c->CR1, I2C_CR1_START);
    while (!(READ_REG(i2c->SR1) & I2C_SR1_SB)) if (--timeout == 0) return;

    // 2. Device Address
    timeout = 100000;
    WRITE_REG(i2c->DR, addr_write);
    while (!(READ_REG(i2c->SR1) & I2C_SR1_ADDR)) if (--timeout == 0) return;
    (void)i2c->SR2; 

    // 3. Register Address
    timeout = 100000;
    WRITE_REG(i2c->DR, reg_address);
    while (!(READ_REG(i2c->SR1) & I2C_SR1_TXE)) if (--timeout == 0) return;

    // 4. Send Data
    WRITE_REG(i2c->DR, reg_value);
    while (!((READ_REG(i2c->SR1) & I2C_SR1_TXE) && (READ_REG(i2c->SR1) & I2C_SR1_BTF))) 
        if (--timeout == 0) break;

    // 5. STOP
    SET_BIT(i2c->CR1, I2C_CR1_STOP);
}
/**
 * @brief Initialize the sensor into ACTIVE mode
 */
void mma8452q_init(I2C_TypeDef* i2c, uint8_t dev_addr) {
    // CTRL_REG1 (0x2A) = 0x01 (Active Mode, 800Hz ODR)
    mma8452q_reg_write(i2c, dev_addr, 0x2A, 0x01);
}

/**
 * @brief Reads all 6 axes and converts to float G-force
 */
void mma8452q_get_accel(I2C_TypeDef* i2c, uint8_t dev_addr, float *accel_data) {
    uint8_t buf[6];
    uint32_t timeout = 100000; // Basic loop counter timeout
    uint8_t addr_write = (dev_addr << 1);
    uint8_t addr_read  = (dev_addr << 1) | 0x01;

    // --- STEP 1: Send the Register Address (0x01) ---
    SET_BIT(i2c->CR1, I2C_CR1_START);
    while (!(READ_REG(i2c->SR1) & I2C_SR1_SB)) if (--timeout == 0) return;

    WRITE_REG(i2c->DR, addr_write);
    while (!(READ_REG(i2c->SR1) & I2C_SR1_ADDR)) if (--timeout == 0) return;
    (void)i2c->SR2; // Clear ADDR flag

    WRITE_REG(i2c->DR, 0x01); // OUT_X_MSB register
    while (!(READ_REG(i2c->SR1) & I2C_SR1_TXE)) if (--timeout == 0) return;

    // --- STEP 2: Repeated Start to Read 6 Bytes ---
    SET_BIT(i2c->CR1, I2C_CR1_START);
    timeout = 100000;
    while (!(READ_REG(i2c->SR1) & I2C_SR1_SB)) if (--timeout == 0) return;

    WRITE_REG(i2c->DR, addr_read);
    while (!(READ_REG(i2c->SR1) & I2C_SR1_ADDR)) if (--timeout == 0) return;
    (void)i2c->SR2; // Clear ADDR flag

    // Enable Acknowledgement for burst reading
    SET_BIT(i2c->CR1, I2C_CR1_ACK);

    for (int i = 0; i < 5; i++) {
        timeout = 100000;
        while (!(READ_REG(i2c->SR1) & I2C_SR1_RXNE)) if (--timeout == 0) return;
        buf[i] = READ_REG(i2c->DR);
    }

    // --- STEP 3: The Last Byte (NACK and STOP) ---
    CLEAR_BIT(i2c->CR1, I2C_CR1_ACK); // Must NACK the last byte
    SET_BIT(i2c->CR1, I2C_CR1_STOP);
    
    timeout = 100000;
    while (!(READ_REG(i2c->SR1) & I2C_SR1_RXNE)) if (--timeout == 0) return;
    buf[5] = READ_REG(i2c->DR);

    // --- STEP 4: Data Conversion ---
    // MMA8452Q is 12-bit left-justified. 
    // Combine High (8 bits) and Low (4 bits from the top of the second byte)
    int16_t x = ((int16_t)(buf[0] << 8) | buf[1]) >> 4;
    int16_t y = ((int16_t)(buf[2] << 8) | buf[3]) >> 4;
    int16_t z = ((int16_t)(buf[4] << 8) | buf[5]) >> 4;

    // Convert to G-force (Assuming +/- 2g range where 1g = 1024 counts)
    accel_data[0] = (float)x / 1024.0f;
    accel_data[1] = (float)y / 1024.0f;
    accel_data[2] = (float)z / 1024.0f;
}

void update_hand_logic(HandState* hand, float* raw) {
    float mx = raw[0] - hand->last_ax;
    float my = raw[1] - hand->last_ay;
    float mz = raw[2] - hand->last_az;
    hand->last_ax = raw[0]; hand->last_ay = raw[1]; hand->last_az = raw[2];

    if(fabs(mx) < 0.15f) mx = 0;
    if(fabs(my) < 0.15f) my = 0;
    if(fabs(mz) < 0.15f) mz = 0;

    hand->vel_x = (hand->vel_x + mx * DT) * HAND_DAMPING;
    hand->vel_y = (hand->vel_y + my * DT) * HAND_DAMPING;
    hand->vel_z = (hand->vel_z + mz * DT) * HAND_DAMPING;

    hand->x = (hand->x + hand->vel_x) * (1.0f - HAND_SPRING);
    hand->y = (hand->y + hand->vel_y) * (1.0f - HAND_SPRING);
    hand->z = (hand->z + hand->vel_z) * (1.0f - HAND_SPRING);
}

void update_head_logic(HeadState* h, float* raw) {
    // 1. Calculate the target angles (assuming Z is gravity/vertical)
    // Use raw[2] (Z) as the reference if the sensor is lying flat
    float target_x = atan2f(raw[0], raw[2]) * (180.0f / 3.14159f) / HEAD_SENSITIVITY;
    float target_z = atan2f(raw[1], raw[2]) * (180.0f / 3.14159f) / HEAD_SENSITIVITY;

    // 2. Simple Low Pass Filter (Smoothing)
    // NewValue = (OldValue * 0.85) + (Target * 0.15)
    h->x = (h->x * (1.0f - HEAD_SMOOTHING)) + (target_x * HEAD_SMOOTHING);
    h->z = (h->z * (1.0f - HEAD_SMOOTHING)) + (target_z * HEAD_SMOOTHING);

    // 3. Clamp
    if(h->x > 1.0f) h->x = 1.0f;   if(h->x < -1.0f) h->x = -1.0f;
    if(h->z > 1.0f) h->z = 1.0f;   if(h->z < -1.0f) h->z = -1.0f;
}