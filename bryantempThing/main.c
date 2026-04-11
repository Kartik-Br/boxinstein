/**
 **************************************************************
 * File: main_demo.c
 * Brief: Bouncing pSprite with alternating "towards-screen" fist punches.
 *
 * Punching model
 * --------------
 * Each arm has a fixed rect beside the body.  Each fist is a square
 * centred on the outer end of its arm.  When a fist punches it grows
 * symmetrically from PS_FIST_MIN to PS_FIST_MAX and back — like a
 * box coming toward the camera in VR.  Only one fist punches at a
 * time; they alternate left/right.
 *
 * Frame loop
 * ----------
 * 1. Erase old sprite (fists erased at their CURRENT size).
 * 2. Advance punch cycle; write new lfist_sz / rfist_sz into guy_ps.
 * 3. Move sprite, bounce off walls.
 * 4. render_update_psprite — recomputes all positions including fist
 *    top-left from the stored fist sizes.
 * 5. Draw updated sprite (fists drawn at new size).
 * 6. Move shadow rect.
 * 7. Refresh border.
 * 8. Frame delay.
 *
 * Wiring reminder
 * ---------------
 *   ILI9341 CS=PB6  RST=PA9  DC=PC7
 *   SPI1: SCK=PA5  MOSI=PA7  MISO=PA6
 *   Backlight → 3V3
 ***************************************************************
 */

#include "processor_hal.h"
#include "ili9488.h"
#include "stm32_adafruit_lcd.h"
#include "render.h"
#include "sprite.h"

/* ------------------------------------------------------------------ */
/*  Configuration                                                      */
/* ------------------------------------------------------------------ */

#define RECT_W   16
#define RECT_H   12

#define SPR_START_X   60
#define SPR_START_Y   80

#define SPR_VX_INIT    2
#define SPR_VY_INIT    1

#define FRAME_DELAY_MS  16

/* Punch cycle: frames for one full extend+retract stroke.
 * Must be even. */
#define PUNCH_CYCLE  16
#define PUNCH_HALF   (PUNCH_CYCLE / 2)

#define FIST_RANGE   (PS_FIST_MAX - PS_FIST_MIN)

/* Colours */
#define COL_BG      LCD_COLOR_BLACK
#define COL_BORDER  LCD_COLOR_WHITE
#define COL_RECT    LCD_COLOR_YELLOW
#define COL_SPRITE  LCD_COLOR_CYAN

/* ------------------------------------------------------------------ */
/*  Playfield clamp limits                                             */
/*                                                                     */
/*  The fist extends outward from the arm.  Max reach outward on each */
/*  side = PS_ARM_W + PS_FIST_MAX.                                    */
/* ------------------------------------------------------------------ */
#define FIST_REACH   (PS_ARM_W + PS_FIST_MAX)

#define SPR_X_MIN  (int16_t)(BORDER_X + 1 + FIST_REACH)
#define SPR_Y_MIN  (int16_t)(BORDER_Y + 1 + PS_NECK_H + PS_HEAD_H)
#define SPR_X_MAX  (int16_t)(BORDER_X + BORDER_W - PS_BODY_W - FIST_REACH - 1)
#define SPR_Y_MAX  (int16_t)(BORDER_Y + BORDER_H - PS_BODY_H - 1)

#define RECT_X_MIN  (int16_t)(BORDER_X + 1)
#define RECT_Y_MIN  (int16_t)(BORDER_Y + 1)
#define RECT_X_MAX  (int16_t)(BORDER_X + BORDER_W - RECT_W - 1)
#define RECT_Y_MAX  (int16_t)(BORDER_Y + BORDER_H - RECT_H - 1)

/* ------------------------------------------------------------------ */
/*  State                                                              */
/* ------------------------------------------------------------------ */
static Sprite   guy;
static pSprite  guy_ps;

static int16_t  spr_vx = SPR_VX_INIT;
static int16_t  spr_vy = SPR_VY_INIT;

static int16_t  rect_x;
static int16_t  rect_y;

/* punch_frame: 0 .. 2*PUNCH_CYCLE - 1
 * 0 .. PUNCH_CYCLE-1       : left fist punches
 * PUNCH_CYCLE .. 2*PC-1    : right fist punches  */
static uint8_t punch_frame = 0;

/* ------------------------------------------------------------------ */
/*  Forward declarations                                               */
/* ------------------------------------------------------------------ */
static void demo_init(void);
static void demo_loop(void);

/* ------------------------------------------------------------------ */
/*  Entry point                                                        */
/* ------------------------------------------------------------------ */
int main(void) {
    HAL_Init();
    demo_init();
    while (1) {
        demo_loop();
    }
}



