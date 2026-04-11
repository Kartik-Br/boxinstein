/**
 **************************************************************
 * File: mylib/render.c
 * Brief: Outline-rect drawing, delta-optimised movement, pSprite
 *        animation, and border management.
 *
 * Key idea — delta erasing
 * ------------------------
 * When a rect outline moves from (ox,oy) to (nx,ny) by (dx,dy):
 *
 *   Old outline pixels NOT covered by new outline form at most an
 *   L-shaped region made of two strips:
 *
 *     Vertical trailing strip   — the column(s) the rect slid away from
 *     Horizontal trailing strip — the row(s) the rect slid away from
 *
 *   Example: moving right (+dx) and down (+dy):
 *
 *     Erase left  strip: x=[ox .. ox+dx-1],       y=[oy .. oy+h+dy-1]
 *     Erase top   strip: x=[ox+dx .. ox+w+dx-1],  y=[oy .. oy+dy-1]
 *     (corner ox..ox+dx × oy..oy+dy counted once in vertical strip)
 *
 *   This is O(|dx|*H + W*|dy|) pixels — tiny for small motions.
 *
 * Border integrity
 * ----------------
 * After every move, render_refresh_border() checks whether the new
 * rect position touches any of the four border lines and redraws
 * only the affected segment(s). This is 4 comparisons + at most 4
 * short line draws.
 *
 * pSprite animation
 * -----------------
 * A pSprite is a stick figure: head, neck, body.  Limb positions are
 * recomputed each frame from the parent Sprite's (x,y) and a walk
 * phase.  The walk phase is a uint8_t index into an 8-entry head-bob
 * LUT, advanced by distance_moved >> BOB_SHIFT (i.e., one step every
 * 4 pixels of movement).  No floating-point math is used.
 ***************************************************************
 */

#include "render.h"
#include "ili9488.h"      
#include "stm32_adafruit_lcd.h"
/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

/* Clamp a value to [lo, hi] */
static inline int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* Integer absolute value for int16 */
static inline uint16_t abs16(int16_t v) {
    return (uint16_t)(v < 0 ? -v : v);
}

/*
 * Draw a horizontal line of `len` pixels starting at (x, y).
 * Clips to display bounds.
 */
static void hline(int16_t x, int16_t y, int16_t len, uint16_t color) {
    if (y < 0 || y >= (int16_t)ili9488_GetLcdPixelHeight() || len <= 0) return;
    if (x < 0) { len += x; x = 0; }
    if (x + len > (int16_t)ili9488_GetLcdPixelWidth()) len = (int16_t)ili9488_GetLcdPixelWidth() - x;
    if (len <= 0) return;
    
    BSP_LCD_SetTextColor(color);
    BSP_LCD_FillRect((uint16_t)x, (uint16_t)y, (uint16_t)len, 1u);
}

/*
 * Draw a vertical line of `len` pixels starting at (x, y).
 * Clips to display bounds.
 */
static void vline(int16_t x, int16_t y, int16_t len, uint16_t color) {
    if (x < 0 || x >= (int16_t)ili9488_GetLcdPixelWidth() || len <= 0) return;
    if (y < 0) { len += y; y = 0; }
    if (y + len > (int16_t)ili9488_GetLcdPixelHeight()) len = (int16_t)ili9488_GetLcdPixelHeight() - y;
    if (len <= 0) return;
    
    BSP_LCD_SetTextColor(color);
    BSP_LCD_FillRect((uint16_t)x, (uint16_t)y, 1u, (uint16_t)len);
}

/*
 * Erase a filled rectangle with the background colour.
 * Clips to display bounds.
 */
static void erase_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t bg) {
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= (int16_t)ili9488_GetLcdPixelWidth()  || y >= (int16_t)ili9488_GetLcdPixelHeight()) return;
    if (x + w > (int16_t)ili9488_GetLcdPixelWidth())  w = (int16_t)ili9488_GetLcdPixelWidth()  - x;
    if (y + h > (int16_t)ili9488_GetLcdPixelHeight()) h = (int16_t)ili9488_GetLcdPixelHeight() - y;
    if (w <= 0 || h <= 0) return;
    
    BSP_LCD_SetTextColor(bg);
    BSP_LCD_FillRect((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h);
}

