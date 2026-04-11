/**
 **************************************************************
 * File: mylib/ili9341.h
 * Brief: ILI9341 TFT LCD driver — STM32 NUCLEO-F429ZI
 *        SPI1 @ 45 MHz  (PA5=SCK PA7=MOSI PA6=MISO)
 *        CS=PB6  RESET=PA9  DC=PC7
 *
 * Speed analysis
 * --------------
 * A full 240x320 screen is 76,800 pixels x 16 bits = 1,228,800 bits.
 * At 45 MHz that takes ~27 ms — this is the physical wire limit for
 * SPI to this display, and no software technique can beat it.
 *
 * Double-buffering the fill makes no difference: DMA setup overhead
 * per row is ~111 ns vs ~171 us of actual transfer time (0.065%).
 *
 * What actually helps:
 *
 *  1. SPI at 45 MHz (BR=0, fPCLK/2) instead of 22.5 MHz — 2x faster.
 *     Still within the ILI9341's 66 MHz write limit.
 *
 *  2. 2-launch DMA fill instead of 320 per-row launches.
 *     MINC=0 lets the DMA repeat a single colour word without any
 *     buffer, eliminating all row-boundary stalls.
 *
 *  3. Async per-column DMA for raycasting.
 *     A raycasting engine renders one vertical column at a time.
 *     With ili9341_column_start_dma() the CPU computes column N+1
 *     while DMA is sending column N — the two overlap completely
 *     because raytracing (~5000 cycles) is faster than a 320-pixel
 *     DMA (~20,000 SPI cycles).  This makes rendering effectively
 *     free in terms of display time.
 *
 * Typical frame time with async columns:
 *   240 columns x 113 us/column (DMA limited) = ~27 ms => ~37 fps
 *   But CPU time is hidden, so you get the most out of every clock.
 ***************************************************************
 * EXTERNAL FUNCTIONS
 ***************************************************************
 * ili9341_init()
 * ili9341_fill_screen(color)            -- blocks ~27ms (wire limit)
 * ili9341_draw_filled_rect(x,y,w,h,c)
 * ili9341_draw_char(x,y,c,fg,bg)
 * ili9341_draw_string(x,y,str,fg,bg)
 *
 * -- Async column API for raycasting --
 * ili9341_column_wait()                 -- wait for previous DMA done
 * ili9341_column_start_dma(x, buf)      -- kick off column DMA, return immediately
 *   buf must be uint16_t[ILI9341_HEIGHT] in SRAM (not CCM)
 ***************************************************************
 */

#ifndef ILI9341_H
#define ILI9341_H

#include "processor_hal.h"
#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Pins                                                               */
/* ------------------------------------------------------------------ */
#define ILI9341_SPI              SPI1
#define ILI9341_SCK_PIN          GPIO_PIN_5      /* PA5 */
#define ILI9341_MOSI_PIN         GPIO_PIN_7      /* PA7 */
#define ILI9341_MISO_PIN         GPIO_PIN_6      /* PA6 */
#define ILI9341_SPI_AF           GPIO_AF5_SPI1

#define ILI9341_CS_PIN           GPIO_PIN_6      /* PB6 */
#define ILI9341_CS_PORT          GPIOB

#define ILI9341_RST_PIN          GPIO_PIN_9      /* PA9 */
#define ILI9341_RST_PORT         GPIOA

#define ILI9341_DC_PIN           GPIO_PIN_7      /* PC7 */
#define ILI9341_DC_PORT          GPIOC

#define ILI9341_CS_LOW()   HAL_GPIO_WritePin(ILI9341_CS_PORT,  ILI9341_CS_PIN,  GPIO_PIN_RESET)
#define ILI9341_CS_HIGH()  HAL_GPIO_WritePin(ILI9341_CS_PORT,  ILI9341_CS_PIN,  GPIO_PIN_SET)
#define ILI9341_DC_CMD()   HAL_GPIO_WritePin(ILI9341_DC_PORT,  ILI9341_DC_PIN,  GPIO_PIN_RESET)
#define ILI9341_DC_DATA()  HAL_GPIO_WritePin(ILI9341_DC_PORT,  ILI9341_DC_PIN,  GPIO_PIN_SET)
#define ILI9341_RST_LOW()  HAL_GPIO_WritePin(ILI9341_RST_PORT, ILI9341_RST_PIN, GPIO_PIN_RESET)
#define ILI9341_RST_HIGH() HAL_GPIO_WritePin(ILI9341_RST_PORT, ILI9341_RST_PIN, GPIO_PIN_SET)

/* ------------------------------------------------------------------ */
/*  Display                                                            */
/* ------------------------------------------------------------------ */
#define ILI9341_WIDTH    240u
#define ILI9341_HEIGHT   320u

