/**
 **************************************************************
 * File: mylib/render.c
 * Brief: Outline-rect drawing, delta-optimised movement, sprite
 *        animation, and border management.
 *
 * Compile with -DUSE_ILI9488 to target the ILI9488 BSP backend.
 * Without it, the ILI9341 direct driver is used.
 *
 * The only things that differ between backends are the three
 * low-level drawing primitives (hline, vline, plot_pixel) and
 * the screen-dimension queries.  Everything else — all the
 * geometry logic — is identical and appears once.
 ***************************************************************
 */

#include "render.h"

/* ================================================================== */
/*  BACKEND PRIMITIVES                                                */
/*  Wrap the two display APIs behind three identical static functions */
/*  so all geometry code above is backend-agnostic.                  */
/* ================================================================== */

/* ------------------------------------------------------------------ */
/*  ILI9488 backend (BSP LCD layer)                                   */
/* ------------------------------------------------------------------ */
#ifdef USE_ILI9488

static void hline(int16_t x, int16_t y, int16_t len, uint16_t color) {
    if (y < 0 || y >= RENDER_HEIGHT() || len <= 0) return;
    if (x < 0) { len += x; x = 0; }
    if (x + len > RENDER_WIDTH()) len = RENDER_WIDTH() - x;
    if (len <= 0) return;
    BSP_LCD_SetTextColor(color);
    BSP_LCD_FillRect((uint16_t)x, (uint16_t)y, (uint16_t)len, 1u);
}

static void vline(int16_t x, int16_t y, int16_t len, uint16_t color) {
    if (x < 0 || x >= RENDER_WIDTH() || len <= 0) return;
    if (y < 0) { len += y; y = 0; }
    if (y + len > RENDER_HEIGHT()) len = RENDER_HEIGHT() - y;
    if (len <= 0) return;
    BSP_LCD_SetTextColor(color);
    BSP_LCD_FillRect((uint16_t)x, (uint16_t)y, 1u, (uint16_t)len);
}

static void erase_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t bg) {
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= RENDER_WIDTH() || y >= RENDER_HEIGHT()) return;
    if (x + w > RENDER_WIDTH())  w = RENDER_WIDTH()  - x;
    if (y + h > RENDER_HEIGHT()) h = RENDER_HEIGHT() - y;
    if (w <= 0 || h <= 0) return;
    BSP_LCD_SetTextColor(bg);
    BSP_LCD_FillRect((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h);
}

static void plot_pixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= RENDER_WIDTH())  return;
    if (y < 0 || y >= RENDER_HEIGHT()) return;
    BSP_LCD_DrawPixel((uint16_t)x, (uint16_t)y, color);
}

/* ------------------------------------------------------------------ */
/*  ILI9341 backend (direct SPI driver)                               */
/* ------------------------------------------------------------------ */
#else /* USE_ILI9341 */

static void hline(int16_t x, int16_t y, int16_t len, uint16_t color) {
    if (y < 0 || y >= RENDER_HEIGHT() || len <= 0) return;
    if (x < 0) { len += x; x = 0; }
    if (x + len > RENDER_WIDTH()) len = RENDER_WIDTH() - x;
    if (len <= 0) return;
    ili9341_draw_filled_rect((uint16_t)x, (uint16_t)y, (uint16_t)len, 1u, color);
}

static void vline(int16_t x, int16_t y, int16_t len, uint16_t color) {
    if (x < 0 || x >= RENDER_WIDTH() || len <= 0) return;
    if (y < 0) { len += y; y = 0; }
    if (y + len > RENDER_HEIGHT()) len = RENDER_HEIGHT() - y;
    if (len <= 0) return;
    ili9341_draw_filled_rect((uint16_t)x, (uint16_t)y, 1u, (uint16_t)len, color);
}

static void erase_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t bg) {
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= RENDER_WIDTH() || y >= RENDER_HEIGHT()) return;
    if (x + w > RENDER_WIDTH())  w = RENDER_WIDTH()  - x;
    if (y + h > RENDER_HEIGHT()) h = RENDER_HEIGHT() - y;
    if (w <= 0 || h <= 0) return;
    ili9341_draw_filled_rect((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h, bg);
}

