#include "board.h"
#include "debug_log.h"
#include "mma8452q.h"
#include "processor_hal.h"
#include <stdint.h>

void hardware_init(void);

#include <math.h>
#include <stdint.h>
#include "mma8452q.h"
#include "board.h"
#include "debug_log.h"
#include <stdio.h>

// Global State Objects
HandState leftHand = {0};
HandState rightHand = {0};
HeadState head = {0};
float raw_data[3];

int main(void) {
    HAL_Init(); 
    hardware_init();
    // Initialize hardware (UART/I2C)
    
    // Wake up sensors
// 2. Initialize the 3 sensors (Wake them up)
    // Hands are on I2C Bus 1
    mma8452q_init(I2C1, MMA_ADDR_GND); // Left Hand (SA0 to GND)
    mma8452q_init(I2C1, MMA_ADDR_VCC); // Right Hand (SA0 to 3.3V)
    
    // Head is on I2C Bus 2
    mma8452q_init(I2C2, MMA_ADDR_GND); // Head sensor (SA0 to GND)

    while (1) {
        mma8452q_get_accel(I2C2, MMA_ADDR_VCC, raw_data);
        update_head_logic(&head, raw_data);

        // --- 4. PROCESS LEFT HAND (I2C1) ---
        mma8452q_get_accel(I2C1, MMA_ADDR_GND, raw_data);
        update_hand_logic(&leftHand, raw_data);

        // --- 5. PROCESS RIGHT HAND (I2C1) ---
        mma8452q_get_accel(I2C1, MMA_ADDR_VCC, raw_data);
        update_hand_logic(&rightHand, raw_data);

        // 3. Test Plotter Output
        // Format for Arduino Serial Plotter
        printf("Head_Dodge:%f, Head_Lean:%f, RHand_X:%f, RHand_Y:%f, RHand_Z:%f\r\n", LHand_X:%f, LHand_Y:%f, LHand_Z:%f 
                head.z, head.x, rightHand.x, rightHand.y, rightHand.z, leftHand.x, leftHand.y, leftHand.z);

        HAL_Delay(16); // ~60 FPS
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