/* ------------------------------------------------------------------ */
/*  render_draw_border                                                 */
/* ------------------------------------------------------------------ */

void render_draw_border(uint16_t color) {
    render_draw_rect_outline(BORDER_X, BORDER_Y,
                              BORDER_W, BORDER_H, color);
}

/* ------------------------------------------------------------------ */
/*  render_draw_rect_outline                                           */
/* ------------------------------------------------------------------ */

void render_draw_rect_outline(int16_t x, int16_t y,
                               uint16_t w, uint16_t h,
                               uint16_t color) {
    int16_t iw = (int16_t)w;
    int16_t ih = (int16_t)h;

    hline(x,          y,          iw, color);   /* top    */
    hline(x,          y + ih - 1, iw, color);   /* bottom */
    vline(x,          y,          ih, color);   /* left   */
    vline(x + iw - 1, y,          ih, color);   /* right  */
}

/* ------------------------------------------------------------------ */
/*  render_move_rect                                                   */
/*                                                                     */
/*  Core delta-erase algorithm:                                        */
/*                                                                     */
/*  Given movement (dx, dy) = (nx-ox, ny-oy):                        */
/*                                                                     */
/*  Step 1 — erase trailing VERTICAL strip                            */
/*    If dx > 0: left side moved away. Erase x=[ox..ox+dx), y=[oy..oy+h+|dy|)  */
/*    If dx < 0: right side moved away. Erase x=[ox+w+dx..ox+w), same y range  */
/*    Width  = |dx|,  Height = h + |dy|  (covers the full swept column)         */
/*                                                                     */
/*  Step 2 — erase trailing HORIZONTAL strip                          */
/*    If dy > 0: top side moved away. Erase y=[oy..oy+dy), x=[ox+|dx|..ox+w)   */
/*    If dy < 0: bot side moved away. Erase y=[oy+h+dy..oy+h), same x range    */
/*    Height = |dy|,  Width = w - |dx|  (the corner was already erased)         */
/*                                                                     */
/*  Step 3 — draw new outline, clamped to playfield interior          */
/* ------------------------------------------------------------------ */

void render_move_rect(int16_t ox, int16_t oy,
                       int16_t nx, int16_t ny,
                       uint16_t w,  uint16_t h,
                       uint16_t fg, uint16_t bg) {

    int16_t dx = nx - ox;
    int16_t dy = ny - oy;
    int16_t adx = (int16_t)abs16(dx);
    int16_t ady = (int16_t)abs16(dy);
    int16_t iw  = (int16_t)w;
    int16_t ih  = (int16_t)h;

    /* --- Step 1: erase trailing vertical strip --- */
    if (dx != 0) {
        int16_t ex = (dx > 0) ? ox : (ox + iw - adx);
        int16_t ey = (dy >= 0) ? oy : oy + dy;      /* extend upward if moved up */
        int16_t eh = ih + ady;
        erase_rect(ex, ey, adx, eh, bg);
    }

    /* --- Step 2: erase trailing horizontal strip --- */
    if (dy != 0) {
        /* X range covers only the part NOT already erased by step 1 */
        int16_t ex, ew;
        if (dx >= 0) {
            ex = ox + adx;           /* start after the vertical strip */
            ew = iw - adx;
        } else {
            ex = ox;
            ew = iw - adx;
        }
        int16_t ey = (dy > 0) ? oy : (oy + ih - ady);
        erase_rect(ex, ey, ew, ady, bg);
    }

    /* --- Step 3: draw new outline (clamped to playfield) --- */
    /* Clamp to playfield interior so border is never overdrawn */
    int16_t cx = clamp16(nx, BORDER_X,
                          (int16_t)(BORDER_X + BORDER_W) - iw);
    int16_t cy = clamp16(ny, BORDER_Y,
                          (int16_t)(BORDER_Y + BORDER_H) - ih);

    render_draw_rect_outline(cx, cy, w, h, fg);
}

