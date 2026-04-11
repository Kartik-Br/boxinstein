/**
 **************************************************************
 * File: ILI9488/lib/render.c
 * Brief: Outline-rect drawing, delta-optimised movement, pSprite
 * animation, and border management for ILI9488.
 *
 * OPTIMISATION NOTES (matching ILI9341 render speed):
 *
 * The ILI9488 in SPI mode must send 24-bit pixels instead of 16-bit,
 * making every FillRect ~50% more bytes. To compensate we:
 *
 *  1. COLOR CACHE: Track the last colour written to DrawProp.TextColor.
 *     Skip BSP_LCD_SetTextColor() when the colour has not changed.
 *     A rect outline = 4 segments all the same colour — this removes
 *     3 of the 4 redundant state-write+BSP-wrapper calls per outline.
 *
 *  2. DIRECT DRIVER CALLS: Call ili9488_DrawHLine / ili9488_DrawVLine /
 *     ili9488_FillRect directly instead of going through BSP wrappers.
 *     Each BSP wrapper re-reads DrawProp.TextColor and checks a
 *     function pointer — small overhead but it adds up on every call.
 *
 *  3. BATCHED COLOUR SET: Public functions that draw multiple segments
 *     (rect outline, pSprite, etc.) call set_color() ONCE at the top
 *     rather than letting each helper call it independently.
 *
 *  4. INLINE HELPERS: hline/vline/erase_rect are static inline so the
 *     compiler can fold them directly into callers.
 ***************************************************************
 */

#include "render.h"
#include "ili9488.h"
#include "stm32_adafruit_lcd.h"

/* ------------------------------------------------------------------ */
/* Color cache                                                        */
/* ------------------------------------------------------------------ */

#define COLOR_INVALID  0xFFFFFFFFUL   /* cannot be a real RGB565 value */

static uint32_t s_current_color = COLOR_INVALID;

static inline void set_color(uint16_t color)
{
    if ((uint32_t)color != s_current_color) {
        BSP_LCD_SetTextColor(color);
        s_current_color = (uint32_t)color;
    }
}

/* ------------------------------------------------------------------ */
/* Internal helpers                                                  */
/* ------------------------------------------------------------------ */

static inline int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline uint16_t abs16(int16_t v) {
    return (uint16_t)(v < 0 ? -v : v);
}

/* Callers must call set_color() before these helpers */

static inline void hline(int16_t x, int16_t y, int16_t len, uint16_t color)
{
    if (y < 0 || y >= (int16_t)BSP_LCD_GetYSize() || len <= 0) return;
    if (x < 0) { len += x; x = 0; }
    if (x + len > (int16_t)BSP_LCD_GetXSize()) len = (int16_t)BSP_LCD_GetXSize() - x;
    if (len <= 0) return;
    set_color(color);
    ili9488_DrawHLine(color, (uint16_t)x, (uint16_t)y, (uint16_t)len);
}

static inline void vline(int16_t x, int16_t y, int16_t len, uint16_t color)
{
    if (x < 0 || x >= (int16_t)BSP_LCD_GetXSize() || len <= 0) return;
    if (y < 0) { len += y; y = 0; }
    if (y + len > (int16_t)BSP_LCD_GetYSize()) len = (int16_t)BSP_LCD_GetYSize() - y;
    if (len <= 0) return;
    set_color(color);
    ili9488_DrawVLine(color, (uint16_t)x, (uint16_t)y, (uint16_t)len);
}

static inline void erase_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t bg)
{
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= (int16_t)BSP_LCD_GetXSize()  || y >= (int16_t)BSP_LCD_GetYSize()) return;
    if (x + w > (int16_t)BSP_LCD_GetXSize())  w = (int16_t)BSP_LCD_GetXSize()  - x;
    if (y + h > (int16_t)BSP_LCD_GetYSize()) h = (int16_t)BSP_LCD_GetYSize() - y;
    if (w <= 0 || h <= 0) return;
    set_color(bg);
    ili9488_FillRect((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h, bg);
}