/* ------------------------------------------------------------------ */
/*  demo_init                                                          */
/* ------------------------------------------------------------------ */
static void demo_init(void) {
    ili9488_Init();         // Wake up the ILI9488
    BSP_LCD_Clear(COL_BG);  // Adafruit command to fill the scree

    guy.x                = SPR_START_X;
    guy.y                = SPR_START_Y;
    guy.isExist          = 1;
    guy.spriteType       = S_GUY;
    guy.isAngled         = 0;
    guy.angle            = 0;
    guy.distanceToPlayer = 0;

    /* Zero-init: render_update_psprite detects zero and sets default sizes */
    guy_ps = (pSprite){0};
    render_update_psprite(&guy_ps, &guy, 0);
    render_draw_psprite(&guy_ps, COL_SPRITE);

    rect_x = (int16_t)guy.x - RECT_W - 4;
    rect_y = (int16_t)guy.y + PS_BODY_H - RECT_H;
    if (rect_x < RECT_X_MIN) rect_x = RECT_X_MIN;
    if (rect_y < RECT_Y_MIN) rect_y = RECT_Y_MIN;
    render_draw_rect_outline(rect_x, rect_y, RECT_W, RECT_H, COL_RECT);
}

/* ------------------------------------------------------------------ */
/*  demo_loop                                                          */
/* ------------------------------------------------------------------ */
static void demo_loop(void) {

    /* ---- 1. Erase old sprite (covers fists at current size) ------- */
    render_erase_psprite(&guy_ps, COL_BG);

    /* ---- 2. Advance punch cycle; update fist sizes in guy_ps ------ *
     *                                                                  *
     * We write directly into guy_ps.lfist_sz / rfist_sz here.        *
     * render_update_psprite (step 4) will read these to recompute    *
     * fist top-left positions, so everything stays in sync.          */
    punch_frame = (punch_frame + 1) % (2 * PUNCH_CYCLE);

    uint8_t left_jab = (punch_frame < PUNCH_CYCLE);
    uint8_t t        = left_jab ? punch_frame : (punch_frame - PUNCH_CYCLE);

    uint16_t active_sz;
    if (t < PUNCH_HALF) {
        /* Extending */
        active_sz = (uint16_t)(PS_FIST_MIN
                    + (uint32_t)t * FIST_RANGE / PUNCH_HALF);
    } else {
        /* Retracting */
        active_sz = (uint16_t)(PS_FIST_MAX
                    - (uint32_t)(t - PUNCH_HALF) * FIST_RANGE / PUNCH_HALF);
    }

    if (left_jab) {
        guy_ps.lfist_sz = active_sz;
        guy_ps.rfist_sz = PS_FIST_MIN;
    } else {
        guy_ps.rfist_sz = active_sz;
        guy_ps.lfist_sz = PS_FIST_MIN;
    }

    /* ---- 3. Move sprite, bounce off walls ------------------------- */
    int16_t new_sx = (int16_t)guy.x + spr_vx;
    int16_t new_sy = (int16_t)guy.y + spr_vy;

    if (new_sx < SPR_X_MIN) { new_sx = SPR_X_MIN; spr_vx = -spr_vx; }
    if (new_sx > SPR_X_MAX) { new_sx = SPR_X_MAX; spr_vx = -spr_vx; }
    if (new_sy < SPR_Y_MIN) { new_sy = SPR_Y_MIN; spr_vy = -spr_vy; }
    if (new_sy > SPR_Y_MAX) { new_sy = SPR_Y_MAX; spr_vy = -spr_vy; }

    guy.x = new_sx;
    guy.y = new_sy;

    /* ---- 4. Recompute all limb positions (fist pos from stored sz) */
    render_update_psprite(&guy_ps, &guy, 0);

    /* ---- 5. Draw updated sprite ----------------------------------- */
    render_draw_psprite(&guy_ps, COL_SPRITE);

    /* ---- 6. Move shadow rect -------------------------------------- */
    int16_t new_rx = (int16_t)guy.x - RECT_W - 4;
    int16_t new_ry = (int16_t)guy.y + PS_BODY_H - RECT_H;

    if (new_rx < RECT_X_MIN) new_rx = RECT_X_MIN;
    if (new_rx > RECT_X_MAX) new_rx = RECT_X_MAX;
    if (new_ry < RECT_Y_MIN) new_ry = RECT_Y_MIN;
    if (new_ry > RECT_Y_MAX) new_ry = RECT_Y_MAX;

    render_move_rect(rect_x, rect_y, new_rx, new_ry,
                     RECT_W, RECT_H, COL_RECT, COL_BG);
    rect_x = new_rx;
    rect_y = new_ry;

    /* ---- 7. Refresh border ---------------------------------------- */
    render_refresh_border(
        (int16_t)guy.x - FIST_REACH,
        (int16_t)guy.y - (PS_HEAD_H + PS_NECK_H),
        PS_BODY_W + 2 * FIST_REACH,
        PS_BODY_H + PS_HEAD_H + PS_NECK_H,
        COL_BORDER);
    render_refresh_border(rect_x, rect_y, RECT_W, RECT_H, COL_BORDER);

    /* ---- 8. Frame delay ------------------------------------------ */
    HAL_Delay(FRAME_DELAY_MS);
}