/* ------------------------------------------------------------------ */
/*  render_refresh_border                                              */
/*                                                                     */
/*  Check all four border segments against the rect bounding box.    */
/*  Redraw any segment that the rect touches or overlaps.             */
/* ------------------------------------------------------------------ */

void render_refresh_border(int16_t x, int16_t y,
                            uint16_t w, uint16_t h,
                            uint16_t border_color) {

    int16_t rx2 = x + (int16_t)w - 1;
    int16_t ry2 = y + (int16_t)h - 1;

    /* Top border: y == BORDER_Y */
    if (y <= BORDER_Y && ry2 >= BORDER_Y)
        hline(BORDER_X, BORDER_Y, (int16_t)BORDER_W, border_color);

    /* Bottom border: y == BORDER_Y + BORDER_H - 1 */
    int16_t bby = (int16_t)(BORDER_Y + BORDER_H - 1u);
    if (y <= bby && ry2 >= bby)
        hline(BORDER_X, bby, (int16_t)BORDER_W, border_color);

    /* Left border: x == BORDER_X */
    if (x <= BORDER_X && rx2 >= BORDER_X)
        vline(BORDER_X, BORDER_Y, (int16_t)BORDER_H, border_color);

    /* Right border: x == BORDER_X + BORDER_W - 1 */
    int16_t bbx = (int16_t)(BORDER_X + BORDER_W - 1u);
    if (x <= bbx && rx2 >= bbx)
        vline(bbx, BORDER_Y, (int16_t)BORDER_H, border_color);
}

/* ------------------------------------------------------------------ */
/*  render_move_sprite                                                 */
/* ------------------------------------------------------------------ */

void render_move_sprite(Sprite *s, int16_t dx, int16_t dy,
                         uint16_t w, uint16_t h,
                         uint16_t fg, uint16_t bg) {

    if (!s || !s->isExist) return;

    /* Clamp new position to playfield interior */
    int16_t nx = clamp16((int16_t)s->x + dx,
                          BORDER_X,
                          (int16_t)(BORDER_X + BORDER_W) - (int16_t)w);
    int16_t ny = clamp16((int16_t)s->y + dy,
                          BORDER_Y,
                          (int16_t)(BORDER_Y + BORDER_H) - (int16_t)h);

    render_move_rect((int16_t)s->x, (int16_t)s->y,
                      nx, ny, w, h, fg, bg);

    /* Refresh border if we moved close to an edge */
    render_refresh_border(nx, ny, w, h, fg);   /* pass border_color separately if needed */

    s->x = nx;
    s->y = ny;
}

/* ------------------------------------------------------------------ */
/*  render_update_psprite                                              */
/*                                                                     */
/*  Recompute all pSprite limb pixel positions.                       */
/*  Coordinate system: y increases downward (screen coords).         */
/*  Sprite (x,y) = top-left of the body rect.                        */
/*                                                                     */
/*  All limbs are rigidly tethered to the body — no animation.       */
/*  Head sits on top of the neck which sits on top of the body.      */
/*  Arms hang from the sides of the body at PS_ARM_Y_OFF.            */
/* ------------------------------------------------------------------ */