static inline void plot_pixel(int16_t x, int16_t y, uint16_t color)
{
    if (x < 0 || x >= (int16_t)BSP_LCD_GetXSize()) return;
    if (y < 0 || y >= (int16_t)BSP_LCD_GetYSize()) return;
    set_color(color);
    BSP_LCD_DrawPixel((uint16_t)x, (uint16_t)y, color);
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

void render_draw_border(uint16_t color) {
    render_draw_rect_outline(BORDER_X, BORDER_Y, BORDER_W, BORDER_H, color);
}

/* set_color() called ONCE — all 4 hline/vline calls skip the BSP wrapper */
void render_draw_rect_outline(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    int16_t iw = (int16_t)w;
    int16_t ih = (int16_t)h;
    set_color(color);
    hline(x,          y,          iw, color);
    hline(x,          y + ih - 1, iw, color);
    vline(x,          y,          ih, color);
    vline(x + iw - 1, y,          ih, color);
}

void render_move_rect(int16_t ox, int16_t oy, int16_t nx, int16_t ny,
                      uint16_t w, uint16_t h, uint16_t fg, uint16_t bg)
{
    int16_t dx  = nx - ox;
    int16_t dy  = ny - oy;
    int16_t adx = (int16_t)abs16(dx);
    int16_t ady = (int16_t)abs16(dy);
    int16_t iw  = (int16_t)w;
    int16_t ih  = (int16_t)h;

    set_color(bg);
    if (dx != 0) {
        int16_t ex = (dx > 0) ? ox : (ox + iw - adx);
        int16_t ey = (dy >= 0) ? oy : oy + dy;
        int16_t eh = ih + ady;
        erase_rect(ex, ey, adx, eh, bg);
    }
    if (dy != 0) {
        int16_t ex, ew;
        if (dx >= 0) { ex = ox + adx; ew = iw - adx; }
        else         { ex = ox;       ew = iw - adx; }
        int16_t ey = (dy > 0) ? oy : (oy + ih - ady);
        erase_rect(ex, ey, ew, ady, bg);
    }

    int16_t cx = clamp16(nx, BORDER_X, (int16_t)(BORDER_X + BORDER_W) - iw);
    int16_t cy = clamp16(ny, BORDER_Y, (int16_t)(BORDER_Y + BORDER_H) - ih);
    render_draw_rect_outline(cx, cy, w, h, fg);
}

void render_resize_rect_outline(int16_t x, int16_t y,
                                uint16_t old_w, uint16_t old_h,
                                uint16_t new_w, uint16_t new_h,
                                uint16_t fg, uint16_t bg)
{
    if (old_w == new_w && old_h == new_h) return;
    set_color(bg);
    vline(x + old_w - 1, y, old_h, bg);
    hline(x, y + old_h - 1, old_w, bg);
    if (old_w > new_w) hline(x + new_w, y, old_w - new_w, bg);
    if (old_h > new_h) vline(x, y + new_h, old_h - new_h, bg);
    render_draw_rect_outline(x, y, new_w, new_h, fg);
}

void render_resize_rect(int16_t x,  int16_t y,
                        uint16_t ow, uint16_t oh,
                        uint16_t nw, uint16_t nh,
                        uint16_t fg, uint16_t bg)
{
    int16_t iow = (int16_t)ow;
    int16_t ioh = (int16_t)oh;
    int16_t inw = (int16_t)nw;
    int16_t inh = (int16_t)nh;

    set_color(bg);
    if (inw < iow) erase_rect(x + inw, y,       iow - inw, ioh, bg);
    if (inh < ioh) erase_rect(x,       y + inh, inw,       ioh - inh, bg);

    int16_t cx = clamp16(x, BORDER_X, (int16_t)(BORDER_X + BORDER_W) - inw);
    int16_t cy = clamp16(y, BORDER_Y, (int16_t)(BORDER_Y + BORDER_H) - inh);
    render_draw_rect_outline(cx, cy, nw, nh, fg);
}

void render_refresh_border(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t border_color)
{
    int16_t rx2 = x + (int16_t)w - 1;
    int16_t ry2 = y + (int16_t)h - 1;

    set_color(border_color);

    if (y <= BORDER_Y && ry2 >= BORDER_Y)
        hline(BORDER_X, BORDER_Y, (int16_t)BORDER_W, border_color);

    int16_t bby = (int16_t)(BORDER_Y + BORDER_H - 1u);
    if (y <= bby && ry2 >= bby)
        hline(BORDER_X, bby, (int16_t)BORDER_W, border_color);

    if (x <= BORDER_X && rx2 >= BORDER_X)
        vline(BORDER_X, BORDER_Y, (int16_t)BORDER_H, border_color);

    int16_t bbx = (int16_t)(BORDER_X + BORDER_W - 1u);
    if (x <= bbx && rx2 >= bbx)
        vline(bbx, BORDER_Y, (int16_t)BORDER_H, border_color);
}

/* ------------------------------------------------------------------ */
/* Line drawing                                                       */
/* ------------------------------------------------------------------ */

static void bresenham_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    int16_t dx  =  abs16(x1 - x0);
    int16_t dy  =  abs16(y1 - y0);
    int16_t sx  = (x0 < x1) ? 1 : -1;
    int16_t sy  = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;

    set_color(color);   /* once for the whole line */
    while (1) {
        plot_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

void render_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    bresenham_line(x0, y0, x1, y1, color);
}

void render_erase_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t bg) {
    bresenham_line(x0, y0, x1, y1, bg);
}

