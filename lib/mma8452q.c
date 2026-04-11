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

/*
void update_hand_logic(HandState* hand, float* raw) {
    float mx = raw[0] - hand->last_ax;
    float my = raw[1] - hand->last_ay;
    float mz = raw[2] - hand->last_az;
    hand->last_ax = raw[0]; hand->last_ay = raw[1]; hand->last_az = raw[2];

    if(fabs(mx) < 0.06f) mx = 0;
    if(fabs(my) < 0.06f) my = 0;
    if(fabs(mz) < 0.06f) mz = 0;

    hand->vel_x = (hand->vel_x + mx * DT) * HAND_DAMPING;
    hand->vel_y = (hand->vel_y + my * DT) * HAND_DAMPING;
    hand->vel_z = (hand->vel_z + mz * DT) * HAND_DAMPING;

    hand->x = (hand->x + hand->vel_x) * (1.0f - HAND_SPRING);
    hand->y = (hand->y + hand->vel_y) * (1.0f - HAND_SPRING);
    hand->z = (hand->z + hand->vel_z) * (1.0f - HAND_SPRING);
}
*/
void update_hand_logic(HandState* hand, float* raw) {
    // 1. High-Pass Filter (Isolate movement, ignore static gravity)
    float ax = raw[0] - hand->last_ax;
    float ay = raw[1] - hand->last_ay;
    float az = raw[2] - hand->last_az;

    hand->last_ax = raw[0];
    hand->last_ay = raw[1];
    hand->last_az = raw[2];

    // 2. Deadzone (Ignore tiny vibrations)
    if(fabs(ax) < DEADZONE) ax = 0;
    if(fabs(ay) < DEADZONE) ay = 0;
    if(fabs(az) < DEADZONE) az = 0;

    // 3. Acceleration -> Velocity (With boosted sensitivity)
    hand->vel_x += (ax * HAND_SENSITIVITY) * DT;
    hand->vel_y += (ay * HAND_SENSITIVITY) * DT;
    hand->vel_z += (az * HAND_SENSITIVITY) * DT;

    // 4. Apply Air Resistance (Damping)
    hand->vel_x *= HAND_DAMPING;
    hand->vel_y *= HAND_DAMPING;
    hand->vel_z *= HAND_DAMPING;

    // 5. Velocity -> Position
    hand->x += hand->vel_x * DT;
    hand->y += hand->vel_y * DT;
    hand->z += hand->vel_z * DT;

    // 6. Return-to-Center Spring
    hand->x *= (1.0f - HAND_SPRING);
    hand->y *= (1.0f - HAND_SPRING);
    hand->z *= (1.0f - HAND_SPRING);

    // 7. Output Limiter (Definitive Bounds: -1.0 to 1.0)
    // This ensures your downstream code never breaks from crazy inputs
    if(hand->x > 1.0f) hand->x = 1.0f; if(hand->x < -1.0f) hand->x = -1.0f;
    if(hand->y > 1.0f) hand->y = 1.0f; if(hand->y < -1.0f) hand->y = -1.0f;
    if(hand->z > 1.0f) hand->z = 1.0f; if(hand->z < -1.0f) hand->z = -1.0f;
}

/*
void update_head_logic(HeadState* h, float* raw) {
    // 1. Calculate change in raw sensor data (Acceleration)
    float mx = raw[0] - h->last_ax;
    float mz = raw[1] - h->last_az;
    
    // Save current raw for next frame
    h->last_ax = raw[0];
    h->last_az = raw[1];

    // 2. Noise Filter (Deadzone)
    if(fabs(mx) < 0.06f) mx = 0;
    if(fabs(mz) < 0.06f) mz = 0;

    // 3. Update Velocity (Apply Damping)
    // HEAD_SENSITIVITY helps scale the raw input to a usable speed
    h->vel_x = (h->vel_x + (mx * HEAD_SENSITIVITY) * DT) * HAND_DAMPING;
    h->vel_z = (h->vel_z + (mz * HEAD_SENSITIVITY) * DT) * HAND_DAMPING;

    // 4. Update Position (Apply Spring/Smoothing)
    h->x = (h->x + h->vel_x) * (1.0f - HAND_SPRING);
    h->z = (h->z + h->vel_z) * (1.0f - HAND_SPRING);

    // 5. Hard Clamp (Optional, keeps the head from spinning 360)
    if(h->x > 1.0f)  h->x = 1.0f;  if(h->x < -1.0f) h->x = -1.0f;
    if(h->z > 1.0f)  h->z = 1.0f;  if(h->z < -1.0f) h->z = -1.0f;
}
*/
void update_head_logic(HeadState* h, float* raw) {
    // 1. High-Pass Filter (Lean is X, Dodge is Z)
    float lean_accel = raw[0] - h->last_ax;
    float dodge_accel = raw[1] - h->last_az; 
    
    h->last_ax = raw[0];
    h->last_az = raw[1]; // Note: using raw[1] (Y-axis of sensor) for Z-depth dodge

    // 2. Deadzone
    if(fabs(lean_accel) < DEADZONE) lean_accel = 0;
    if(fabs(dodge_accel) < DEADZONE) dodge_accel = 0;

    // 3. Acceleration -> Velocity
    h->vel_x += (lean_accel * HEAD_SENSITIVITY) * DT;
    h->vel_z += (dodge_accel * HEAD_SENSITIVITY) * DT;

    // 4. Apply Tighter Head Damping
    h->vel_x *= HEAD_DAMPING;
    h->vel_z *= HEAD_DAMPING;

    // 5. Velocity -> Position
    h->x += h->vel_x * DT;
    h->z += h->vel_z * DT;

    // 6. Tighter Return-to-Center Spring
    h->x *= (1.0f - HEAD_SPRING);
    h->z *= (1.0f - HEAD_SPRING);

    // 7. Output Limiter (-1.0 to 1.0)
    if(h->x > 1.0f) h->x = 1.0f; if(h->x < -1.0f) h->x = -1.0f;
    if(h->z > 1.0f) h->z = 1.0f; if(h->z < -1.0f) h->z = -1.0f;
}