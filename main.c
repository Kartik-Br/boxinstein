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

    debug_log("System Booted...\r\n");

    mma8452q_init(I2C1, MMA_ADDR_GND);
    debug_log("Left Hand Init OK\r\n");

    mma8452q_init(I2C1, MMA_ADDR_VCC);
    debug_log("Right Hand Init OK\r\n");
    
    mma8452q_init(I2C2, MMA_ADDR_GND);
    debug_log("Head Init OK\r\n");

    uint8_t id = mma8452q_reg_read(I2C2, MMA_ADDR_GND, 0x0D);
    debug_log("Head Sensor ID: 0x%02X (Expected: 0x2A)\r\n", id);

    uint8_t sysmod = mma8452q_reg_read(I2C2, MMA_ADDR_GND, 0x0B);
    debug_log("Head SYSMOD: 0x%02X (0x01=Standby, 0x02=Active)\r\n", sysmod);
    // Initialize hardware (UART/I2C)

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
        //debug_log("Head_Dodge:%f, Head_Lean:%f, RHand_X:%f, RHand_Y:%f, RHand_Z:%f\r\n, LHand_X:%f, LHand_Y:%f, LHand_Z:%f", 
          //      head.z, head.x, rightHand.x, rightHand.y, rightHand.z, leftHand.x, leftHand.y, leftHand.z);
        // Example Plotter Output: Tracking Speed and Position on the X-Axis
        debug_log("Head_Spd_X:%f, Head_Pos_X:%f, RHand_Spd_X:%f, RHand_Pos_X:%f, LHand_Spd_X:%f, LHand_Pos_X:%f\r\n", 
                head.vel_x, head.x, rightHand.vel_x, rightHand.x, leftHand.vel_x, leftHand.x);
                
        HAL_Delay(30); // ~60 FPS
    }
}

void hardware_init(void) {
    uint32_t pclk1;
    uint32_t freqrange;

    // 1. Initialise Standard Board Features
    BRD_LEDInit(); 
    BRD_debuguart_init();
    BRD_LEDGreenOff();

    // 2. Enable Peripheral Clocks
    __HAL_RCC_GPIOB_CLK_ENABLE();  // Pins for both I2C1 and I2C2 are on Port B
    __HAL_RCC_I2C1_CLK_ENABLE();   // Clock for I2C1
    __HAL_RCC_I2C2_CLK_ENABLE();   // Clock for I2C2

    // ---------------------------------------------------------
    // 3. GPIO CONFIGURATION (PB8, PB9 for I2C1 | PB10, PB11 for I2C2)
    // ---------------------------------------------------------

    // Set Alternate Function AF4 for pins PB8, PB9, PB10, PB11
    // AFR[1] handles pins 8 through 15
    MODIFY_REG(GPIOB->AFR[1], 
        (0xF << ((8-8)*4)) | (0xF << ((9-8)*4)) | (0xF << ((10-8)*4)) | (0xF << ((11-8)*4)),
        (4 << ((8-8)*4))   | (4 << ((9-8)*4))   | (4 << ((10-8)*4))  | (4 << ((11-8)*4))
    );

    // Set MODER to Alternate Function for all 4 pins
    MODIFY_REG(GPIOB->MODER,
        (0x3 << (8*2)) | (0x3 << (9*2)) | (0x3 << (10*2)) | (0x3 << (11*2)),
        (0x2 << (8*2)) | (0x2 << (9*2)) | (0x2 << (10*2)) | (0x2 << (11*2))
    );

    // Set Output Type to OPEN DRAIN (Required for I2C)
    SET_BIT(GPIOB->OTYPER, (1 << 8) | (1 << 9) | (1 << 10) | (1 << 11));

    // Set Pull-up resistors (Internal pull-ups)
    MODIFY_REG(GPIOB->PUPDR,
        (0x3 << (8*2)) | (0x3 << (9*2)) | (0x3 << (10*2)) | (0x3 << (11*2)),
        (0x1 << (8*2)) | (0x1 << (9*2)) | (0x1 << (10*2)) | (0x1 << (11*2))
    );

    // ---------------------------------------------------------
    // 4. I2C PERIPHERAL CONFIGURATION (Common Logic)
    // ---------------------------------------------------------
    pclk1 = HAL_RCC_GetPCLK1Freq();
    freqrange = I2C_FREQRANGE(pclk1);

    // --- Configure I2C1 ---
    CLEAR_BIT(I2C1->CR1, I2C_CR1_PE); // Disable to config
    MODIFY_REG(I2C1->CR2, I2C_CR2_FREQ, freqrange);
    MODIFY_REG(I2C1->TRISE, I2C_TRISE_TRISE, I2C_RISE_TIME(freqrange, 100000));
    MODIFY_REG(I2C1->CCR, (I2C_CCR_FS | I2C_CCR_DUTY | I2C_CCR_CCR), I2C_SPEED(pclk1, 100000, I2C_DUTYCYCLE_2));
    SET_BIT(I2C1->CR1, I2C_CR1_PE);   // Enable

    // --- Configure I2C2 ---
    CLEAR_BIT(I2C2->CR1, I2C_CR1_PE); // Disable to config
    MODIFY_REG(I2C2->CR2, I2C_CR2_FREQ, freqrange);
    MODIFY_REG(I2C2->TRISE, I2C_TRISE_TRISE, I2C_RISE_TIME(freqrange, 100000));
    MODIFY_REG(I2C2->CCR, (I2C_CCR_FS | I2C_CCR_DUTY | I2C_CCR_CCR), I2C_SPEED(pclk1, 100000, I2C_DUTYCYCLE_2));
    SET_BIT(I2C2->CR1, I2C_CR1_PE);   // Enable
}