/* ------------------------------------------------------------------ */
/* Sprite helpers                                                     */
/* ------------------------------------------------------------------ */

void render_move_sprite(Sprite *s, int16_t dx, int16_t dy,
                        uint16_t w, uint16_t h, uint16_t fg, uint16_t bg)
{
    if (!s || !s->isExist) return;

    int16_t nx = clamp16((int16_t)s->x + dx, BORDER_X, (int16_t)(BORDER_X + BORDER_W) - (int16_t)w);
    int16_t ny = clamp16((int16_t)s->y + dy, BORDER_Y, (int16_t)(BORDER_Y + BORDER_H) - (int16_t)h);

    render_move_rect((int16_t)s->x, (int16_t)s->y, nx, ny, w, h, fg, bg);
    render_refresh_border(nx, ny, w, h, fg);

    s->x = nx;
    s->y = ny;
}

void render_update_psprite(pSprite *ps, const Sprite *s, int16_t head_offset_x, int16_t head_offset_y)
{
    if (!ps || !s) return;

    int16_t base_x = (int16_t)s->x;
    int16_t base_y = (int16_t)s->y;

    ps->x_body = base_x;
    ps->y_body = base_y;

    ps->x_neck = base_x + (PS_BODY_W / 2) - (PS_NECK_W / 2);
    ps->y_neck = base_y - PS_NECK_H;

    ps->x_head = ps->x_neck + (PS_NECK_W / 2) - (PS_HEAD_W / 2) + head_offset_x;
    ps->y_head = ps->y_neck - PS_HEAD_H + head_offset_y;
}

/* draw all 3 parts with ONE set_color() call */
void render_draw_psprite(const pSprite *ps, uint16_t color)
{
    if (!ps) return;
    set_color(color);
    render_draw_rect_outline((int16_t)ps->x_body, (int16_t)ps->y_body, PS_BODY_W, PS_BODY_H, color);
    vline((int16_t)ps->x_neck + PS_NECK_W / 2, (int16_t)ps->y_neck, PS_NECK_H, color);
    render_draw_rect_outline((int16_t)ps->x_head, (int16_t)ps->y_head, PS_HEAD_W, PS_HEAD_H, color);
}

/* erase all 3 parts with ONE set_color() call */
void render_erase_psprite(const pSprite *ps, uint16_t bg)
{
    if (!ps) return;
    set_color(bg);
    erase_rect((int16_t)ps->x_head - 1, (int16_t)ps->y_head - 1, PS_HEAD_W + 2, PS_HEAD_H + 2, bg);
    erase_rect((int16_t)ps->x_neck - 1, (int16_t)ps->y_neck - 1, PS_NECK_W + 2, PS_NECK_H + 2, bg);
    erase_rect((int16_t)ps->x_body - 1, (int16_t)ps->y_body - 1, PS_BODY_W + 2, PS_BODY_H + 2, bg);
}

void render_draw_hand(const HandSprite *h, uint16_t color)
{
    if (!h || !h->isExist || h->size <= 0) return;
    set_color(color);
    render_draw_rect_outline((int16_t)(h->x - h->size/2), (int16_t)(h->y - h->size/2),
                             (uint16_t)h->size, (uint16_t)h->size, color);
}

void render_erase_hand(const HandSprite *h, uint16_t bg)
{
    if (!h || !h->isExist || h->size <= 0) return;
    set_color(bg);
    erase_rect((int16_t)(h->x - h->size/2), (int16_t)(h->y - h->size/2),
               (int16_t)h->size, (int16_t)h->size, bg);
}
