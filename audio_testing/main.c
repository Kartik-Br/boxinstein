#include "board.h"
#include "processor_hal.h"
#include "nrf24l01plus.h"
#include "debug_log.h"
#include <stdint.h>
#include <string.h>

typedef struct {
    char msg[32];
} RadioPacket;

static RadioPacket packet_sent = {0};
static RadioPacket packet_received = {0};

void led_init(void);

int main(void) {

    HAL_Init();
    led_init();
    BRD_debuguart_init();
    debug_log("Program start\r\n");
    nrf24l01plus_init();
    debug_log("Radio init done\r\n");

    uint32_t last_send_time = 0;
    uint8_t waiting_for_reply = 0;

    while (1) {
        // SEND
        strcpy(packet_sent.msg, "HELLO");
        nrf24l01plus_send((uint8_t*)&packet_sent);
        debug_log("A sent\r\n");

        // WAIT UP TO 200ms FOR REPLY
        uint32_t start = HAL_GetTick();
        while ((HAL_GetTick() - start) < 200) {
            if (nrf24l01plus_recv((uint8_t*)&packet_received)) {
                packet_received.msg[31] = '\0';
                debug_log("A received: %s\r\n", packet_received.msg);
                break;
            }
        }

    }
}


void led_init(void) {

    __GPIOE_CLK_ENABLE();
    __GPIOG_CLK_ENABLE();

    // Configure PE6 as output
    GPIOE->MODER &= ~(0x03 << (6 * 2));
    GPIOE->MODER |=  (0x01 << (6 * 2));

    GPIOE->OSPEEDR &= ~(0x03 << (6 * 2));
    GPIOE->OSPEEDR |=  (0x02 << (6 * 2));

    GPIOE->OTYPER &= ~(0x01 << 6);

    // Configure PG15 as output
    GPIOG->MODER &= ~(0x03 << (15 * 2));
    GPIOG->MODER |=  (0x01 << (15 * 2));

    GPIOG->OSPEEDR &= ~(0x03 << (15 * 2));
    GPIOG->OSPEEDR |=  (0x02 << (15 * 2));

    GPIOG->OTYPER &= ~(0x01 << 15);
}

    // audio_init();
    // // First hit
    // audio_on();
    // audio_set_frequency(200);   // lowered from 600
    // HAL_Delay(40);
    // audio_off();
    // HAL_Delay(60);

    // // Second hit
    // audio_on();
    // audio_set_frequency(300);   // lowered from 800
    // HAL_Delay(40);
    // audio_off();