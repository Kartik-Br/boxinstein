//#include "accel.c"
#include "board.h"
#include "processor_hal.h"
#include "debug_log.h"
#include "nrf24l01plus.h"
#include "ili9341.h"
#include "render.h"
#include "sprite.h"
#include "3dEngine.h"
#include "collisions.h"
#include <string.h>

// ========================================
// COMMENT OUT ONE OF THESE TO SWITCH ROLES
// ========================================
//#define BOARD_MASTER
#define BOARD_SLAVE
// ========================================

#define FRAME_DELAY_MS 67


static int16_t  rect_x = 40;
static int16_t  rect_y = 40;
static uint16_t rect_w = 20;
static uint16_t rect_h = 20;

static Sprite   guy;
static pSprite  guy_ps;
static HandSprite right_hand;

/* State for erasing the line frame-to-frame */
static int16_t last_line_x0, last_line_y0, last_line_x1, last_line_y1;
// Camera state
int16_t cam_x = 0; // Negative = player moved left, World moves right
int16_t cam_y = 0; // Positive = player ducked, World moves up
static void demo_init(void);
static void demo_loop(void);

uint8_t map[20][20] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

void hardware_init(void);

int main(void) {
    HAL_Init();
    hardware_init();
    demo_init();
    Player_info player = {.x = 2*64, .y = 4*64, .angle = -90};
    int numSprites = 4;
    HandSprite handSprites[4];
    HandSprite* sprites = handSprites;

    pSprite opSprite;
    opSprite.xPos = 5*64;
    opSprite.yPos = 2*64;
    opSprite.x_body = 0;
    opSprite.y_body = 0;
    opSprite.x_head = 0;
    opSprite.y_head = 0;
    opSprite.x_neck = 0;
    opSprite.y_neck = 0;
    opSprite.dLeft = false;
    opSprite.dRight = false;
    opSprite.dDuck = false;

    //Sprite* players;
    //Sprite* allSprites;
    sprites[0].x = player.x - 70;
    //sprites[0].x = opSprite.xPos - 25;
    sprites[0].y = player.y - 128;
    //sprites[0].y = opSprite.yPos - 5;
    sprites[0].z = 125;
    sprites[0].isExist = true;
    sprites[0].size = 50;
    sprites[0].blockCounter = 0;

    sprites[1].x = player.x - 300;
    sprites[1].y = player.y - 300;
    sprites[1].z = 32;
    sprites[1].isExist = true;
    sprites[1].size = 50;
    sprites[1].blockCounter = 0;


    sprites[2].x = opSprite.xPos + 100;
    //sprites[2].x = opSprite.xPos;
    sprites[2].y = opSprite.yPos - 128;
    //sprites[2].y = opSprite.yPos - 1;
    sprites[2].z = -50;
    sprites[2].isExist = true;
    sprites[2].size = 50;

    sprites[3].x = player.x + 600;
    sprites[3].y = player.y + 500;
    sprites[3].z = -80;
    sprites[3].isExist = true;
    sprites[3].size = 50;

    #ifdef BOARD_MASTER
    //pSprite my_player = {0};
    //pSprite enemy_player = {0};
    //my_player.x_body = 10;
    //my_player.y_body = 20;
    uint8_t tx_buffer[32] = {0};
    uint8_t rx_buffer[32] = {0};

    debug_log("Board 1 (Master) Booted...\r\n");

    #elif defined(BOARD_SLAVE)
    //pSprite enemy_player = {0};
    //pSprite my_player = {0};
    //my_player.x_body = 500;
    //my_player.y_body = 600;
    uint8_t rx_buffer[32] = {0};
    uint8_t ack_buffer[32] = {0};

    debug_log("Board 2 (Slave) Booted...\r\n");

    nrf24l01plus_ce();
    //memcpy(ack_buffer, &player, sizeof(pSprite));
    //nrf24l01plus_write_ack_payload(0, ack_buffer, 32);
    #else
    #error "You must define either BOARD_MASTER or BOARD_SLAVE at the top of main.c"
    #endif


    int rows = 320;
    int cols = 240;
    uint32_t timePassed = HAL_GetTick();
    while(1) {
        while (HAL_GetTick() - timePassed < FRAME_DELAY_MS);
        if (check_collide_block(&(sprites[0]), sprites)) {
            //break;
        }
        if (check_collide_block(&(sprites[1]), sprites)) {
            //break;
        }
        if (check_collide_head(&(sprites[0]), &opSprite)) {
            //break;
        }
        if (check_collide_head(&(sprites[1]), &opSprite)) {
            //break;
        }
        if (check_collide_body(&(sprites[0]), &opSprite)) {
            //break;
        }
        if (check_collide_body(&(sprites[1]), &opSprite)) {
            //break;
        }
        timePassed = HAL_GetTick();
        float FOV = 70.0f; // degrees
        float FOV_RAD = FOV * (M_PI / 180.0f);
        player.angle += 5;
        //opSprite.xPos += 5;
        //sprites[0].y -= 5;

        for (int i = 0; i < numSprites; i++) {
	    sprites[i].distanceToPlayer = dist(player.x, player.y, sprites[i].x, sprites[i].y, 0);
        }
        //draw_all_stuff(map, &player, cols, rows, &sprites, numSprites);
        draw_all_stuff(map, &player, cols, rows, sprites, numSprites, &opSprite);
        //HAL_Delay(FRAME_DELAY_MS);
        //break;
        #ifdef BOARD_MASTER
        //my_player.x_body++;
        int8_t buf8;
        int16_t buf16;
        buf16 = player.x;
        memcpy(tx_buffer, &buf16, 2);
        buf16 = player.y;
        memcpy(tx_buffer + 2, &buf16, 2);
        buf16 = sprites[0].x;
        memcpy(tx_buffer + 4, &buf16, 2);
        buf16 = sprites[0].y;
        memcpy(tx_buffer + 6, &buf16, 2);
        buf16 = sprites[0].z;
        memcpy(tx_buffer + 8, &buf16, 2);
        buf8 = sprites[0].isHit;
        memcpy(tx_buffer + 10, &buf8, 1);
        buf16 = sprites[1].x;
        memcpy(tx_buffer + 11, &buf16, 2);
        buf16 = sprites[1].y;
        memcpy(tx_buffer + 13, &buf16, 2);
        buf16 = sprites[1].z;
        memcpy(tx_buffer + 15, &buf16, 2);
        buf8 = sprites[1].isHit;
        memcpy(tx_buffer + 17, &buf8, 1);
        nrf24l01plus_send(tx_buffer);

        if (nrf24l01plus_recv(rx_buffer) == 1) {
            memcpy(&buf16, rx_buffer, 2);
            opSprite.xPos = buf16;
            memcpy(&buf16, rx_buffer + 2, 2);
            opSprite.yPos = buf16;
            memcpy(&buf16, rx_buffer + 4, 2);
            sprites[2].x = buf16;
            memcpy(&buf16, rx_buffer + 6, 2);
            sprites[2].y = buf16;
            memcpy(&buf16, rx_buffer + 8, 2);
            sprites[2].z = buf16;
            memcpy(&buf16, rx_buffer + 10, 1);
            sprites[2].isHit = buf8;
            memcpy(&buf16, rx_buffer + 11, 2);
            sprites[3].x = buf16;
            memcpy(&buf16, rx_buffer + 13, 2);
            sprites[3].y = buf16;
            memcpy(&buf16, rx_buffer + 15, 2);
            sprites[3].z = buf16;
            memcpy(&buf16, rx_buffer + 17, 1);
            sprites[3].isHit = buf8;
            debug_log("TX SUCCESS! Enemy is at X:%d, Y:%d\r\n", opSprite.xPos, opSprite.yPos);
            BRD_LEDGreenToggle();
        } else {
            debug_log("TX FAILED\r\n");
        }

        #elif defined(BOARD_SLAVE)
        int8_t buf8;
        int16_t buf16;
        if (nrf24l01plus_recv(rx_buffer) == 1) {
            //memcpy(&opSprite, rx_buffer, 4);
            //memcpy(&(sprites[2]), rx_buffer, 7);
            //memcpy(&(sprites[3]), rx_buffer, 7);
            memcpy(&buf16, rx_buffer, 2);
            opSprite.xPos = buf16;
            memcpy(&buf16, rx_buffer + 2, 2);
            opSprite.yPos = buf16;
            memcpy(&buf16, rx_buffer + 4, 2);
            sprites[2].x = buf16;
            memcpy(&buf16, rx_buffer + 6, 2);
            sprites[2].y = buf16;
            memcpy(&buf16, rx_buffer + 8, 2);
            sprites[2].z = buf16;
            memcpy(&buf16, rx_buffer + 10, 1);
            sprites[2].isHit = buf8;
            memcpy(&buf16, rx_buffer + 11, 2);
            sprites[3].x = buf16;
            memcpy(&buf16, rx_buffer + 13, 2);
            sprites[3].y = buf16;
            memcpy(&buf16, rx_buffer + 15, 2);
            sprites[3].z = buf16;
            memcpy(&buf16, rx_buffer + 17, 1);
            sprites[3].isHit = buf8;

            debug_log("RX SUCCESS! Enemy is at X:%d, Y:%d\r\n", opSprite.xPos, opSprite.yPos);
            BRD_LEDGreenToggle();
            //my_player.x_body--;
            //memcpy(ack_buffer, &player, 4);
            //memcpy(ack_buffer + 4, &(sprites[0]), 7);
            //memcpy(ack_buffer + 11, &(sprites[1]), 7);
            buf16 = player.x;
            memcpy(ack_buffer, &buf16, 2);
            buf16 = player.y;
            memcpy(ack_buffer + 2, &buf16, 2);
            buf16 = sprites[0].x;
            memcpy(ack_buffer + 4, &buf16, 2);
            buf16 = sprites[0].y;
            memcpy(ack_buffer + 6, &buf16, 2);
            buf16 = sprites[0].z;
            memcpy(ack_buffer + 8, &buf16, 2);
            buf8 = sprites[0].isHit;
            memcpy(ack_buffer + 10, &buf8, 1);
            buf16 = sprites[1].x;
            memcpy(ack_buffer + 11, &buf16, 2);
            buf16 = sprites[1].y;
            memcpy(ack_buffer + 13, &buf16, 2);
            buf16 = sprites[1].z;
            memcpy(ack_buffer + 15, &buf16, 2);
            buf8 = sprites[1].isHit;
            memcpy(ack_buffer + 17, &buf8, 1);

            nrf24l01plus_write_ack_payload(0, ack_buffer, 32);
            nrf24l01plus_ce();
        }
        #endif
    }
    return 0;
}

