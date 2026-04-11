/**
 **************************************************************
 * File: main_demo.c
 * Brief: Demonstration of delta-optimised rendering on the
 *        ILI9341 TFT display (STM32 NUCLEO-F429ZI).
 *
 * What this demo shows
 * --------------------
 * 1. A filled background is drawn once at startup (black).
 * 2. A hard white border is drawn once and maintained throughout.
 * 3. A yellow rectangle moves around the screen with velocity,
 *    bouncing off the border walls.
 *    - Only the two thin trailing strips are erased each frame,
 *      not the whole screen.
 * 4. A stick-figure sprite (pSprite) walks alongside the rect.
 *    - Its head bobs according to how far it has walked.
 *    - It is erased and redrawn using only the limb bounding boxes.
 * 5. If either object's outline touches the border, the border is
 *    refreshed immediately so it always remains intact.
 *
 * Frame loop
 * ----------
 *   while(1) {
 *       update positions
 *       move rect  (delta-erase + redraw outline)
 *       move sprite (erase old limbs, update positions, redraw)
 *       refresh border if needed
 *       delay
 *   }
 *
 * Wiring reminder
 * ---------------
 *   ILI9341 CS=PB6  RST=PA9  DC=PC7
 *   SPI1: SCK=PA5  MOSI=PA7  MISO=PA6
 *   Backlight → 3V3
 ***************************************************************
 */

#include "processor_hal.h"
#include "ili9341.h"
#include "render.h"
#include "sprite.h"

/* ------------------------------------------------------------------ */
/*  Demo configuration                                                 */
/* ------------------------------------------------------------------ */

/* Moving rectangle dimensions */
#define RECT_W   30
#define RECT_H   20

/* Starting position (inside the border) */
#define RECT_START_X  40
#define RECT_START_Y  60

/* Initial velocity in pixels per frame */
#define INIT_VX   2
#define INIT_VY   1

/* Milliseconds between frames */
#define FRAME_DELAY_MS  16   /* ~60 fps target; actual speed is DMA-limited */

/* Colours */
#define COL_BG      ILI9341_BLACK
#define COL_BORDER  ILI9341_WHITE
#define COL_RECT    ILI9341_YELLOW
#define COL_SPRITE  ILI9341_CYAN

/* ------------------------------------------------------------------ */
/*  Playfield limits (inside the border, accounting for rect size)    */
/* ------------------------------------------------------------------ */
#define PLAY_X_MIN  (BORDER_X + 1)
#define PLAY_Y_MIN  (BORDER_Y + 1)
#define PLAY_X_MAX  (int16_t)(BORDER_X + BORDER_W - RECT_W  - 1)
#define PLAY_Y_MAX  (int16_t)(BORDER_Y + BORDER_H - RECT_H  - 1)

#define SPR_X_MAX   (int16_t)(BORDER_X + BORDER_W - PS_BODY_W - 1)
#define SPR_Y_MAX   (int16_t)(BORDER_Y + BORDER_H - PS_BODY_H - PS_HEAD_H - PS_NECK_H - PS_BOB_AMP - 2)

/* ------------------------------------------------------------------ */
/*  State                                                              */
/* ------------------------------------------------------------------ */
static int16_t  rect_x  = RECT_START_X;
static int16_t  rect_y  = RECT_START_Y;
static int16_t  vel_x   = INIT_VX;
static int16_t  vel_y   = INIT_VY;

static Sprite   guy;
static pSprite  guy_ps;
static uint32_t guy_dist = 0;   /* cumulative distance moved (pixels) */

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

    /* Configure system clock to 180 MHz (typical sourcelib setup)    */
    /* Replace with your project's SystemClock_Config() if different  */
    SystemClock_Config();

    demo_init();

    while (1) {
        demo_loop();
    }
}

/* ------------------------------------------------------------------ */
/*  demo_init                                                          */
/* ------------------------------------------------------------------ */

