#ifndef RENDER_H
#define RENDER_H

#include "ili9341.h"
#include "sprite.h"
#include <stdint.h>
#include <stdlib.h>  /* abs() */

#define BORDER_X  1
#define BORDER_Y  1
#define BORDER_W  (ILI9341_WIDTH  - 2u)
#define BORDER_H  (ILI9341_HEIGHT - 2u)

#define PS_HEAD_W       10
#define PS_HEAD_H       10
#define PS_NECK_W        4
#define PS_NECK_H        4
#define PS_BODY_W       14
#define PS_BODY_H       18

void render_draw_border(uint16_t color);
void render_draw_rect_outline(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t color);
void render_move_rect(int16_t ox, int16_t oy, int16_t nx, int16_t ny, uint16_t w, uint16_t h, uint16_t fg, uint16_t bg);

/** * Resize a rect anchored at (x,y) with minimal redraws.
 * Erases only the old outlines that don't overlap with the new ones.
 */
void render_resize_rect_outline(int16_t x, int16_t y, uint16_t old_w, uint16_t old_h, uint16_t new_w, uint16_t new_h, uint16_t fg, uint16_t bg);

/** Draw a standard Bresenham line between two points */
void render_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

void render_move_sprite(Sprite *s, int16_t dx, int16_t dy, uint16_t w, uint16_t h, uint16_t fg, uint16_t bg);

/** * Update pSprite limb positions.
 * Head is tethered to the neck but shifted by head_offset_x and head_offset_y.
 */
void render_update_psprite(pSprite *ps, const Sprite *s, int16_t head_offset_x, int16_t head_offset_y);
void render_draw_psprite(const pSprite *ps, uint16_t color);
void render_erase_psprite(const pSprite *ps, uint16_t bg);

/** Hand Rendering API */
void render_draw_hand(const HandSprite *hand, uint16_t color);
void render_erase_hand(const HandSprite *hand, uint16_t bg);

void render_refresh_border(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t border_color);

#endif /* RENDER_H */