static void plot_pixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= RENDER_WIDTH())  return;
    if (y < 0 || y >= RENDER_HEIGHT()) return;
    ili9341_draw_filled_rect((uint16_t)x, (uint16_t)y, 1u, 1u, color);
}

#endif /* USE_ILI9488 */

/* ================================================================== */
/*  SHARED GEOMETRY — identical for both backends                     */
/* ================================================================== */

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                  */
/* ------------------------------------------------------------------ */

static inline int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline int16_t abs16(int16_t v) {
    return (int16_t)(v < 0 ? -v : v);
}

static void bresenham_line(int16_t x0, int16_t y0,
                            int16_t x1, int16_t y1, uint16_t color) {
    int16_t dx  =  abs16(x1 - x0);
    int16_t dy  =  abs16(y1 - y0);
    int16_t sx  = (x0 < x1) ?  1 : -1;
    int16_t sy  = (y0 < y1) ?  1 : -1;
    int16_t err = dx - dy;

    while (1) {
        plot_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = (int16_t)(err * 2);
        if (e2 > -dy) { err = (int16_t)(err - dy); x0 = (int16_t)(x0 + sx); }
        if (e2 <  dx) { err = (int16_t)(err + dx); y0 = (int16_t)(y0 + sy); }
    }
}

/* ------------------------------------------------------------------ */
/*  render_draw_border                                                */
/* ------------------------------------------------------------------ */

void render_draw_border(uint16_t color) {
    render_draw_rect_outline(BORDER_X, BORDER_Y, BORDER_W, BORDER_H, color);
}

/* ------------------------------------------------------------------ */
/*  render_draw_rect_outline                                          */
/* ------------------------------------------------------------------ */

void render_draw_rect_outline(int16_t x, int16_t y,
                               uint16_t w, uint16_t h, uint16_t color) {
    int16_t iw = (int16_t)w;
    int16_t ih = (int16_t)h;
    hline(x,          y,          iw, color);  /* top    */
    hline(x,          y + ih - 1, iw, color);  /* bottom */
    vline(x,          y,          ih, color);  /* left   */
    vline(x + iw - 1, y,          ih, color);  /* right  */
}

/* ------------------------------------------------------------------ */
/*  render_move_rect                                                  */
/*                                                                    */
/*  Erase only the two trailing strips left by the movement,         */
/*  then draw the new outline. O(|dx|*h + w*|dy|) pixels touched.   */
/* ------------------------------------------------------------------ */

void render_move_rect(int16_t ox, int16_t oy,
                       int16_t nx, int16_t ny,
                       uint16_t w, uint16_t h,
                       uint16_t fg, uint16_t bg) {
    int16_t dx  = (int16_t)(nx - ox);
    int16_t dy  = (int16_t)(ny - oy);
    int16_t adx = abs16(dx);
    int16_t ady = abs16(dy);
    int16_t iw  = (int16_t)w;
    int16_t ih  = (int16_t)h;

    /* Step 1: erase trailing vertical strip */
    if (dx != 0) {
        int16_t ex = (dx > 0) ? ox : (int16_t)(ox + iw - adx);
        int16_t ey = (dy >= 0) ? oy : (int16_t)(oy + dy);
        erase_rect(ex, ey, adx, (int16_t)(ih + ady), bg);
    }

    /* Step 2: erase trailing horizontal strip (no corner double-erase) */
    if (dy != 0) {
        int16_t ex = (dx >= 0) ? (int16_t)(ox + adx) : ox;
        int16_t ew = (int16_t)(iw - adx);
        int16_t ey = (dy > 0) ? oy : (int16_t)(oy + ih - ady);
        erase_rect(ex, ey, ew, ady, bg);
    }

    /* Step 3: draw new outline clamped to playfield */
    int16_t cx = clamp16(nx, BORDER_X, (int16_t)(BORDER_X + BORDER_W) - iw);
    int16_t cy = clamp16(ny, BORDER_Y, (int16_t)(BORDER_Y + BORDER_H) - ih);
    render_draw_rect_outline(cx, cy, w, h, fg);
}

