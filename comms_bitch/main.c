#include "board.h"
#include "processor_hal.h"
#include "debug_log.h"
#include "nrf24l01plus.h"
#include "sprite.h"
#include <string.h>

// ========================================
// COMMENT OUT ONE OF THESE TO SWITCH ROLES
// ========================================
//#define BOARD_MASTER
#define BOARD_SLAVE
// ========================================

void hardware_init(void);

int main(void) {
    HAL_Init();
    hardware_init();

#ifdef BOARD_MASTER
    pSprite my_player = {0};
    pSprite enemy_player = {0};
    my_player.x_body = 10;
    my_player.y_body = 20;
    uint8_t tx_buffer[32] = {0};
    uint8_t rx_buffer[32] = {0};

    debug_log("Board 1 (Master) Booted...\r\n");

    while (1) {
        my_player.x_body++;
        memcpy(tx_buffer, &my_player, sizeof(pSprite));
        nrf24l01plus_send(tx_buffer);

        if (nrf24l01plus_recv(rx_buffer) == 1) {
            memcpy(&enemy_player, rx_buffer, sizeof(pSprite));
            debug_log("TX SUCCESS! Enemy is at X:%d, Y:%d\r\n", enemy_player.x_body, enemy_player.y_body);
            BRD_LEDGreenToggle();
        } else {
            debug_log("TX FAILED\r\n");
        }
        HAL_Delay(500);
    }

#elif defined(BOARD_SLAVE)
    pSprite enemy_player = {0};
    pSprite my_player = {0};
    my_player.x_body = 500;
    my_player.y_body = 600;
    uint8_t rx_buffer[32] = {0};
    uint8_t ack_buffer[32] = {0};

    debug_log("Board 2 (Slave) Booted...\r\n");

    nrf24l01plus_ce();
    memcpy(ack_buffer, &my_player, sizeof(pSprite));
    nrf24l01plus_write_ack_payload(0, ack_buffer, 32);

    while (1) {
        if (nrf24l01plus_recv(rx_buffer) == 1) {
            memcpy(&enemy_player, rx_buffer, sizeof(pSprite));
            debug_log("RX SUCCESS! Master is at X:%d, Y:%d\r\n", enemy_player.x_body, enemy_player.y_body);
            BRD_LEDGreenToggle();
            my_player.x_body--;
            memcpy(ack_buffer, &my_player, sizeof(pSprite));
            nrf24l01plus_write_ack_payload(0, ack_buffer, 32);
            nrf24l01plus_ce();
        }
    }

#else
    #error "You must define either BOARD_MASTER or BOARD_SLAVE at the top of main.c"
#endif
}

void hardware_init(void) {
    BRD_debuguart_init();
    BRD_LEDInit();
    BRD_LEDGreenOff();
    nrf24l01plus_init();
    HAL_Delay(2);
}