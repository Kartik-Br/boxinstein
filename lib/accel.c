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
 *
 */
#include "board.h"
#include "debug_log.h"
#include "processor_hal.h"

#define I2C_DEV_SDA_PIN 9
#define I2C_DEV_SCL_PIN 8
#define I2C_DEV_GPIO GPIOB
#define I2C_DEV_GPIO_AF GPIO_AF4_I2C1
#define I2C_DEV_GPIO_CLK() __GPIOB_CLK_ENABLE()

#define I2C_DEV I2C1
#define I2C_DEV_CLOCKSPEED 100000

/*
NOTE: [device]_ADDRESS is the physical chip I2C address
      [device]_WHO_AM_I_REG is the "who am i" register, used for identifying,
CANT BE WRITTEN
      MMA8452Q_CTRL_REG1 is the control register to set
operational mode, wake sleep behaviours etc
*/


#define MMA8452Q_ADDRESS (0x1D << 1) // MMA8452Q I2C address
#define MMA8452Q_WHO_AM_I_REG 0x0D // MMA8452Q "Who am I" register address
#define MMA8452Q_CTRL_REG1                                                     \
    0x2A // control register to toggle wake/sleep (write 0x01 to wake)
#define MMA8452Q_ACCEL_XOUT_H 0x05
#define MMA8452Q_ACCEL_XOUT_L 0x06


/*
#define MPU6050_ADDRESS                                                        \
    (0x68 << 1) // left shifted by 1 to accomodate for read/write bit
#define MPU6050_WHO_AM_I_REG 0x75 // Who am I register address
#define WHO_AM_I_RETURN 0x68      // return address of who am I
#define MPU6050_CTRL_REG1                                                      \
    0x6B // power management register, so we can awake it (write 0x00 to wake)
#define MPU6050_ACCEL_XOUT_H 0x3B // high bit for mpu6050 accelerometer
#define MPU6050_ACCEL_XOUT_L 0x3C // low bit
*/

void hardware_init(void);

uint32_t status;

uint8_t s4_mma8452q_reg_read(uint8_t reg_address) {
    uint8_t reg_value;

    SET_BIT(I2C_DEV->CR1, I2C_CR1_START); // start condition
    // Wait the START condition has been correctly sent
    while ((READ_REG(I2C_DEV->SR1) & I2C_SR1_SB) == 0)
        ;

    // Send Device Address (for reading
    WRITE_REG(I2C_DEV->DR, MMA8452Q_ADDRESS);
    // Wait for address to be acknowledged
    while ((READ_REG(I2C_DEV->SR1) & I2C_SR1_ADDR) == 0)
        ;
    // Clear ADDR Flag by reading SR1 and SR2.
    status = READ_REG(I2C_DEV->SR2);

    // Send register address
    WRITE_REG(I2C_DEV->DR, reg_address);
    while ((READ_REG(I2C_DEV->SR1) & I2C_SR1_TXE) == 0)
        ;

    // repeat start
    SET_BIT(I2C_DEV->CR1, I2C_CR1_START);
    // Wait to read
    while ((READ_REG(I2C_DEV->SR1) & I2C_SR1_SB) == 0)
        ;

    WRITE_REG(I2C_DEV->DR, MMA8452Q_ADDRESS | 0x01); // LSB is 1 for read mode
    while ((READ_REG(I2C_DEV->SR1) & I2C_SR1_ADDR) == 0)
        ;

    // NACK and STOP
    CLEAR_BIT(I2C_DEV->CR1, I2C_CR1_ACK);
    status = READ_REG(I2C_DEV->SR2); // CLEAR ADDR
    SET_BIT(I2C_DEV->CR1, I2C_CR1_STOP);
    // waiting for data to arrive
    while ((READ_REG(I2C_DEV->SR1) & I2C_SR1_RXNE) == 0)
        ;

    // Read received value
    reg_value = READ_REG(I2C_DEV->DR);

    return reg_value;
}