/* ------------------------------------------------------------------ */
/*  render_resize_rect_outline                                        */
/* ------------------------------------------------------------------ */

void render_resize_rect_outline(int16_t x, int16_t y,
                                 uint16_t old_w, uint16_t old_h,
                                 uint16_t new_w, uint16_t new_h,
                                 uint16_t fg, uint16_t bg) {
    if (old_w == new_w && old_h == new_h) return;

    /* Erase old right and bottom edges */
    vline((int16_t)(x + old_w - 1), y, (int16_t)old_h, bg);
    hline(x, (int16_t)(y + old_h - 1), (int16_t)old_w, bg);

    /* Clean up remaining segments if shrinking */
    if (old_w > new_w) hline((int16_t)(x + new_w), y,
                               (int16_t)(old_w - new_w), bg);
    if (old_h > new_h) vline(x, (int16_t)(y + new_h),
                               (int16_t)(old_h - new_h), bg);

    render_draw_rect_outline(x, y, new_w, new_h, fg);
}

/* ------------------------------------------------------------------ */
/*  render_resize_rect                                                */
/* ------------------------------------------------------------------ */

void render_resize_rect(int16_t x,  int16_t y,
                         uint16_t ow, uint16_t oh,
                         uint16_t nw, uint16_t nh,
                         uint16_t fg, uint16_t bg) {
    int16_t iow = (int16_t)ow;
    int16_t ioh = (int16_t)oh;
    int16_t inw = (int16_t)nw;
    int16_t inh = (int16_t)nh;

    if (inw < iow) erase_rect((int16_t)(x + inw), y, iow - inw, ioh, bg);
    if (inh < ioh) erase_rect(x, (int16_t)(y + inh), inw, ioh - inh, bg);

    int16_t cx = clamp16(x, BORDER_X, (int16_t)(BORDER_X + BORDER_W) - inw);
    int16_t cy = clamp16(y, BORDER_Y, (int16_t)(BORDER_Y + BORDER_H) - inh);
    render_draw_rect_outline(cx, cy, nw, nh, fg);
}

/* ------------------------------------------------------------------ */
/*  render_draw_line / render_erase_line                              */
/* ------------------------------------------------------------------ */

void render_draw_line(int16_t x0, int16_t y0,
                       int16_t x1, int16_t y1, uint16_t color) {
    bresenham_line(x0, y0, x1, y1, color);
}

void render_erase_line(int16_t x0, int16_t y0,
                        int16_t x1, int16_t y1, uint16_t bg) {
    bresenham_line(x0, y0, x1, y1, bg);
}

/* ------------------------------------------------------------------ */
/*  render_refresh_border                                             */
/* ------------------------------------------------------------------ */

void render_refresh_border(int16_t x, int16_t y,
                            uint16_t w, uint16_t h,
                            uint16_t border_color) {
    int16_t rx2 = (int16_t)(x + w - 1);
    int16_t ry2 = (int16_t)(y + h - 1);

    if (y  <= BORDER_Y && ry2 >= BORDER_Y)
        hline(BORDER_X, BORDER_Y, (int16_t)BORDER_W, border_color);

    int16_t bby = (int16_t)(BORDER_Y + BORDER_H - 1u);
    if (y  <= bby && ry2 >= bby)
        hline(BORDER_X, bby, (int16_t)BORDER_W, border_color);

    if (x  <= BORDER_X && rx2 >= BORDER_X)
        vline(BORDER_X, BORDER_Y, (int16_t)BORDER_H, border_color);

    int16_t bbx = (int16_t)(BORDER_X + BORDER_W - 1u);
    if (x  <= bbx && rx2 >= bbx)
        vline(bbx, BORDER_Y, (int16_t)BORDER_H, border_color);
}

/* ------------------------------------------------------------------ */
/*  render_move_sprite                                                */
/* ------------------------------------------------------------------ */