/* ------------------------------------------------------------------ */
/*  Colours (RGB565)                                                   */
/* ------------------------------------------------------------------ */
#define ILI9341_BLACK    0x0000u
#define ILI9341_WHITE    0xFFFFu
#define ILI9341_RED      0xF800u
#define ILI9341_GREEN    0x07E0u
#define ILI9341_BLUE     0x001Fu
#define ILI9341_CYAN     0x07FFu
#define ILI9341_MAGENTA  0xF81Fu
#define ILI9341_YELLOW   0xFFE0u
#define ILI9341_COLOR(r,g,b) \
    ((uint16_t)((((r)&0xF8u)<<8u)|(((g)&0xFCu)<<3u)|(((b)&0xF8u)>>3u)))

/* ------------------------------------------------------------------ */
/*  ILI9341 commands                                                   */
/* ------------------------------------------------------------------ */
#define ILI9341_CMD_SWRESET 0x01u
#define ILI9341_CMD_SLPOUT  0x11u
#define ILI9341_CMD_DISPON  0x29u
#define ILI9341_CMD_CASET   0x2Au
#define ILI9341_CMD_PASET   0x2Bu
#define ILI9341_CMD_RAMWR   0x2Cu
#define ILI9341_CMD_MADCTL  0x36u
#define ILI9341_CMD_COLMOD  0x3Au
#define ILI9341_CMD_FRMCTR1 0xB1u
#define ILI9341_CMD_DFUNCTR 0xB6u
#define ILI9341_CMD_PWCTR1  0xC0u
#define ILI9341_CMD_PWCTR2  0xC1u
#define ILI9341_CMD_VMCTR1  0xC5u
#define ILI9341_CMD_VMCTR2  0xC7u
#define ILI9341_CMD_GMMACP  0xE0u
#define ILI9341_CMD_GMMANP  0xE1u

/* ------------------------------------------------------------------ */
/*  DMA — SPI1_TX = DMA2 Stream3 Channel3 (F429 RM Table 43)         */
/*  Stream 3 flags are in the LOW registers (streams 0-3)             */
/* ------------------------------------------------------------------ */
#define ILI9341_DMA          DMA2
#define ILI9341_DMA_STREAM   DMA2_Stream3
#define ILI9341_DMA_CHANNEL  (3u << DMA_SxCR_CHSEL_Pos)
#define ILI9341_DMA_TCIF     DMA_LISR_TCIF3
#define ILI9341_DMA_CLR      (DMA_LIFCR_CTCIF3 | DMA_LIFCR_CHTIF3 | \
                               DMA_LIFCR_CTEIF3 | DMA_LIFCR_CDMEIF3 | \
                               DMA_LIFCR_CFEIF3)

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

void ili9341_init(void);

/** Fill entire screen. Blocks for ~27 ms (wire-speed limit at 45 MHz). */
void ili9341_fill_screen(uint16_t color);

void ili9341_draw_filled_rect(uint16_t x, uint16_t y,
                               uint16_t width, uint16_t height,
                               uint16_t color);

void ili9341_draw_char(uint16_t x, uint16_t y, char c,
                        uint16_t color, uint16_t bg);

void ili9341_draw_string(uint16_t x, uint16_t y, const char *str,
                          uint16_t color, uint16_t bg);

/* ------------------------------------------------------------------ */
/*  Async column API — use this in your raycasting render loop        */
/*                                                                     */
/*  Usage pattern:                                                     */
/*                                                                     */
/*    uint16_t col_a[ILI9341_HEIGHT];  // in SRAM, not CCM            */
/*    uint16_t col_b[ILI9341_HEIGHT];                                  */
/*    uint16_t *draw = col_a, *next = col_b;                          */
/*                                                                     */
/*    ili9341_column_wait();           // ensure no DMA in flight      */
/*    for (uint16_t x = 0; x < ILI9341_WIDTH; x++) {                  */
/*        raytrace_column(x, next);   // CPU fills next buffer         */
/*        ili9341_column_wait();      // wait for previous DMA         */
/*        ili9341_column_start_dma(x, next); // fire DMA, return now  */
/*        uint16_t *tmp = draw; draw = next; next = tmp; // swap       */
/*    }                                                                */
/*    ili9341_column_wait();           // wait for last column         */
/* ------------------------------------------------------------------ */

/**
 * Block until any in-flight column DMA has finished.
 * Safe to call even when no DMA is running.
 */
void ili9341_column_wait(void);

/**
 * Set the address window for column x and start a DMA transfer of
 * ILI9341_HEIGHT pixels from buf[] to the display.
 * Returns immediately — call ili9341_column_wait() before reusing buf.
 *
 * @param x    Column index (0 … ILI9341_WIDTH-1)
 * @param buf  uint16_t[ILI9341_HEIGHT] in SRAM (NOT CCM at 0x10000000)
 *             Values must be byte-swapped RGB565 (use ILI9341_SWAP macro).
 */
void ili9341_column_start_dma(uint16_t x, const uint16_t *buf);

/**
 * Byte-swap a RGB565 colour for direct use in a column buffer.
 * SPI shifts MSB first; the F4 is little-endian, so the high byte
 * must be sent first.
 */
#define ILI9341_SWAP(color) ((uint16_t)(((color) << 8) | ((color) >> 8)))

#endif /* ILI9341_H */