void s4_mma8452q_reg_write(uint8_t reg_address, uint8_t reg_value) {

    CLEAR_BIT(I2C_DEV->SR1, I2C_SR1_AF); // Clear Flags

    SET_BIT(I2C_DEV->CR1, I2C_CR1_START); // Generate the START condition
    // Wait the START condition has been correctly sent
    while ((READ_REG(I2C_DEV->SR1) & I2C_SR1_SB) == 0)
        ;

    // Send Peripheral Device Write address
    WRITE_REG(I2C_DEV->DR, I2C_7BIT_ADD_WRITE(MMA8452Q_ADDRESS));
    // Wait for address to be acknowledged
    while ((READ_REG(I2C_DEV->SR1) & I2C_SR1_ADDR) == 0)
        ;
    // Clear ADDR Flag by reading SR1 and SR2.
    status = READ_REG(I2C_DEV->SR2);

    // Send Read Register Address - WHO_AM_I Register Address
    WRITE_REG(I2C_DEV->DR, reg_address);
    while ((READ_REG(I2C_DEV->SR1) & I2C_SR1_TXE) == 0)
        ;

    // send data
    WRITE_REG(I2C_DEV->DR, reg_value);
    // Wait until register Address byte is transmitted
    while (((READ_REG(I2C_DEV->SR1) & I2C_SR1_TXE) == 0) &&
           ((READ_REG(I2C_DEV->SR1) & I2C_SR1_BTF) == 0))
        ;

    // finally stop
    SET_BIT(I2C_DEV->CR1, I2C_CR1_STOP);
}

/*
 * Main program
 */
int main(void) {

    uint8_t read_reg_val;

    HAL_Init();      // Initialise Board
    hardware_init(); // Initialise hardware peripherals

    s4_mma8452q_reg_write(MMA8452Q_CTRL_REG1, 0x01); // waking it up
    HAL_Delay(50);                                  // small delay

    // Cyclic Executive (CE) loop
    while (1) {


        // Task 1
        uint8_t task1_val = s4_mma8452q_reg_read(0x0D);
        if (task1_val == 0x2A){
            BRD_LEDGreenToggle();
        }

        // Task 3
        s4_mma8452q_reg_write(MMA8452Q_CTRL_REG1, 0x01);

        read_reg_val = s4_mma8452q_reg_read(MMA8452Q_CTRL_REG1);

        debug_log("Register Read: 0x%02X\n\r", read_reg_val);
        // Check WHO_AM_I Register value is 0x2A
        if (read_reg_val == 0x01) {
            BRD_LEDRedToggle();
        }

        //Task 4
        uint8_t xHigh = s4_mma8452q_reg_read(MMA8452Q_ACCEL_XOUT_H);
        uint8_t xLow = s4_mma8452q_reg_read(MMA8452Q_ACCEL_XOUT_L);

        // signed because MPU6050 data is twos comp
        uint16_t xComb = (uint16_t)((xHigh << 8) | xLow);

        debug_log("MMA8452Q X-AXIS: 0x%02X\n\r", xComb);
        
        HAL_Delay(1000); // Delay for 1s (1000ms)
    }
}

/*
 * Initialise Hardware
 */
