/**
 **************************************************************
 * File: mylib/ili9341.h
 * Brief: ILI9341 TFT LCD driver for STM32 NUCLEO-F429ZI
 *        Interfaced via SPI1 (PA5=SCK, PA7=MOSI, PA6=MISO)
 *        CS=PB6, RESET=PA9, DC=PC7
 ***************************************************************
 * EXTERNAL FUNCTIONS
 ***************************************************************
 * ili9341_init()                          - Initialise the ILI9341 display
 * ili9341_draw_filled_rect(x, y, w, h, color) - Draw a filled rectangle
 * ili9341_draw_char(x, y, c, color, bg)   - Draw a single character
 * ili9341_draw_string(x, y, str, color, bg) - Draw a string of text
 ***************************************************************
 */

#ifndef ILI9341_H
#define ILI9341_H

#include "processor_hal.h"
#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Pin definitions — ILI9341 wired to SPI1 Arduino header            */
/* ------------------------------------------------------------------ */
#define ILI9341_SPI             SPI1

/* SCK  → PA5  (AF5) */
#define ILI9341_SCK_PIN         GPIO_PIN_5
#define ILI9341_SCK_PORT        GPIOA
#define ILI9341_SCK_AF          GPIO_AF5_SPI1
#define ILI9341_SCK_CLK_EN()    __HAL_RCC_GPIOA_CLK_ENABLE()

/* MOSI → PA7  (AF5) */
#define ILI9341_MOSI_PIN        GPIO_PIN_7
#define ILI9341_MOSI_PORT       GPIOA
#define ILI9341_MOSI_AF         GPIO_AF5_SPI1
#define ILI9341_MOSI_CLK_EN()   __HAL_RCC_GPIOA_CLK_ENABLE()

/* MISO → PA6  (AF5, optional) */
#define ILI9341_MISO_PIN        GPIO_PIN_6
#define ILI9341_MISO_PORT       GPIOA
#define ILI9341_MISO_AF         GPIO_AF5_SPI1
#define ILI9341_MISO_CLK_EN()   __HAL_RCC_GPIOA_CLK_ENABLE()

/* CS   → PB6  (GPIO output) */
#define ILI9341_CS_PIN          GPIO_PIN_6
#define ILI9341_CS_PORT         GPIOB
#define ILI9341_CS_CLK_EN()     __HAL_RCC_GPIOB_CLK_ENABLE()

/* RESET → PA9 (GPIO output) */
#define ILI9341_RST_PIN         GPIO_PIN_9
#define ILI9341_RST_PORT        GPIOA
#define ILI9341_RST_CLK_EN()    __HAL_RCC_GPIOA_CLK_ENABLE()

/* DC   → PC7  (GPIO output, 0=command, 1=data) */
#define ILI9341_DC_PIN          GPIO_PIN_7
#define ILI9341_DC_PORT         GPIOC
#define ILI9341_DC_CLK_EN()     __HAL_RCC_GPIOC_CLK_ENABLE()

/* ------------------------------------------------------------------ */
/*  Convenience macros                                                 */
/* ------------------------------------------------------------------ */
#define ILI9341_CS_LOW()    HAL_GPIO_WritePin(ILI9341_CS_PORT,  ILI9341_CS_PIN,  GPIO_PIN_RESET)
#define ILI9341_CS_HIGH()   HAL_GPIO_WritePin(ILI9341_CS_PORT,  ILI9341_CS_PIN,  GPIO_PIN_SET)
#define ILI9341_DC_CMD()    HAL_GPIO_WritePin(ILI9341_DC_PORT,  ILI9341_DC_PIN,  GPIO_PIN_RESET)
#define ILI9341_DC_DATA()   HAL_GPIO_WritePin(ILI9341_DC_PORT,  ILI9341_DC_PIN,  GPIO_PIN_SET)
#define ILI9341_RST_LOW()   HAL_GPIO_WritePin(ILI9341_RST_PORT, ILI9341_RST_PIN, GPIO_PIN_RESET)
#define ILI9341_RST_HIGH()  HAL_GPIO_WritePin(ILI9341_RST_PORT, ILI9341_RST_PIN, GPIO_PIN_SET)

