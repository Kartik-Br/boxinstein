#include "processor_hal.h"
#include "ili9341.h"
#include "render.h"
#include "sprite.h"

#define FRAME_DELAY_MS  30

#define COL_BG      ILI9341_BLACK
#define COL_BORDER  ILI9341_WHITE
#define COL_RECT    ILI9341_YELLOW
#define COL_SPRITE  ILI9341_CYAN
#define COL_LINE    ILI9341_MAGENTA

static int16_t  rect_x = 40;
static int16_t  rect_y = 40;
static uint16_t rect_w = 20;
static uint16_t rect_h = 20;

static Sprite   guy;
static pSprite  guy_ps;
static HandSprite right_hand;

/* State for erasing the line frame-to-frame */
static int16_t last_line_x0, last_line_y0, last_line_x1, last_line_y1;
// Camera state
int16_t cam_x = 0; // Negative = player moved left, World moves right
int16_t cam_y = 0; // Positive = player ducked, World moves up
static void demo_init(void);
static void demo_loop(void);
void render_wireframe_ring(int16_t cx, int16_t cy, uint16_t color);

int main(void) {
    HAL_Init();
    demo_init();

    while (1) {
        demo_loop();
    }
}

static void demo_init(void) {
    ili9341_init();
    ili9341_fill_screen(COL_BG);
    render_draw_border(COL_BORDER);

    /* Setup player */
    guy.x = 100;
    guy.y = 120;
    guy.isExist = 1;
    guy.spriteType = S_GUY;
    render_update_psprite(&guy_ps, &guy, 0, 0);
    render_draw_psprite(&guy_ps, COL_SPRITE);

    /* Setup Hand */
    right_hand.isExist = true;
    right_hand.x = 140;
    right_hand.y = 100;
    right_hand.size = 6;

    render_draw_rect_outline(rect_x, rect_y, rect_w, rect_h, COL_RECT);
}

static void demo_loop(void) {
    static int frame = 0;
    frame++;

    /* --- 1. Delta-Erased Resizing Rect --- */
    /* Pulse size between 20 and 40 */
    uint16_t target_w = 20 + (frame % 20);
    uint16_t target_h = 20 + ((frame/2) % 20);

    render_resize_rect_outline(rect_x, rect_y, rect_w, rect_h, target_w, target_h, COL_RECT, COL_BG);
    rect_w = target_w;
    rect_h = target_h;

    /* --- 2. Head freely orbiting neck --- */
    render_erase_psprite(&guy_ps, COL_BG);

    int16_t head_offset_x = (frame % 10 < 5) ? 2 : -2;
    int16_t head_offset_y = (frame % 8 < 4) ? -1 : 1;

    render_update_psprite(&guy_ps, &guy, head_offset_x, head_offset_y);
    render_draw_psprite(&guy_ps, COL_SPRITE);

    /* --- 3. Hand Sprite (Z-depth mapping) --- */
    render_erase_hand(&right_hand, COL_BG);
    right_hand.size = 5 + (frame % 15); // Pulsing depth to simulate Z movement
    render_draw_hand(&right_hand, ILI9341_YELLOW);

    /* --- 4. Draw Line --- */
    /* Erase old line using background color */
    render_draw_line(last_line_x0, last_line_y0, last_line_x1, last_line_y1, COL_BG);

    /* Draw new line from the player's freely moving head to the hand */
    render_draw_line(guy_ps.x_head, guy_ps.y_head, right_hand.x, right_hand.y, COL_LINE);

    /* Save coords to erase on next frame */
    last_line_x0 = guy_ps.x_head;
    last_line_y0 = guy_ps.y_head;
    last_line_x1 = right_hand.x;
    last_line_y1 = right_hand.y;

    HAL_Delay(FRAME_DELAY_MS);
}

void render_wireframe_ring(int16_t cx, int16_t cy, uint16_t color) {
    // 1. Calculate the dynamic horizon position
    // Default horizon is middle of screen (120).
    // If player ducks (cy > 0), horizon moves UP.
    int16_t horizon_y = 120 - cy;

    // 2. Calculate the vanishing point X
    // Default VP is middle of screen (120).
    int16_t vp_x = 120 - cx;

    // 3. Draw the Horizon
    render_draw_line(BORDER_X, horizon_y, BORDER_X + BORDER_W, horizon_y, color);

    // 4. Draw Perspective Lines (The Floor/Ropes)
    // These start at the bottom of the screen and converge at the Vanishing Point
    // Left edge of ring
    render_draw_line(0, 240, vp_x, horizon_y, color);
    // Right edge of ring
    render_draw_line(240, 240, vp_x, horizon_y, color);

    // Optional: Add a few "floor tiles" lines
    render_draw_line(60, 240, vp_x, horizon_y, color);
    render_draw_line(180, 240, vp_x, horizon_y, color);
}
