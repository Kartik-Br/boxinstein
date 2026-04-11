/**
 **************************************************************
 * File: mylib/render.h
 * Brief: Outline-rect drawing, delta-optimised movement, sprite
 * animation, and border management for the ILI9488 driver.
 ***************************************************************
 */

#ifndef RENDER_H
#define RENDER_H

#include "ili9488.h"
#include "sprite.h"
#include <stdint.h>
#include <stdlib.h>  /* abs() */

/* ------------------------------------------------------------------ */
/* Border — drawn one pixel inside the display edge                  */
/* ------------------------------------------------------------------ */
#define BORDER_X  1
#define BORDER_Y  1
#define BORDER_W  (BSP_LCD_GetXSize() - 2u)
#define BORDER_H  (BSP_LCD_GetYSize() - 2u)

/* ------------------------------------------------------------------ */
/* pSprite geometry (pixels, relative to sprite world position)      */
/* ------------------------------------------------------------------ */
#define PS_HEAD_W       10
#define PS_HEAD_H       10
#define PS_NECK_W        4
#define PS_NECK_H        4
#define PS_BODY_W       14
#define PS_BODY_H       18

/* Arm geometry (fixed rects on each side of the body) */
#define PS_ARM_W         3
#define PS_ARM_H        10
#define PS_ARM_Y_OFF     2   /* pixels below body top where arms attach */

/* Fist geometry — a square centred on the outer end of each arm.
 * The fist grows symmetrically when punching ("towards the screen"). */
#define PS_FIST_MIN      4   /* resting size (pixels, square)          */
#define PS_FIST_MAX     12   /* fully-punched size (pixels, square)    */

/* Vertical offsets from sprite base y (top of body) upward           */
#define PS_BODY_Y_OFF   0           /* body top = sprite y            */
#define PS_NECK_Y_OFF   -(PS_NECK_H)             /* neck sits above body */
#define PS_HEAD_Y_OFF   -(PS_NECK_H + PS_HEAD_H) /* head sits above neck */

/* ------------------------------------------------------------------ */
/* Public API                                                        */
/* ------------------------------------------------------------------ */

void render_resize_rect_outline(int16_t x, int16_t y,
                                uint16_t old_w, uint16_t old_h,
                                uint16_t new_w, uint16_t new_h,
                                uint16_t fg, uint16_t bg);

void render_resize_rect(int16_t x,  int16_t y,
                         uint16_t ow, uint16_t oh,
                         uint16_t nw, uint16_t nh,
                         uint16_t fg, uint16_t bg);

void render_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

void render_erase_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t bg);

void render_draw_border(uint16_t color);

void render_draw_rect_outline(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t color);

void render_move_rect(int16_t ox, int16_t oy, int16_t nx, int16_t ny,
                       uint16_t w,  uint16_t h, uint16_t fg, uint16_t bg);

void render_move_sprite(Sprite *s, int16_t dx, int16_t dy,
                         uint16_t w, uint16_t h, uint16_t fg, uint16_t bg);

/* UPDATED: Now takes head offsets instead of distance_moved */
void render_update_psprite(pSprite *ps, const Sprite *s, int16_t head_offset_x, int16_t head_offset_y);

void render_draw_psprite(const pSprite *ps, uint16_t color);

void render_erase_psprite(const pSprite *ps, uint16_t bg);

void render_refresh_border(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t border_color);

/* NEW: Hand Sprite functionality */
void render_draw_hand(const HandSprite *h, uint16_t color);
void render_erase_hand(const HandSprite *h, uint16_t bg);

#endif /* RENDER_H */