void render_move_sprite(Sprite *s, int16_t dx, int16_t dy,
                         uint16_t w, uint16_t h,
                         uint16_t fg, uint16_t bg) {
    if (!s || !s->isExist) return;

    int16_t nx = clamp16((int16_t)(s->x + dx),
                          BORDER_X,
                          (int16_t)(BORDER_X + BORDER_W) - (int16_t)w);
    int16_t ny = clamp16((int16_t)(s->y + dy),
                          BORDER_Y,
                          (int16_t)(BORDER_Y + BORDER_H) - (int16_t)h);

    render_move_rect((int16_t)s->x, (int16_t)s->y, nx, ny, w, h, fg, bg);
    render_refresh_border(nx, ny, w, h, fg);

    s->x = nx;
    s->y = ny;
}

/* ------------------------------------------------------------------ */
/*  render_update_psprite                                             */
/* ------------------------------------------------------------------ */

void render_update_psprite(pSprite *ps, const Sprite *s,
                            int16_t head_offset_x, int16_t head_offset_y) {
    if (!ps || !s) return;

    int16_t base_x = (int16_t)s->x;
    int16_t base_y = (int16_t)s->y;

    ps->x_body = base_x;
    ps->y_body = base_y;   /* NOTE: sprite.h has a typo "y_boy" — fix to "y_body" */

    ps->x_neck = (int16_t)(base_x + PS_BODY_W / 2 - PS_NECK_W / 2);
    ps->y_neck = (int16_t)(base_y - PS_NECK_H);

    ps->x_head = (int16_t)(ps->x_neck + PS_NECK_W / 2 - PS_HEAD_W / 2 + head_offset_x);
    ps->y_head = (int16_t)(ps->y_neck - PS_HEAD_H + head_offset_y);
}

/* ------------------------------------------------------------------ */
/*  render_draw_psprite / render_erase_psprite                        */
/* ------------------------------------------------------------------ */

void render_draw_psprite(const pSprite *ps, uint16_t color) {
    if (!ps) return;
    render_draw_rect_outline((int16_t)ps->x_body, (int16_t)ps->y_body,
                              PS_BODY_W, PS_BODY_H, color);
    vline((int16_t)(ps->x_neck + PS_NECK_W / 2), (int16_t)ps->y_neck,
          PS_NECK_H, color);
    render_draw_rect_outline((int16_t)ps->x_head, (int16_t)ps->y_head,
                              PS_HEAD_W, PS_HEAD_H, color);
}

void render_erase_psprite(const pSprite *ps, uint16_t bg) {
    if (!ps) return;
    erase_rect((int16_t)(ps->x_head - 1), (int16_t)(ps->y_head - 1),
               PS_HEAD_W + 2, PS_HEAD_H + 2, bg);
    erase_rect((int16_t)(ps->x_neck - 1), (int16_t)(ps->y_neck - 1),
               PS_NECK_W + 2, PS_NECK_H + 2, bg);
    erase_rect((int16_t)(ps->x_body - 1), (int16_t)(ps->y_body - 1),
               PS_BODY_W + 2, PS_BODY_H + 2, bg);
}

/* ------------------------------------------------------------------ */
/*  render_draw_hand / render_erase_hand                              */
/*                                                                    */
/*  HandSprite is expected to have fields: xScr, yScr, size, isExist */
/*  If your struct uses x/y instead, compile with -DHAND_USE_XY.     */
/* ------------------------------------------------------------------ */

void render_draw_hand(const HandSprite *h, uint16_t color) {
    if (!h || !h->isExist || h->size <= 0) return;
#ifdef HAND_USE_XY
    render_draw_rect_outline((int16_t)(h->x    - h->size / 2),
                              (int16_t)(h->y    - h->size / 2),
#else
    render_draw_rect_outline((int16_t)(h->xScr - h->size / 2),
                              (int16_t)(h->yScr - h->size / 2),
#endif
                              (uint16_t)h->size, (uint16_t)h->size, color);
}

void render_erase_hand(const HandSprite *h, uint16_t bg) {
    if (!h || !h->isExist || h->size <= 0) return;
#ifdef HAND_USE_XY
    erase_rect((int16_t)(h->x    - h->size / 2),
               (int16_t)(h->y    - h->size / 2),
#else
    erase_rect((int16_t)(h->xScr - h->size / 2),
               (int16_t)(h->yScr - h->size / 2),
#endif
               (int16_t)h->size, (int16_t)h->size, bg);
}