void hardware_init(void) {

    uint32_t pclk1;
    uint32_t freqrange;

    BRD_LEDInit(); // Initialise LEDs
    BRD_debuguart_init();

    // Turn off LEDs
    BRD_LEDGreenOff();

    // Enable GPIO clock
    I2C_DEV_GPIO_CLK();

    //******************************************************
    // IMPORTANT NOTE: SCL Must be Initialised BEFORE SDA
    //******************************************************

    // Clear and Set Alternate Function for pin (upper ARF register)
    MODIFY_REG(I2C_DEV_GPIO->AFR[1],
               ((0x0F) << ((I2C_DEV_SCL_PIN - 8) * 4)) |
                   ((0x0F) << ((I2C_DEV_SDA_PIN - 8) * 4)),
               ((I2C_DEV_GPIO_AF << ((I2C_DEV_SCL_PIN - 8) * 4)) |
                (I2C_DEV_GPIO_AF << ((I2C_DEV_SDA_PIN - 8)) * 4)));

    // Clear and Set Alternate Function Push Pull Mode
    MODIFY_REG(
        I2C_DEV_GPIO->MODER,
        ((0x03 << (I2C_DEV_SCL_PIN * 2)) | (0x03 << (I2C_DEV_SDA_PIN * 2))),
        ((GPIO_MODE_AF_OD << (I2C_DEV_SCL_PIN * 2)) |
         (GPIO_MODE_AF_OD << (I2C_DEV_SDA_PIN * 2))));

    // Set low speed.
    SET_BIT(I2C_DEV_GPIO->OSPEEDR, (GPIO_SPEED_LOW << I2C_DEV_SCL_PIN) |
                                       (GPIO_SPEED_LOW << I2C_DEV_SDA_PIN));

    // Set Bit for Push/Pull output
    SET_BIT(I2C_DEV_GPIO->OTYPER,
            ((0x01 << I2C_DEV_SCL_PIN) | (0x01 << I2C_DEV_SDA_PIN)));

    // Clear and set bits for no push/pull
    MODIFY_REG(I2C_DEV_GPIO->PUPDR,
               (0x03 << (I2C_DEV_SCL_PIN * 2)) |
                   (0x03 << (I2C_DEV_SDA_PIN * 2)),
               (GPIO_PULLUP << (I2C_DEV_SCL_PIN * 2)) |
                   (GPIO_PULLUP << (I2C_DEV_SDA_PIN * 2)));

    // Configure the I2C peripheral
    // Enable I2C peripheral clock
    __I2C1_CLK_ENABLE();

    // Disable the selected I2C peripheral
    CLEAR_BIT(I2C_DEV->CR1, I2C_CR1_PE);

    pclk1 = HAL_RCC_GetPCLK1Freq();   // Get PCLK1 frequency
    freqrange = I2C_FREQRANGE(pclk1); // Calculate frequency range

    // I2Cx CR2 Configuration - Configure I2Cx: Frequency range
    MODIFY_REG(I2C_DEV->CR2, I2C_CR2_FREQ, freqrange);

    // I2Cx TRISE Configuration - Configure I2Cx: Rise Time
    MODIFY_REG(I2C_DEV->TRISE, I2C_TRISE_TRISE,
               I2C_RISE_TIME(freqrange, I2C_DEV_CLOCKSPEED));

    // I2Cx CCR Configuration - Configure I2Cx: Speed
    MODIFY_REG(I2C_DEV->CCR, (I2C_CCR_FS | I2C_CCR_DUTY | I2C_CCR_CCR),
               I2C_SPEED(pclk1, I2C_DEV_CLOCKSPEED, I2C_DUTYCYCLE_2));

    // I2Cx CR1 Configuration - Configure I2Cx: Generalcall and NoStretch mode
    MODIFY_REG(I2C_DEV->CR1, (I2C_CR1_ENGC | I2C_CR1_NOSTRETCH),
               (I2C_GENERALCALL_DISABLE | I2C_NOSTRETCH_DISABLE));

    // I2Cx OAR1 Configuration - Configure I2Cx: Own Address1 and addressing
    // mode
    MODIFY_REG(
        I2C_DEV->OAR1,
        (I2C_OAR1_ADDMODE | I2C_OAR1_ADD8_9 | I2C_OAR1_ADD1_7 | I2C_OAR1_ADD0),
        I2C_ADDRESSINGMODE_7BIT);

    // I2Cx OAR2 Configuration - Configure I2Cx: Dual mode and Own Address2
    MODIFY_REG(I2C_DEV->OAR2, (I2C_OAR2_ENDUAL | I2C_OAR2_ADD2),
               I2C_DUALADDRESS_DISABLE);

    // Enable the selected I2C peripheral
    SET_BIT(I2C_DEV->CR1, I2C_CR1_PE);
}
