/**
 **************************************************************
 * File: mylib/render.h
 * Brief: Outline-rect drawing, delta-optimised movement, sprite
 *        animation, and border management.
 *
 * Display backend selection
 * -------------------------
 * Define USE_ILI9488 before including this header (or in your
 * Makefile with -DUSE_ILI9488) to target the ILI9488 via the
 * BSP LCD layer.  Without it, the ILI9341 direct driver is used.
 *
 *   ILI9341 (default):   #include "render.h"
 *   ILI9488 (BSP):       #define USE_ILI9488  then  #include "render.h"
 *   Makefile:            CFLAGS += -DUSE_ILI9488
 *
 * HandSprite field names
 * ----------------------
 * Both backends expect HandSprite to have fields:
 *   int   xScr, yScr;   -- screen-space position (pixels)
 *   int   size;         -- square side length (pixels)
 *   bool  isExist;
 * If your HandSprite uses x/y instead of xScr/yScr, add
 * -DHAND_USE_XY to your CFLAGS.
 *
 * pSprite field names
 * -------------------
 * sprite.h has a typo: y_boy should be y_body.
 * Both render backends already use y_body. Update sprite.h:
 *   int y_body;   (was y_boy)
 ***************************************************************
 */

#ifndef RENDER_H
#define RENDER_H

#include "sprite.h"
#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/*  Backend selection — pull in the right display header              */
/* ------------------------------------------------------------------ */
#ifdef USE_ILI9488
#  include "ili9488.h"
#  include "stm32_adafruit_lcd.h"
   /* Screen dimensions come from the BSP at runtime */
#  define RENDER_WIDTH()   ((int16_t)BSP_LCD_GetXSize())
#  define RENDER_HEIGHT()  ((int16_t)BSP_LCD_GetYSize())
#else
#  include "ili9341.h"
   /* Screen dimensions are compile-time constants */
#  define RENDER_WIDTH()   ((int16_t)ILI9341_WIDTH)
#  define RENDER_HEIGHT()  ((int16_t)ILI9341_HEIGHT)
#endif

/* ------------------------------------------------------------------ */
/*  Border — one pixel inside the display edge                        */
/* ------------------------------------------------------------------ */
#define BORDER_X  1
#define BORDER_Y  1
#define BORDER_W  ((uint16_t)(RENDER_WIDTH()  - 2))
#define BORDER_H  ((uint16_t)(RENDER_HEIGHT() - 2))

/* ------------------------------------------------------------------ */
/*  pSprite geometry                                                   */
/* ------------------------------------------------------------------ */
#define PS_HEAD_W   10
#define PS_HEAD_H   10
#define PS_NECK_W    4
#define PS_NECK_H    4
#define PS_BODY_W   14
#define PS_BODY_H   18

/* Arm geometry */
#define PS_ARM_W     3
#define PS_ARM_H    10
#define PS_ARM_Y_OFF 2

/* Fist geometry */
#define PS_FIST_MIN  4
#define PS_FIST_MAX 12

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

/** Draw the fixed border around the screen. */
void render_draw_border(uint16_t color);

/** Draw a hollow 1-pixel rectangle outline. */
void render_draw_rect_outline(int16_t x, int16_t y,
                               uint16_t w, uint16_t h,
                               uint16_t color);

/** Move a rect outline with minimal redraws (delta-erase). */
void render_move_rect(int16_t ox, int16_t oy,
                       int16_t nx, int16_t ny,
                       uint16_t w, uint16_t h,
                       uint16_t fg, uint16_t bg);

/** Resize a rect outline in place with minimal redraws. */
void render_resize_rect_outline(int16_t x, int16_t y,
                                 uint16_t old_w, uint16_t old_h,
                                 uint16_t new_w, uint16_t new_h,
                                 uint16_t fg, uint16_t bg);

/** Resize a filled rect in place with minimal redraws. */
void render_resize_rect(int16_t x,  int16_t y,
                         uint16_t ow, uint16_t oh,
                         uint16_t nw, uint16_t nh,
                         uint16_t fg, uint16_t bg);

/** Bresenham line draw / erase. */
void render_draw_line(int16_t x0, int16_t y0,
                       int16_t x1, int16_t y1, uint16_t color);
void render_erase_line(int16_t x0, int16_t y0,
                        int16_t x1, int16_t y1, uint16_t bg);

/** Move a Sprite, clamped to playfield. Updates s->x, s->y. */
void render_move_sprite(Sprite *s, int16_t dx, int16_t dy,
                         uint16_t w, uint16_t h,
                         uint16_t fg, uint16_t bg);

/**
 * Recompute pSprite limb positions.
 * head_offset_x/y shift the head relative to the neck
 * (drive these from your accelerometer readings).
 */
void render_update_psprite(pSprite *ps, const Sprite *s,
                            int16_t head_offset_x, int16_t head_offset_y);

void render_draw_psprite(const pSprite *ps, uint16_t color);
void render_erase_psprite(const pSprite *ps, uint16_t bg);

/**
 * Refresh any border segment that (x,y,w,h) overlaps.
 * Call after every move near the screen edge.
 */
void render_refresh_border(int16_t x, int16_t y,
                            uint16_t w, uint16_t h,
                            uint16_t border_color);

/** Hand sprite — square centred on (xScr, yScr) with side `size`. */
void render_draw_hand(const HandSprite *h, uint16_t color);
void render_erase_hand(const HandSprite *h, uint16_t bg);

#endif /* RENDER_H */
