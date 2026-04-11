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
 * render_move_sprite(s, dx, dy, fg, bg)
 * render_draw_psprite(ps, color)
 * render_erase_psprite(ps, bg)
 * render_update_psprite(ps, s, distance_moved)
 ***************************************************************
 */

#ifndef RENDER_H
#define RENDER_H

#include "ili9341.h"
#include "sprite.h"
#include <stdint.h>
#include <stdlib.h>  /* abs() */

/* ------------------------------------------------------------------ */
/*  Border — drawn one pixel inside the display edge                  */
/* ------------------------------------------------------------------ */
#define BORDER_X  1
#define BORDER_Y  1
#define BORDER_W  (ILI9341_WIDTH  - 2u)
#define BORDER_H  (ILI9341_HEIGHT - 2u)

/* ------------------------------------------------------------------ */
/*  pSprite geometry (pixels, relative to sprite world position)      */
/* ------------------------------------------------------------------ */
#define PS_HEAD_W       10
#define PS_HEAD_H       10
#define PS_NECK_W        4
#define PS_NECK_H        4
#define PS_BODY_W       14
#define PS_BODY_H       18

/* Vertical offsets from sprite base y (feet) upward                 */
#define PS_BODY_Y_OFF   0           /* body bottom = sprite y         */
#define PS_NECK_Y_OFF   (PS_BODY_H)
#define PS_HEAD_Y_OFF   (PS_BODY_H + PS_NECK_H)

/* Head bob amplitude and LUT size */
#define PS_BOB_STEPS    8
#define PS_BOB_AMP      2           /* pixels                         */

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

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
