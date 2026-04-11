/**
 **************************************************************
 * File: mylib/render.h
 * Brief: Outline-rect drawing, delta-optimised movement, sprite
 *        animation, and border management for the ILI9341 driver.
 *
 * Design principles
 * -----------------
 * - The background is never wiped.  Objects are erased by painting
 *   the background colour over only the pixels that were vacated.
 * - For a rect moving by (dx, dy) this means at most two thin
 *   axis-aligned strips (one vertical, one horizontal) are erased
 *   before the new outline is drawn — O(perimeter + |dx|*h + w*|dy|)
 *   pixels instead of O(w*h).
 * - A hard border is drawn once and refreshed only on the edges where
 *   a sprite could overdraw it.
 * - pSprite limb positions are derived from a walk-phase counter that
 *   advances proportionally to distance moved, using an integer LUT
 *   (no floating-point trig required).
 ***************************************************************
 * EXTERNAL FUNCTIONS
 ***************************************************************
 * render_draw_border(color)
 * render_draw_rect_outline(x,y,w,h,color)
 * render_move_rect(ox,oy,nx,ny,w,h,fg,bg)
 * render_resize_rect(x,y,ow,oh,nw,nh,fg,bg)
 * render_draw_line(x0,y0,x1,y1,color)
 * render_erase_line(x0,y0,x1,y1,bg)
 * render_move_sprite(s, dx, dy, fg, bg)
 * render_draw_psprite(ps, color)
 * render_erase_psprite(ps, bg)
 * render_update_psprite(ps, s, distance_moved)
 ***************************************************************
 */

#ifndef RENDER_H
#define RENDER_H

#include "ili9488.h"
#include "sprite.h"
#include <stdint.h>
#include <stdlib.h>  /* abs() */

/* ------------------------------------------------------------------ */
/*  Border — drawn one pixel inside the display edge                  */
/* ------------------------------------------------------------------ */
#define BORDER_X  1
#define BORDER_Y  1
#define BORDER_W  (ili9488_GetLcdPixelWidth()  - 2u)
#define BORDER_H  (ili9488_GetLcdPixelHeight() - 2u)

/* ------------------------------------------------------------------ */
/*  pSprite geometry (pixels, relative to sprite world position)      */
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
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

/**
 * Resize a rect outline in place (same top-left, new w/h).
 * Uses the same delta-erase strategy as render_move_rect:
 * only the pixels vacated by the size change are erased.
 *
 * @param x,y     Top-left position (unchanged)
 * @param ow,oh   Old width / height
 * @param nw,nh   New width / height
 * @param fg      Outline colour
 * @param bg      Background / erase colour
 */
void render_resize_rect(int16_t x,  int16_t y,
                         uint16_t ow, uint16_t oh,
                         uint16_t nw, uint16_t nh,
                         uint16_t fg, uint16_t bg);

/**
 * Draw a 1-pixel-wide line from (x0,y0) to (x1,y1) using
 * Bresenham's algorithm. Clips each pixel to display bounds.
 */
void render_draw_line(int16_t x0, int16_t y0,
                      int16_t x1, int16_t y1,
                      uint16_t color);

/**
 * Erase a previously drawn line by redrawing it in background colour.
 */
void render_erase_line(int16_t x0, int16_t y0,
                       int16_t x1, int16_t y1,
                       uint16_t bg);

/**
 * Draw the fixed border around the screen. Call once after init,
 * and call again if you suspect a sprite has overdrawn an edge.
 */
void render_draw_border(uint16_t color);

/**
 * Draw a hollow rectangle outline (1-pixel thick border).
 * Clips to the display; does nothing if fully out of bounds.
 */
void render_draw_rect_outline(int16_t x, int16_t y,
                               uint16_t w, uint16_t h,
                               uint16_t color);

/**
 * Move a rect outline from (ox,oy) to (nx,ny) with minimal redraws.
 *
 * Erases exactly the two trailing strips vacated by the movement,
 * then draws the new outline on top. The new outline is clipped to
 * the interior of the border so it cannot overdraw it.
 *
 * @param ox,oy   Old top-left position
 * @param nx,ny   New top-left position
 * @param w,h     Rect dimensions (unchanged)
 * @param fg      Outline colour
 * @param bg      Background / erase colour
 */
void render_move_rect(int16_t ox, int16_t oy,
                       int16_t nx, int16_t ny,
                       uint16_t w,  uint16_t h,
                       uint16_t fg, uint16_t bg);

/**
 * Move a Sprite by (dx, dy), erasing the old outline and drawing the
 * new one. Clamps to the playfield interior (inside the border).
 * Updates s->x and s->y in place.
 */
void render_move_sprite(Sprite *s, int16_t dx, int16_t dy,
                         uint16_t w, uint16_t h,
                         uint16_t fg, uint16_t bg);

/**
 * Recompute pSprite limb pixel positions from a Sprite world position
 * and a walk-phase driven by cumulative distance moved.
 * Call this before render_draw_psprite each frame.
 *
 * @param ps              pSprite to update (limb positions written here)
 * @param s               Parent Sprite (world x,y used as base)
 * @param distance_moved  Cumulative pixels moved this session (monotonic)
 */
void render_update_psprite(pSprite *ps, const Sprite *s,
                            uint32_t distance_moved);

/**
 * Draw a pSprite stick figure using outline primitives.
 * Call render_update_psprite first.
 */
void render_draw_psprite(const pSprite *ps, uint16_t color);

/**
 * Erase a pSprite by drawing background over each limb rect.
 * Call before render_update_psprite when the sprite moves.
 */
void render_erase_psprite(const pSprite *ps, uint16_t bg);

/**
 * Redraw the border segments that could have been overdrawn by a rect
 * at position (x,y,w,h). Cheap: only the four possible edge overlaps
 * are checked and redrawn.
 */
void render_refresh_border(int16_t x, int16_t y,
                            uint16_t w, uint16_t h,
                            uint16_t border_color);

#endif /* RENDER_H */