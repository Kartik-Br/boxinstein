#include "mma8452q.h"
#include "board.h"
#include "debug_log.h"
#include "processor_hal.h"
#include <stdint.h>

void hardware_init(void);

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

