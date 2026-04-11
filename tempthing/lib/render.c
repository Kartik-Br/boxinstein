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
    if (y < 0 || y >= (int16_t)ILI9341_HEIGHT || len <= 0) return;
    if (x < 0) { len += x; x = 0; }
    if (x + len > (int16_t)ILI9341_WIDTH) len = (int16_t)ILI9341_WIDTH - x;
    if (len <= 0) return;
    ili9341_draw_filled_rect((uint16_t)x, (uint16_t)y,
                              (uint16_t)len, 1u, color);
}

/*
 * Draw a vertical line of `len` pixels starting at (x, y).
 * Clips to display bounds.
 */
static void vline(int16_t x, int16_t y, int16_t len, uint16_t color) {
    if (x < 0 || x >= (int16_t)ILI9341_WIDTH || len <= 0) return;
    if (y < 0) { len += y; y = 0; }
    if (y + len > (int16_t)ILI9341_HEIGHT) len = (int16_t)ILI9341_HEIGHT - y;
    if (len <= 0) return;
    ili9341_draw_filled_rect((uint16_t)x, (uint16_t)y,
                              1u, (uint16_t)len, color);
}

/*
 * Erase a filled rectangle with the background colour.
 * Clips to display bounds.
 */
static void erase_rect(int16_t x, int16_t y,
                        int16_t w, int16_t h, uint16_t bg) {
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= (int16_t)ILI9341_WIDTH  || y >= (int16_t)ILI9341_HEIGHT) return;
    if (x + w > (int16_t)ILI9341_WIDTH)  w = (int16_t)ILI9341_WIDTH  - x;
    if (y + h > (int16_t)ILI9341_HEIGHT) h = (int16_t)ILI9341_HEIGHT - y;
    if (w <= 0 || h <= 0) return;
    ili9341_draw_filled_rect((uint16_t)x, (uint16_t)y,
                              (uint16_t)w, (uint16_t)h, bg);
}

/* ------------------------------------------------------------------ */
/*  Head-bob LUT (signed offsets, 8 steps, amplitude PS_BOB_AMP)     */
/*  Pattern: 0 up up peak up 0 down down — a simple triangle wave    */
/* ------------------------------------------------------------------ */
static const int8_t bob_lut[PS_BOB_STEPS] = {
     0, -1, -2, -1,   /* rising: neutral → up → peak → up */
     0,  1,  2,  1    /* falling: neutral → down → trough → down */
};
#define BOB_SHIFT  2   /* advance one LUT step every (1<<2)=4 pixels moved */

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
/* ------------------------------------------------------------------ */

void render_update_psprite(pSprite *ps, const Sprite *s,
                            uint32_t distance_moved) {

    if (!ps || !s) return;

    /* Walk phase: advance one LUT step every BOB_SHIFT pixels */
    uint8_t phase = (uint8_t)((distance_moved >> BOB_SHIFT) & (PS_BOB_STEPS - 1u));
    int8_t  bob   = bob_lut[phase];

    int16_t base_x = (int16_t)s->x;
    int16_t base_y = (int16_t)s->y;

    /* Body: top-left = (base_x, base_y) */
    ps->x_body = base_x + (PS_BODY_W / 2) - (PS_BODY_W / 2);  /* centred */
    ps->y_body  = base_y;   /* note: typo 'y_body' preserved from sprite.h  */

    /* Neck: centred above body, bobs with walk phase */
    ps->x_neck = base_x + (PS_BODY_W / 2) - (PS_NECK_W / 2);
    ps->y_neck = base_y - PS_NECK_H + (int16_t)bob;

    /* Head: centred above neck */
    ps->x_head = base_x + (PS_BODY_W / 2) - (PS_HEAD_W / 2);
    ps->y_head = ps->y_neck - PS_HEAD_H + (int16_t)bob;
}

/* ------------------------------------------------------------------ */
/*  render_draw_psprite                                                */
/* ------------------------------------------------------------------ */

void render_draw_psprite(const pSprite *ps, uint16_t color) {
    if (!ps) return;

    /* Body */
    render_draw_rect_outline((int16_t)ps->x_body, (int16_t)ps->y_body,
                              PS_BODY_W, PS_BODY_H, color);

    /* Neck (filled 1-pixel wide vertical line) */
    vline((int16_t)ps->x_neck + PS_NECK_W / 2,
          (int16_t)ps->y_neck,
          PS_NECK_H, color);

    /* Head */
    render_draw_rect_outline((int16_t)ps->x_head, (int16_t)ps->y_head,
                              PS_HEAD_W, PS_HEAD_H, color);
}

/* ------------------------------------------------------------------ */
/*  render_erase_psprite                                               */
/* ------------------------------------------------------------------ */

void render_erase_psprite(const pSprite *ps, uint16_t bg) {
    if (!ps) return;

    /* Erase head (filled rect to cover any bob artefacts) */
    erase_rect((int16_t)ps->x_head - 1, (int16_t)ps->y_head - 1,
               PS_HEAD_W + 2, PS_HEAD_H + PS_BOB_AMP + 2, bg);

    /* Erase neck */
    erase_rect((int16_t)ps->x_neck - 1, (int16_t)ps->y_neck - 1,
               PS_NECK_W + 2, PS_NECK_H + PS_BOB_AMP + 2, bg);

    /* Erase body */
    erase_rect((int16_t)ps->x_body, (int16_t)ps->y_body,
               PS_BODY_W, PS_BODY_H, bg);
}