void render_update_psprite(pSprite *ps, const Sprite *s,
                            uint32_t distance_moved) {
    (void)distance_moved;   /* reserved for future use; unused now */

    if (!ps || !s) return;

    int16_t bx = (int16_t)s->x;
    int16_t by = (int16_t)s->y;

    /* Body: top-left = (bx, by) */
    ps->x_body = bx;
    ps->y_body = by;

    /* Neck: centred horizontally on body, sits directly above it */
    ps->x_neck = bx + (PS_BODY_W / 2) - (PS_NECK_W / 2);
    ps->y_neck = by - PS_NECK_H;

    /* Head: centred horizontally on body, sits directly above neck */
    ps->x_head = bx + (PS_BODY_W / 2) - (PS_HEAD_W / 2);
    ps->y_head = by - PS_NECK_H - PS_HEAD_H;

    /* Left arm: attached to the left side of the body */
    ps->x_larm = bx - PS_ARM_W;
    ps->y_larm = by + PS_ARM_Y_OFF;

    /* Right arm: attached to the right side of the body */
    ps->x_rarm = bx + PS_BODY_W;
    ps->y_rarm = by + PS_ARM_Y_OFF;

    /* Initialise fist sizes on first call (zero → uninitialised) */
    if (ps->lfist_sz == 0) ps->lfist_sz = PS_FIST_MIN;
    if (ps->rfist_sz == 0) ps->rfist_sz = PS_FIST_MIN;

    /*
     * Fist position: centred horizontally on the arm's outer face,
     * centred vertically on the arm midpoint.
     *
     * Left fist outer face = bx - PS_ARM_W  (left edge of left arm).
     * Centre of arm vertically = y_larm + PS_ARM_H/2.
     * Fist top-left = (outer_face - fist_sz,  arm_mid - fist_sz/2).
     */
    int16_t arm_mid_y = (int16_t)ps->y_larm + PS_ARM_H / 2;

    ps->x_lfist = (bx - PS_ARM_W) - (int16_t)ps->lfist_sz;
    ps->y_lfist = arm_mid_y       - (int16_t)(ps->lfist_sz / 2);

    /* Right fist outer face = bx + PS_BODY_W + PS_ARM_W */
    ps->x_rfist = bx + PS_BODY_W + PS_ARM_W;
    ps->y_rfist = arm_mid_y - (int16_t)(ps->rfist_sz / 2);
}

/* ------------------------------------------------------------------ */
/*  render_draw_psprite                                                */
/* ------------------------------------------------------------------ */

void render_draw_psprite(const pSprite *ps, uint16_t color) {
    if (!ps) return;

    /* Body */
    render_draw_rect_outline((int16_t)ps->x_body, (int16_t)ps->y_body,
                              PS_BODY_W, PS_BODY_H, color);

    /* Neck (single vertical line, centred) */
    vline((int16_t)ps->x_neck + PS_NECK_W / 2,
          (int16_t)ps->y_neck,
          PS_NECK_H, color);

    /* Head */
    render_draw_rect_outline((int16_t)ps->x_head, (int16_t)ps->y_head,
                              PS_HEAD_W, PS_HEAD_H, color);

    /* Left arm (fixed rect beside body) */
    render_draw_rect_outline((int16_t)ps->x_larm, (int16_t)ps->y_larm,
                              PS_ARM_W, PS_ARM_H, color);

    /* Right arm */
    render_draw_rect_outline((int16_t)ps->x_rarm, (int16_t)ps->y_rarm,
                              PS_ARM_W, PS_ARM_H, color);

    /* Left fist (square centred on arm end, grows when punching) */
    render_draw_rect_outline((int16_t)ps->x_lfist, (int16_t)ps->y_lfist,
                              ps->lfist_sz, ps->lfist_sz, color);

    /* Right fist */
    render_draw_rect_outline((int16_t)ps->x_rfist, (int16_t)ps->y_rfist,
                              ps->rfist_sz, ps->rfist_sz, color);
}

/* ------------------------------------------------------------------ */
/*  render_erase_psprite                                               */
/* ------------------------------------------------------------------ */

void render_erase_psprite(const pSprite *ps, uint16_t bg) {
    if (!ps) return;

    /* Erase head (+1px padding on all sides) */
    erase_rect((int16_t)ps->x_head - 1, (int16_t)ps->y_head - 1,
               PS_HEAD_W + 2, PS_HEAD_H + 2, bg);

    /* Erase neck */
    erase_rect((int16_t)ps->x_neck - 1, (int16_t)ps->y_neck - 1,
               PS_NECK_W + 2, PS_NECK_H + 2, bg);

    /* Erase body */
    erase_rect((int16_t)ps->x_body, (int16_t)ps->y_body,
               PS_BODY_W, PS_BODY_H, bg);

    /* Erase left arm */
    erase_rect((int16_t)ps->x_larm, (int16_t)ps->y_larm,
               PS_ARM_W, PS_ARM_H, bg);

    /* Erase right arm */
    erase_rect((int16_t)ps->x_rarm, (int16_t)ps->y_rarm,
               PS_ARM_W, PS_ARM_H, bg);

    /*
     * Erase fists at their CURRENT (possibly large) size.
     * +1px padding covers the outline pixel on all sides.
     */
    erase_rect((int16_t)ps->x_lfist - 1, (int16_t)ps->y_lfist - 1,
               (int16_t)ps->lfist_sz + 2, (int16_t)ps->lfist_sz + 2, bg);
    erase_rect((int16_t)ps->x_rfist - 1, (int16_t)ps->y_rfist - 1,
               (int16_t)ps->rfist_sz + 2, (int16_t)ps->rfist_sz + 2, bg);
}