/* ------------------------------------------------------------------ */
/*  Display dimensions                                                 */
/* ------------------------------------------------------------------ */
#define ILI9341_WIDTH   240
#define ILI9341_HEIGHT  320

/* ------------------------------------------------------------------ */
/*  Common 16-bit (RGB565) colour helpers                              */
/* ------------------------------------------------------------------ */
#define ILI9341_BLACK   0x0000
#define ILI9341_WHITE   0xFFFF
#define ILI9341_RED     0xF800
#define ILI9341_GREEN   0x07E0
#define ILI9341_BLUE    0x001F
#define ILI9341_CYAN    0x07FF
#define ILI9341_MAGENTA 0xF81F
#define ILI9341_YELLOW  0xFFE0

/** Pack 8-bit R, G, B components into a 16-bit RGB565 value. */
#define ILI9341_COLOR(r, g, b) \
    ((uint16_t)(((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3))

/* ------------------------------------------------------------------ */
/*  ILI9341 command codes (subset used by this driver)                */
/* ------------------------------------------------------------------ */
#define ILI9341_CMD_SWRESET     0x01
#define ILI9341_CMD_SLPOUT      0x11
#define ILI9341_CMD_DISPON      0x29
#define ILI9341_CMD_CASET       0x2A
#define ILI9341_CMD_PASET       0x2B
#define ILI9341_CMD_RAMWR       0x2C
#define ILI9341_CMD_MADCTL      0x36
#define ILI9341_CMD_COLMOD      0x3A
#define ILI9341_CMD_FRMCTR1     0xB1
#define ILI9341_CMD_DFUNCTR     0xB6
#define ILI9341_CMD_PWCTR1      0xC0
#define ILI9341_CMD_PWCTR2      0xC1
#define ILI9341_CMD_VMCTR1      0xC5
#define ILI9341_CMD_VMCTR2      0xC7
#define ILI9341_CMD_GMMACP      0xE0
#define ILI9341_CMD_GMMANP      0xE1

// Framebuffer dimensions (adjust RES_SCALE as needed)
#define FB_WIDTH   (ILI9341_WIDTH  / RES_SCALE)
#define FB_HEIGHT  (ILI9341_HEIGHT / RES_SCALE)


/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief  Initialise the ILI9341: clocks, GPIO, SPI, and display sequence.
 *         Must be called once before any drawing function.
 */
void ili9341_init(void);

/**
 * @brief  Draw a filled rectangle.
 *
 * @param  x      Top-left column (0 … ILI9341_WIDTH-1)
 * @param  y      Top-left row    (0 … ILI9341_HEIGHT-1)
 * @param  width  Width  in pixels
 * @param  height Height in pixels
 * @param  color  16-bit RGB565 fill colour (e.g. ILI9341_RED)
 */
void ili9341_draw_filled_rect(uint16_t x, uint16_t y,
                               uint16_t width, uint16_t height,
                               uint16_t color);

/**
 * @brief  Draw a single ASCII character using the built-in 5×7 font.
 *
 * @param  x      Top-left column of the character cell
 * @param  y      Top-left row    of the character cell
 * @param  c      ASCII character to draw
 * @param  color  Foreground colour (RGB565)
 * @param  bg     Background colour (RGB565)
 */
void ili9341_draw_char(uint16_t x, uint16_t y, char c,
                        uint16_t color, uint16_t bg);

/**
 * @brief  Draw a null-terminated ASCII string.
 *         Characters are placed left-to-right; no automatic line-wrap.
 *
 * @param  x      Starting column
 * @param  y      Starting row
 * @param  str    Null-terminated string
 * @param  color  Foreground colour (RGB565)
 * @param  bg     Background colour (RGB565)
 */
void ili9341_draw_string(uint16_t x, uint16_t y, const char *str,
                          uint16_t color, uint16_t bg);
void ili9341_fill_dma(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);


// The framebuffer — define in ili9341.c, extern here
extern uint16_t ili9341_fb[FB_HEIGHT][FB_WIDTH];

// Flush the entire framebuffer to the display
void ili9341_flush(void);
#endif /* ILI9341_H */