void hardware_init(void) {
    BRD_debuguart_init();
    BRD_LEDInit();
    BRD_LEDGreenOff();
    nrf24l01plus_init();
    HAL_Delay(2);
}

static void demo_init(void) {
    ili9341_init();
    ili9341_fill_screen(COL_BG);
    render_draw_border(COL_BORDER);

    ///* Setup player */
    //guy.x = 100;
    //guy.y = 120;
    //guy.isExist = 1;
    //guy.spriteType = S_GUY;
    //render_update_psprite(&guy_ps, &guy, 0, 0);
    //render_draw_psprite(&guy_ps, COL_SPRITE);

    ///* Setup Hand */
    //right_hand.isExist = true;
    //right_hand.x = 140;
    //right_hand.y = 100;
    //right_hand.size = 6;

    //render_draw_rect_outline(rect_x, rect_y, rect_w, rect_h, COL_RECT);
}
static void demo_loop(void) {
    static int frame = 0;
    frame++;

    /* --- 1. Delta-Erased Resizing Rect --- */
    /* Pulse size between 20 and 40 */
    uint16_t target_w = 20 + (frame % 20);
    uint16_t target_h = 20 + ((frame/2) % 20);

    render_resize_rect_outline(rect_x, rect_y, rect_w, rect_h, target_w, target_h, COL_RECT, COL_BG);
    rect_w = target_w;
    rect_h = target_h;

    /* --- 2. Head freely orbiting neck --- */
    render_erase_psprite(&guy_ps, COL_BG);

    int16_t head_offset_x = (frame % 10 < 5) ? 2 : -2;
    int16_t head_offset_y = (frame % 8 < 4) ? -1 : 1;

    render_update_psprite(&guy_ps, &guy, head_offset_x, head_offset_y);
    render_draw_psprite(&guy_ps, COL_SPRITE);

    /* --- 3. Hand Sprite (Z-depth mapping) --- */
    render_erase_hand(&right_hand, COL_BG);
    right_hand.size = 5 + (frame % 15); // Pulsing depth to simulate Z movement
    render_draw_hand(&right_hand, ILI9341_YELLOW);

    /* --- 4. Draw Line --- */
    /* Erase old line using background color */
    render_draw_line(last_line_x0, last_line_y0, last_line_x1, last_line_y1, COL_BG);

    /* Draw new line from the player's freely moving head to the hand */
    render_draw_line(guy_ps.x_head, guy_ps.y_head, right_hand.x, right_hand.y, COL_LINE);

    /* Save coords to erase on next frame */
    last_line_x0 = guy_ps.x_head;
    last_line_y0 = guy_ps.y_head;
    last_line_x1 = right_hand.x;
    last_line_y1 = right_hand.y;

    HAL_Delay(FRAME_DELAY_MS);
}

