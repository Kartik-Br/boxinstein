/* lib/display_shim.c
 * Only compiled when USE_ILI9488 is active (see Makefile).
 * Provides the ili9341_draw_filled_rect symbol that lib/render.c
 * links against, forwarding to the BSP fill call.
 */
#ifdef USE_ILI9488

#include "stm32_adafruit_lcd.h"
#include <stdint.h>

void ili9341_draw_filled_rect(uint16_t x, uint16_t y,
                               uint16_t w, uint16_t h,
                               uint16_t color)
{
    BSP_LCD_SetTextColor(color);
    BSP_LCD_FillRect(x, y, w, h);
}

void ili9341_init(void)        { BSP_LCD_Init(); }
void ili9341_fill_screen(uint16_t color) {
    BSP_LCD_SetBackColor(color);
    BSP_LCD_Clear(color);
}

#endif