/* ------------------------------------------------------------------ */
/*  render_resize_rect                                                 */
/*                                                                     */
/*  Resize a rect outline at (x,y) from (ow x oh) to (nw x nh).     */
/*                                                                     */
/*  Delta-erase strategy (mirrors render_move_rect but for size):    */
/*                                                                     */
/*  Growing (nw > ow or nh > oh):                                    */
/*    No pixels need erasing — just draw the new (larger) outline.   */
/*    The old inner corners are automatically covered because the     */
/*    new outline shares the same top-left origin.                    */
/*                                                                     */
/*  Shrinking (nw < ow or nh < oh):                                  */
/*    Right trailing strip : x=[x+nw .. x+ow), full old height       */
/*    Bottom trailing strip: y=[y+nh .. y+oh), width only new width  */
/*    (corner is counted once in the vertical strip)                  */
/* ------------------------------------------------------------------ */

void render_resize_rect(int16_t x,  int16_t y,
                         uint16_t ow, uint16_t oh,
                         uint16_t nw, uint16_t nh,
                         uint16_t fg, uint16_t bg) {

    int16_t iow = (int16_t)ow;
    int16_t ioh = (int16_t)oh;
    int16_t inw = (int16_t)nw;
    int16_t inh = (int16_t)nh;

    /* --- Erase trailing right strip when shrinking horizontally --- */
    if (inw < iow) {
        /* x=[x+nw .. x+ow), full OLD height */
        erase_rect(x + inw, y, iow - inw, ioh, bg);
    }

    /* --- Erase trailing bottom strip when shrinking vertically --- */
    if (inh < ioh) {
        /* y=[y+nh .. y+oh), only NEW width (right strip already erased) */
        erase_rect(x, y + inh, inw, ioh - inh, bg);
    }

    /* --- Draw new outline (clamped to playfield) --- */
    int16_t cx = clamp16(x, BORDER_X,
                          (int16_t)(BORDER_X + BORDER_W) - inw);
    int16_t cy = clamp16(y, BORDER_Y,
                          (int16_t)(BORDER_Y + BORDER_H) - inh);
    render_draw_rect_outline(cx, cy, nw, nh, fg);
}

/* ------------------------------------------------------------------ */
/*  render_draw_line  /  render_erase_line                            */
/*                                                                     */
/*  Bresenham's line algorithm.  Each pixel is plotted individually   */
/*  via a 1x1 filled rect so it uses the same ILI9341 primitive as   */
/*  every other render function.  Clips each pixel to display bounds. */
/* ------------------------------------------------------------------ */
/* * Replaces the old plot_pixel
 */
static void plot_pixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= (int16_t)ili9488_GetLcdPixelWidth())  return;
    if (y < 0 || y >= (int16_t)ili9488_GetLcdPixelHeight()) return;
    
    BSP_LCD_DrawPixel((uint16_t)x, (uint16_t)y, color);
}

static void bresenham_line(int16_t x0, int16_t y0,
                            int16_t x1, int16_t y1,
                            uint16_t color) {
    int16_t dx  =  abs16(x1 - x0);
    int16_t dy  =  abs16(y1 - y0);
    int16_t sx  = (x0 < x1) ? 1 : -1;
    int16_t sy  = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;

    while (1) {
        plot_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

void render_draw_line(int16_t x0, int16_t y0,
                      int16_t x1, int16_t y1,
                      uint16_t color) {
    bresenham_line(x0, y0, x1, y1, color);
}

void render_erase_line(int16_t x0, int16_t y0,
                       int16_t x1, int16_t y1,
                       uint16_t bg) {
    bresenham_line(x0, y0, x1, y1, bg);
}