static void demo_init(void) {

    ili9341_init();

    /* --- Draw background once --- */
    ili9341_fill_screen(COL_BG);

    /* --- Draw border once --- */
    render_draw_border(COL_BORDER);

    /* --- Draw initial rect outline --- */
    render_draw_rect_outline(rect_x, rect_y, RECT_W, RECT_H, COL_RECT);

    /* --- Initialise the walking sprite --- */
    guy.x           = rect_x + RECT_W + 4;   /* just to the right of the rect */
    guy.y           = rect_y + 2;
    guy.isExist     = 1;
    guy.spriteType  = S_GUY;
    guy.isAngled    = 0;
    guy.angle       = 0;
    guy.distanceToPlayer = 0;

    render_update_psprite(&guy_ps, &guy, guy_dist);
    render_draw_psprite(&guy_ps, COL_SPRITE);
}

/* ------------------------------------------------------------------ */
/*  demo_loop — called every frame                                     */
/* ------------------------------------------------------------------ */

static void demo_loop(void) {

    /* ---- 1. Compute new rect position ---- */
    int16_t new_rx = rect_x + vel_x;
    int16_t new_ry = rect_y + vel_y;

    /* Bounce off border walls */
    if (new_rx < PLAY_X_MIN) { new_rx = PLAY_X_MIN; vel_x = -vel_x; }
    if (new_rx > PLAY_X_MAX) { new_rx = PLAY_X_MAX; vel_x = -vel_x; }
    if (new_ry < PLAY_Y_MIN) { new_ry = PLAY_Y_MIN; vel_y = -vel_y; }
    if (new_ry > PLAY_Y_MAX) { new_ry = PLAY_Y_MAX; vel_y = -vel_y; }

    /* ---- 2. Move rect (delta-erase + redraw) ---- */
    render_move_rect(rect_x, rect_y,
                     new_rx, new_ry,
                     RECT_W, RECT_H,
                     COL_RECT, COL_BG);

    rect_x = new_rx;
    rect_y = new_ry;

    /* ---- 3. Move the sprite to follow the rect ---- */
    int16_t target_sx = rect_x + RECT_W + 4;
    int16_t target_sy = rect_y + 2;

    /* Clamp sprite to playfield */
    if (target_sx > SPR_X_MAX) target_sx = SPR_X_MAX;
    if (target_sy > SPR_Y_MAX) target_sy = SPR_Y_MAX;
    if (target_sx < PLAY_X_MIN) target_sx = PLAY_X_MIN;
    if (target_sy < PLAY_Y_MIN) target_sy = PLAY_Y_MIN;

    int16_t sdx = target_sx - (int16_t)guy.x;
    int16_t sdy = target_sy - (int16_t)guy.y;

    if (sdx != 0 || sdy != 0) {

        /* Erase old limbs */
        render_erase_psprite(&guy_ps, COL_BG);

        /* Accumulate walked distance */
        uint16_t step = (uint16_t)(abs16(sdx) + abs16(sdy));
        guy_dist += step;
        guy.distanceToPlayer += step;

        /* Update sprite world position */
        guy.x = target_sx;
        guy.y = target_sy;

        /* Recompute limb positions with new walk phase */
        render_update_psprite(&guy_ps, &guy, guy_dist);

        /* Draw updated sprite */
        render_draw_psprite(&guy_ps, COL_SPRITE);
    }

    /* ---- 4. Refresh border where either object might have touched it ---- */
    render_refresh_border(rect_x, rect_y, RECT_W, RECT_H, COL_BORDER);
    render_refresh_border((int16_t)guy.x,
                           (int16_t)guy.y - (PS_HEAD_H + PS_NECK_H + PS_BOB_AMP),
                           PS_BODY_W,
                           PS_BODY_H + PS_HEAD_H + PS_NECK_H + PS_BOB_AMP,
                           COL_BORDER);

    /* ---- 5. Frame delay ---- */
    HAL_Delay(FRAME_DELAY_MS);
}
